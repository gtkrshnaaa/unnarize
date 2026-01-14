#include "ucore_uon.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

// Schema storage only - we don't store data anymore
static Map* uonSchemas = NULL; // TableName -> StructDef

// Cursor structure for streaming
typedef struct {
    FILE* file;
    char* tableName;
    long startOffset; // Where the implementation loop starts (after leading [)
} UonCursor;

static void cursorCleanup(void* data);

static void ensureInit(VM* vm) {
    if (!uonSchemas) uonSchemas = newMap(vm);
}

// ---- Parser Utils ----

static char peek(const char* p) { return *p; }
static void skipSpace(const char** p) {
    while (peek(*p) && isspace(peek(*p))) (*p)++;
}
// File-based peek/skip
static int fpeek(FILE* f) {
    int c = fgetc(f);
    ungetc(c, f);
    return c;
}
static void fskipSpace(FILE* f) {
    int c;
    while ((c = fgetc(f)) != EOF) {
        if (!isspace(c)) {
            ungetc(c, f);
            return;
        }
    }
}


// Helper to read identifier from file
static char* fparseIdentifier(FILE* f) {
    fskipSpace(f);
    char buf[256];
    int i = 0;
    int c;
    while ((c = fgetc(f)) != EOF) {
        if (isalnum(c) || c == '_') {
            if (i < 255) buf[i++] = c;
        } else {
            ungetc(c, f);
            break;
        }
    }
    buf[i] = '\0';
    if (i == 0) return NULL;
    return strdup(buf);
}

// Re-use string parser logic for schema parsing (loaded in RAM still, as schema is small)
static char* parseIdentifier(const char** p) {
    skipSpace(p);
    const char* start = *p;
    while (isalnum(peek(*p)) || peek(*p) == '_') (*p)++;
    int len = *p - start;
    if (len == 0) return NULL;
    return strndup(start, len);
}

static void parseSchemaBlock(VM* vm, const char** p) {
    skipSpace(p);
    if (peek(*p) != '{') return;
    (*p)++;
    
    while (peek(*p) && peek(*p) != '}') {
        skipSpace(p);
        if (peek(*p) == '}') break;
        
        char* tableName = parseIdentifier(p);
        if (!tableName) break;
        
        skipSpace(p);
        if (peek(*p) == ':') (*p)++;
        
        skipSpace(p);
        if (peek(*p) == '[') (*p)++;
        
        StructDef* def = ALLOCATE_OBJ(vm, StructDef, OBJ_STRUCT_DEF);
        def->name = tableName;
        def->fields = malloc(16 * sizeof(char*));
        def->fieldCount = 0;
        int cap = 16;
        
        while (peek(*p) && peek(*p) != ']') {
            skipSpace(p);
            char* col = parseIdentifier(p);
            if (col) {
                skipSpace(p); // ignore > relation
                if (peek(*p) == '>') {
                    while (peek(*p) && peek(*p) != ',' && peek(*p) != ']') (*p)++;
                }
                
                if (def->fieldCount >= cap) {
                    cap *= 2;
                    def->fields = realloc(def->fields, cap * sizeof(char*));
                }
                def->fields[def->fieldCount++] = col;
            }
            skipSpace(p);
            if (peek(*p) == ',') (*p)++;
        }
        if (peek(*p) == ']') (*p)++;
        
        Value vDef = OBJ_VAL(def);
        mapSetStr(uonSchemas, tableName, (int)strlen(tableName), vDef);
        
        skipSpace(p);
        if (peek(*p) == ',') (*p)++;
    }
    if (peek(*p) == '}') (*p)++;
}

static void parseFromSource(VM* vm, const char* source) {
    ensureInit(vm);
    const char* p = source;
    while (peek(p)) {
        skipSpace(&p);
        if (strncmp(p, "@schema", 7) == 0) {
            p += 7;
            parseSchemaBlock(vm, &p);
        } else if (strncmp(p, "@flow", 5) == 0) {
            // Stop parsing flow in RAM
            break;
        } else {
            if (peek(p)) p++;
        }
    }
}

