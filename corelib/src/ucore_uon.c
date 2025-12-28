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
        
        Value vDef; vDef.type = VAL_OBJ; vDef.obj = (Obj*)def;
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
    if (argCount != 1 || !IS_STRING(args[0])) return (Value){VAL_INT, .intVal = 0};
    parseFromSource(vm, AS_CSTRING(args[0]));
    return (Value){VAL_BOOL, .boolVal = true};
}

// Globals
static char g_lastPath[1024] = {0};

static Value uon_load_impl(VM* vm, Value* args, int argCount) {
    if (argCount < 1 || !IS_STRING(args[0])) return (Value){VAL_BOOL, .boolVal=false};
    char* path = AS_CSTRING(args[0]);
    strncpy(g_lastPath, path, 1023);
    
    // Schema parse limited logic
    FILE* f = fopen(path, "r");
    if(!f) return (Value){VAL_BOOL, .boolVal=false};
    
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
    
    return (Value){VAL_BOOL, .boolVal=true};
}

// Forward decl
static Value fparseValue(VM* vm, FILE* f);

static Value uon_next_impl(VM* vm, Value* args, int argCount) {
    if (argCount != 1 || !IS_OBJ(args[0]) || AS_OBJ(args[0])->type != OBJ_RESOURCE) {
        return (Value){VAL_NIL, .intVal=0};
    }
    
    ObjResource* res = (ObjResource*)AS_OBJ(args[0]);
    UonCursor* cursor = (UonCursor*)res->data;
    FILE* f = cursor->file;
    if (!f) return (Value){VAL_NIL, .intVal=0}; // already closed?
    
    fskipSpace(f);
    int c = fpeek(f);
    if (c == ']') return (Value){VAL_NIL, .intVal=0}; // End of list
    
    // Expect {
    if (c == ',') { fgetc(f); fskipSpace(f); c = fpeek(f); }
    if (c == ']') return (Value){VAL_NIL, .intVal=0};
    
    if (c != '{') {
         // Error or end
         return (Value){VAL_NIL, .intVal=0}; 
    }
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
    
    Value vRet; vRet.type = VAL_OBJ; vRet.obj = (Obj*)m;
    return vRet;
}


static Value uon_get_impl(VM* vm, Value* args, int argCount) {
    char* tableName = NULL;
    char* path = g_lastPath;
    
    if (argCount == 1) {
        if (!IS_STRING(args[0])) return (Value){VAL_INT, .intVal=0};
        tableName = AS_CSTRING(args[0]);
    } else if (argCount == 2) {
        if (!IS_STRING(args[0]) || !IS_STRING(args[1])) return (Value){VAL_INT, .intVal=0};
        path = AS_CSTRING(args[0]);
        tableName = AS_CSTRING(args[1]);
    } else return (Value){VAL_INT, .intVal=0};
    
    FILE* f = fopen(path, "r");
    if (!f) return (Value){VAL_INT, .intVal=0};
    
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
    
    if (!inFlow) { fclose(f); return (Value){VAL_INT, .intVal=0}; }
    
    // Find "tableName:"
    fskipSpace(f);
    if (fgetc(f) != '{') { fclose(f); return (Value){VAL_INT, .intVal=0}; }
    
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
    
    if (!foundTable) { fclose(f); return (Value){VAL_INT, .intVal=0}; }
    
    fskipSpace(f);
    if (fgetc(f) != ':') { fclose(f); return (Value){VAL_INT, .intVal=0}; }
    fskipSpace(f);
    if (fgetc(f) != '[') { fclose(f); return (Value){VAL_INT, .intVal=0}; }
    
    // Created Cursor!
    ObjResource* res = ALLOCATE_OBJ(vm, ObjResource, OBJ_RESOURCE);
    UonCursor* cursor = malloc(sizeof(UonCursor));
    cursor->file = f;
    cursor->tableName = strdup(tableName);
    cursor->startOffset = ftell(f);
    
    res->data = cursor;
    res->cleanup = (ResourceCleanupFn)cursorCleanup;
    
    Value v; v.type = VAL_OBJ; v.obj = (Obj*)res;
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
        return (Value){VAL_OBJ, .obj=(Obj*)s};
    }
    
    if (isdigit(c) || c == '-') {
        char buf[64]; int i=0;
        while((c=fgetc(f)) != EOF && (isdigit(c) || c == '.' || c=='-') && i < 63) buf[i++] = c;
        buf[i] = '\0';
        ungetc(c, f);
        if (strchr(buf, '.')) {
            return (Value){VAL_FLOAT, .floatVal = atof(buf)};
        } else {
            return (Value){VAL_INT, .intVal = atoi(buf)};
        }
    }
    
    // bool/null/ident
    char* id = fparseIdentifier(f);
    if (id) {
         Value v = {VAL_NIL, .intVal=0};
         if (strcmp(id, "true")==0) { v.type = VAL_BOOL; v.boolVal = true; }
         else if (strcmp(id, "false")==0) { v.type = VAL_BOOL; v.boolVal = false; }
         else if (strcmp(id, "null")==0) { v.type = VAL_NIL; }
         else {
             ObjString* s = internString(vm, id, (int)strlen(id));
             v.type = VAL_OBJ; v.obj = (Obj*)s; 
         }
         free(id);
         return v;
    }
    return (Value){VAL_NIL, .intVal=0};
}


static Value uon_save_dummy(VM* vm, Value* args, int argCount) {
    (void)vm; (void)args; (void)argCount;
    printf("Warning: ucoreUon.save() disabled in lazy mode (read-only optimization).\n");
    return (Value){VAL_BOOL, .boolVal=true};
}

static Value uon_noop(VM* vm, Value* args, int argCount) {
    (void)vm; (void)args; (void)argCount;
    return (Value){VAL_BOOL, .boolVal=false};
}

void registerUCoreUON(VM* vm) {
    ObjString* modNameObj = internString(vm, "ucoreUon", 8);
    char* modName = modNameObj->chars; 
    
    Module* mod = ALLOCATE_OBJ(vm, Module, OBJ_MODULE);
    mod->name = strdup(modName); 
    mod->env = malloc(sizeof(Environment));
    if(!mod->env) error("Alloc fail uon env", 0);
    memset(mod->env->buckets, 0, sizeof(mod->env->buckets));
    memset(mod->env->funcBuckets, 0, sizeof(mod->env->funcBuckets));
    mod->env->enclosing = NULL;

    defineNative(vm, mod->env, "parse", uon_parse, 1);
    defineNative(vm, mod->env, "load", uon_load_impl, 1);
    defineNative(vm, mod->env, "get", uon_get_impl, 1);
    defineNative(vm, mod->env, "next", uon_next_impl, 1);
    defineNative(vm, mod->env, "save", uon_save_dummy, 1);
    defineNative(vm, mod->env, "insert", uon_noop, 2);

    Value vMod; vMod.type = VAL_OBJ; vMod.obj = (Obj*)mod;
    defineGlobal(vm, "ucoreUon", vMod);
}