// --------------------------------------------------------
// API
// --------------------------------------------------------

static Value uon_parse(VM* vm, Value* args, int argCount) {
    if (argCount != 1 || !IS_STRING(args[0])) return INT_VAL(0);
    parseFromSource(vm, AS_CSTRING(args[0]));
    return BOOL_VAL(true);
}

// Globals
static char g_lastPath[1024] = {0};

static Value uon_load_impl(VM* vm, Value* args, int argCount) {
    if (argCount < 1 || !IS_STRING(args[0])) return BOOL_VAL(false);
    char* path = AS_CSTRING(args[0]);
    strncpy(g_lastPath, path, 1023);
    
    // Schema parse limited logic
    FILE* f = fopen(path, "r");
    if(!f) return BOOL_VAL(false);
    
    size_t cap = 1024 * 64; // Limit schema read to 64KB
    size_t len = 0;
    char* buf = malloc(cap + 1);
    int c;
    while((c = fgetc(f)) != EOF && len < cap) {
        buf[len++] = c;
        if(len >= 5 && strncmp(&buf[len-5], "@flow", 5) == 0) break;
    }
    buf[len] = '\0';
    fclose(f);
    parseFromSource(vm, buf);
    free(buf);
    
     return BOOL_VAL(true);
}

// Forward decl
static Value fparseValue(VM* vm, FILE* f);

static Value uon_next_impl(VM* vm, Value* args, int argCount) {
    if (argCount != 1 || !IS_OBJ(args[0]) || AS_OBJ(args[0])->type != OBJ_RESOURCE) {
        return NIL_VAL;
    }
    
    ObjResource* res = (ObjResource*)AS_OBJ(args[0]);
    UonCursor* cursor = (UonCursor*)res->data;
    FILE* f = cursor->file;
    if (!f) return NIL_VAL; // already closed?
    
    fskipSpace(f);
    int c = fpeek(f);
    if (c == ']') return NIL_VAL; // End of list
    
    // Expect {
    if (c == ',') { fgetc(f); fskipSpace(f); c = fpeek(f); }
    if (c == ']') return NIL_VAL;
    
    if (c != '{') return NIL_VAL; // Not a valid record start
    fgetc(f); // eat {
    
    // Parse fields
    Map* m = newMap(vm);
    
    while(1) {
        fskipSpace(f);
        if (fpeek(f) == '}') { fgetc(f); break; }
        
        char* key = fparseIdentifier(f);
        if (!key) break;
        
        fskipSpace(f);
        if (fgetc(f) == ':') {
             Value val = fparseValue(vm, f);
             mapSetStr(m, key, (int)strlen(key), val);
        }
        free(key);
        
        fskipSpace(f);
        if (fpeek(f) == ',') fgetc(f);
    }
    
    Value vRet = OBJ_VAL(m);
    return vRet;
}


static Value uon_get_impl(VM* vm, Value* args, int argCount) {
    char* tableName = NULL;
    char* path = g_lastPath;
    
    if (argCount == 1) {
        if (!IS_STRING(args[0])) return INT_VAL(0);
        tableName = AS_CSTRING(args[0]);
    } else if (argCount == 2) {
        if (!IS_STRING(args[0]) || !IS_STRING(args[1])) return INT_VAL(0);
        path = AS_CSTRING(args[0]);
        tableName = AS_CSTRING(args[1]);
    } else return INT_VAL(0);
    
    FILE* f = fopen(path, "r");
    if (!f) return INT_VAL(0);
    
    // Fast-forward to @flow
    fseek(f, 0, SEEK_SET);
    
    // Brute force @flow scan
    int matched = 0;
    bool inFlow = false;
    while(true) {
        int c = fgetc(f);
        if (c == EOF) break;
        if (c == "@flow"[matched]) matched++;
        else matched = 0;
        if (matched == 5) { inFlow = true; break; }
    }
    
    if (!inFlow) { fclose(f); return INT_VAL(0); }
    
    // Find "tableName:"
    fskipSpace(f);
    if (fgetc(f) != '{') { fclose(f); return INT_VAL(0); }
    
    bool foundTable = false;
    while(true) {
        fskipSpace(f);
        int c = fpeek(f);
        if (c == '}' || c == EOF) break;
        
        char* id = fparseIdentifier(f);
        if (!id) break;
        
        if (strcmp(id, tableName) == 0) {
            free(id);
            foundTable = true;
            break;
        }
        free(id);
        
        // Skip value
        fskipSpace(f);
        if (fgetc(f) == ':') {
             fskipSpace(f);
             if (fgetc(f) == '[') {
                 // Skip content until closing ]
                 int depth = 1;
                 while(depth > 0 && (c=fgetc(f)) != EOF) {
                     if(c=='[') depth++;
                     if(c==']') depth--;
                 }
             }
             fskipSpace(f);
             if (fpeek(f) == ',') fgetc(f);
        }
    }
    
    if (!foundTable) { fclose(f); return INT_VAL(0); }
    
    fskipSpace(f);
    if (fgetc(f) != ':') { fclose(f); return INT_VAL(0); }
    fskipSpace(f);
    if (fgetc(f) != '[') { fclose(f); return INT_VAL(0); }
    
    // Created Cursor!
    ObjResource* res = ALLOCATE_OBJ(vm, ObjResource, OBJ_RESOURCE);
    UonCursor* cursor = malloc(sizeof(UonCursor));
    cursor->file = f;
    cursor->tableName = strdup(tableName);
    cursor->startOffset = ftell(f);
    
    res->data = cursor;
    res->cleanup = (ResourceCleanupFn)cursorCleanup;
    
    Value v = OBJ_VAL(res);
    return v;
}

static void cursorCleanup(void* data) {
    UonCursor* cursor = (UonCursor*)data;
    if (cursor) {
        if (cursor->file) fclose(cursor->file);
        if (cursor->tableName) free(cursor->tableName);
        free(cursor);
    }
}

static Value fparseValue(VM* vm, FILE* f) { 
    fskipSpace(f);
    int c = fpeek(f);
    
    if (c == '"') {
        fgetc(f);
        char buf[4096]; int i=0; // increased buffer
        while((c=fgetc(f)) != EOF && c != '"' && i < 4095) buf[i++] = c;
        buf[i] = '\0';
        ObjString* s = internString(vm, buf, i);
        return OBJ_VAL(s);
    }
    
    if (isdigit(c) || c == '-') {
        char buf[64]; int i=0;
        while((c=fgetc(f)) != EOF && (isdigit(c) || c == '.' || c=='-') && i < 63) buf[i++] = c;
        buf[i] = '\0';
        ungetc(c, f);
        if (strchr(buf, '.')) {
            return FLOAT_VAL(atof(buf));
        } else {
            return INT_VAL(atoi(buf));
        }
    }
    
    // bool/null/ident
    char* id = fparseIdentifier(f);
    if (id) {
         Value v = NIL_VAL;
         if (strcmp(id, "true")==0) { v = BOOL_VAL(true); }
         else if (strcmp(id, "false")==0) { v = BOOL_VAL(false); }
         else if (strcmp(id, "null")==0) { v = NIL_VAL; }
         else {
             ObjString* s = internString(vm, id, (int)strlen(id));
             v = OBJ_VAL(s); 
         }
         free(id);
         return v;
    }
    return NIL_VAL;
}

// ---- UON Generator ----
// Helper to append string to buffer
static void uon_buf_append(char** buf, int* len, int* cap, const char* str) {
    int l = (int)strlen(str);
    if (*len + l >= *cap) {
        *cap = *cap * 2 + l + 1024;
        *buf = realloc(*buf, *cap);
    }
    strcpy(*buf + *len, str);
    *len += l;
}

// Serialize value for UON format
static void uon_serialize_val(Value v, char** buf, int* len, int* cap) {
    char temp[64];
    if (IS_NIL(v)) {
        uon_buf_append(buf, len, cap, "null");
        return;
    }
    
    switch (getValueType(v)) {
        case VAL_INT:
            snprintf(temp, sizeof(temp), "%lld", (long long)AS_INT(v));
            uon_buf_append(buf, len, cap, temp);
            break;
        case VAL_FLOAT:
            snprintf(temp, sizeof(temp), "%.6g", AS_FLOAT(v));
            uon_buf_append(buf, len, cap, temp);
            break;
        case VAL_BOOL:
            uon_buf_append(buf, len, cap, AS_BOOL(v) ? "true" : "false");
            break;
        case VAL_OBJ: {
            Obj* o = AS_OBJ(v);
            if (o->type == OBJ_STRING) {
                uon_buf_append(buf, len, cap, "\"");
                uon_buf_append(buf, len, cap, ((ObjString*)o)->chars);
                uon_buf_append(buf, len, cap, "\"");
            } else {
                uon_buf_append(buf, len, cap, "null");
            }
            break;
        }
        default:
            uon_buf_append(buf, len, cap, "null");
            break;
    }
}

// ucoreUon.generate(schemaMap, dataMap) -> string
// schemaMap: { tableName: ["field1", "field2"], ... }
// dataMap: { tableName: [{ field1: val, ... }, ...], ... }
static Value uon_generate(VM* vm, Value* args, int argCount) {
    if (argCount != 2 || !IS_OBJ(args[0]) || !IS_OBJ(args[1])) {
        printf("Error: ucoreUon.generate(schema, data) expects 2 map arguments.\n");
        ObjString* empty = internString(vm, "", 0);
        return OBJ_VAL(empty);
    }
    
    Obj* schemaObj = AS_OBJ(args[0]);
    Obj* dataObj = AS_OBJ(args[1]);
    
    if (schemaObj->type != OBJ_MAP || dataObj->type != OBJ_MAP) {
        printf("Error: ucoreUon.generate expects map arguments.\n");
        ObjString* empty = internString(vm, "", 0);
        return OBJ_VAL(empty);
    }
    
    Map* schema = (Map*)schemaObj;
    Map* data = (Map*)dataObj;
    
    int cap = 4096;
    int len = 0;
    char* buf = malloc(cap);
    buf[0] = '\0';
    
    // Generate @schema block
    uon_buf_append(&buf, &len, &cap, "@schema {\n");
    
    bool firstTable = true;
    for (int i = 0; i < TABLE_SIZE; i++) {
        MapEntry* e = schema->buckets[i];
        while (e) {
            if (!firstTable) uon_buf_append(&buf, &len, &cap, ",\n");
            firstTable = false;
            
            uon_buf_append(&buf, &len, &cap, "    ");
            uon_buf_append(&buf, &len, &cap, e->key);
            uon_buf_append(&buf, &len, &cap, ": [");
            
            // Schema value should be an array of field names
            if (IS_OBJ(e->value) && AS_OBJ(e->value)->type == OBJ_ARRAY) {
                Array* fields = (Array*)AS_OBJ(e->value);
                for (int j = 0; j < fields->count; j++) {
                    if (j > 0) uon_buf_append(&buf, &len, &cap, ", ");
                    if (IS_STRING(fields->items[j])) {
                        uon_buf_append(&buf, &len, &cap, AS_CSTRING(fields->items[j]));
                    }
                }
            }
            uon_buf_append(&buf, &len, &cap, "]");
            e = e->next;
        }
    }
    uon_buf_append(&buf, &len, &cap, "\n}\n\n");
    
    // Generate @flow block
    uon_buf_append(&buf, &len, &cap, "@flow {\n");
    
    firstTable = true;
    for (int i = 0; i < TABLE_SIZE; i++) {
        MapEntry* e = data->buckets[i];
        while (e) {
            if (!firstTable) uon_buf_append(&buf, &len, &cap, ",\n");
            firstTable = false;
            
            uon_buf_append(&buf, &len, &cap, "    ");
            uon_buf_append(&buf, &len, &cap, e->key);
            uon_buf_append(&buf, &len, &cap, ": [\n");
            
            // Data value should be an array of maps (records)
            if (IS_OBJ(e->value) && AS_OBJ(e->value)->type == OBJ_ARRAY) {
                Array* records = (Array*)AS_OBJ(e->value);
                for (int j = 0; j < records->count; j++) {
                    if (j > 0) uon_buf_append(&buf, &len, &cap, ",\n");
                    uon_buf_append(&buf, &len, &cap, "        { ");
                    
                    if (IS_OBJ(records->items[j]) && AS_OBJ(records->items[j])->type == OBJ_MAP) {
                        Map* record = (Map*)AS_OBJ(records->items[j]);
                        bool firstField = true;
                        for (int k = 0; k < TABLE_SIZE; k++) {
                            MapEntry* f = record->buckets[k];
                            while (f) {
                                if (!firstField) uon_buf_append(&buf, &len, &cap, ", ");
                                firstField = false;
                                uon_buf_append(&buf, &len, &cap, f->key);
                                uon_buf_append(&buf, &len, &cap, ": ");
                                uon_serialize_val(f->value, &buf, &len, &cap);
                                f = f->next;
                            }
                        }
                    }
                    uon_buf_append(&buf, &len, &cap, " }");
                }
            }
            uon_buf_append(&buf, &len, &cap, "\n    ]");
            e = e->next;
        }
    }
    uon_buf_append(&buf, &len, &cap, "\n}\n");
    
    ObjString* result = internString(vm, buf, len);
    free(buf);
    return OBJ_VAL(result);
}

static Value uon_save_dummy(VM* vm, Value* args, int argCount) {
    (void)vm; (void)args; (void)argCount;
    printf("Warning: ucoreUon.save() disabled in lazy mode (read-only optimization).\n");
    return BOOL_VAL(true);
}

static Value uon_noop(VM* vm, Value* args, int argCount) {
    (void)vm; (void)args; (void)argCount;
    return BOOL_VAL(false);
}

void registerUCoreUON(VM* vm) {
    ObjString* modNameObj = internString(vm, "ucoreUon", 8);
    char* modName = modNameObj->chars; 
    
    Module* mod = ALLOCATE_OBJ(vm, Module, OBJ_MODULE);
    mod->name = strdup(modName); 
    mod->obj.isMarked = true; // PERMANENT ROOT
    
    // CRITICAL: Use ALLOCATE_OBJ for Environment to enable GC tracking
    Environment* modEnv = ALLOCATE_OBJ(vm, Environment, OBJ_ENVIRONMENT);
    memset(modEnv->buckets, 0, sizeof(modEnv->buckets));
    memset(modEnv->funcBuckets, 0, sizeof(modEnv->funcBuckets));
    modEnv->enclosing = NULL;
    modEnv->obj.isMarked = true; // PERMANENT ROOT
    mod->env = modEnv;

    defineNative(vm, mod->env, "parse", uon_parse, 1);
    defineNative(vm, mod->env, "load", uon_load_impl, 1);
    defineNative(vm, mod->env, "get", uon_get_impl, 1);
    defineNative(vm, mod->env, "next", uon_next_impl, 1);
    defineNative(vm, mod->env, "generate", uon_generate, 2);
    defineNative(vm, mod->env, "save", uon_save_dummy, 1);
    defineNative(vm, mod->env, "insert", uon_noop, 2);

    Value vMod = OBJ_VAL(mod);
    defineGlobal(vm, "ucoreUon", vMod);
}
