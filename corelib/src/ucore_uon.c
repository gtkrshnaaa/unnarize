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

static void ensureInit() {
    if (!uonSchemas) uonSchemas = newMap();
    // uonData removed
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

static void parseSchemaBlock(const char** p) {
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
        
        StructDef* def = malloc(sizeof(StructDef));
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
        
        Value vDef; vDef.type = VAL_STRUCT_DEF; vDef.structDef = def;
        mapSetStr(uonSchemas, tableName, strlen(tableName), vDef);
        
        skipSpace(p);
        if (peek(*p) == ',') (*p)++;
    }
    if (peek(*p) == '}') (*p)++;
}

static void parseFromSource(const char* source) {
    ensureInit();
    const char* p = source;
    while (peek(p)) {
        skipSpace(&p);
        if (strncmp(p, "@schema", 7) == 0) {
            p += 7;
            parseSchemaBlock(&p);
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
    (void)vm;
    if (argCount != 1 || args[0].type != VAL_STRING) return (Value){VAL_INT, .intVal = 0};
    parseFromSource(args[0].stringVal);
    // Note: parsing flow inline (string) is essentially ignored/not useful for streaming.
    // If user provides a full string with flow, we might want to support it, but
    // for now we prioritize FILE streaming.
    return (Value){VAL_BOOL, .boolVal = true};
}

// Globals
static char g_lastPath[1024] = {0};

static Value uon_load_impl(VM* vm, Value* args, int argCount) {
    (void)vm;
    if (argCount < 1 || args[0].type != VAL_STRING) return (Value){VAL_BOOL, .boolVal=false};
    char* path = args[0].stringVal;
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
    parseFromSource(buf);
    free(buf);
    
    return (Value){VAL_BOOL, .boolVal=true};
}

static Value uon_get_impl(VM* vm, Value* args, int argCount) {
    (void)vm;
    char* tableName = NULL;
    char* path = g_lastPath;
    
    if (argCount == 1) {
        tableName = args[0].stringVal;
    } else if (argCount == 2) {
        path = args[0].stringVal;
        tableName = args[1].stringVal;
    } else return (Value){VAL_INT, .intVal=0};
    
    FILE* f = fopen(path, "r");
    if (!f) return (Value){VAL_INT, .intVal=0};
    
    // Scan for @flow
    // This is inefficient (scanning every time), but robust for "Low Cost" (no RAM index).
    // Performance: O(File) per Get. Acceptable for CLI tools.
    
    // Fast-forward to @flow
    // Reset file
    fseek(f, 0, SEEK_SET);
    
    // Find "@flow"
    // Brute force scan
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
    // Inside @flow { ... }
    
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
             // Verify it is a list [
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
    
    // Found table. Should be at ':'
    fskipSpace(f);
    if (fgetc(f) != ':') { fclose(f); return (Value){VAL_INT, .intVal=0}; }
    fskipSpace(f);
    if (fgetc(f) != '[') { fclose(f); return (Value){VAL_INT, .intVal=0}; }
    
    // Created Cursor!
    UonCursor* cursor = malloc(sizeof(UonCursor));
    cursor->file = f;
    cursor->tableName = strdup(tableName);
    cursor->startOffset = ftell(f);
    
    Value v; v.type = VAL_RESOURCE; v.resourceVal = cursor;
    return v;
}

static Value fparseValue(FILE* f) {
    fskipSpace(f);
    int c = fpeek(f);
    
    if (c == '"') {
        fgetc(f);
        char buf[1024]; int i=0;
        while((c=fgetc(f)) != EOF && c != '"' && i < 1023) buf[i++] = c;
        buf[i] = '\0';
        Value v; v.type = VAL_STRING; v.stringVal = strdup(buf);
        return v;
    }
    
    if (isdigit(c) || c == '-') {
        char buf[64]; int i=0;
        while((c=fgetc(f)) != EOF && (isdigit(c) || c == '.' || c=='-') && i < 63) buf[i++] = c;
        buf[i] = '\0';
        ungetc(c, f); // Put back non-digit
        
        if (strchr(buf, '.')) {
            Value v; v.type = VAL_FLOAT; v.floatVal = atof(buf);
            return v;
        } else {
            Value v; v.type = VAL_INT; v.intVal = atoi(buf);
            return v;
        }
    }
    return (Value){VAL_INT, .intVal=0};
}

static Value uon_next(VM* vm, Value* args, int argCount) {
    (void)vm;
    if (argCount != 1 || args[0].type != VAL_RESOURCE) return (Value){VAL_INT, .intVal=0}; // null
    
    UonCursor* cursor = (UonCursor*)args[0].resourceVal;
    FILE* f = cursor->file;
    
    fskipSpace(f);
    int c = fpeek(f);
    if (c == ']') {
        // End of list
        // Close automatically?
        fclose(f);
        free(cursor->tableName);
        free(cursor);
        args[0].resourceVal = NULL; // Zero out to prevent use-after-free
        return (Value){VAL_INT, .intVal=0}; // null loop terminator
    }
    
    if (c == ',') { fgetc(f); fskipSpace(f); c = fpeek(f); }
    if (c == ']') { fclose(f); free(cursor->tableName); free(cursor); args[0].resourceVal = NULL; return (Value){VAL_INT, .intVal=0}; }

    if (c != '[') {
        // Malformed or end
        return (Value){VAL_INT, .intVal=0};
    }
    fgetc(f); // eat [
    
    // Parse Row
    Array* row = newArray();
    while ((c=fpeek(f)) != ']' && c != EOF) {
        Value val = fparseValue(f);
        arrayPush(row, val);
        fskipSpace(f);
        if (fpeek(f) == ',') fgetc(f);
    }
    if (fpeek(f) == ']') fgetc(f);
    
    // Convert to StructInstance if schema exists
    ensureInit();
    char* tn = cursor->tableName;
    MapEntry* e = mapFindEntry(uonSchemas, tn, strlen(tn), NULL);
    if (e && e->value.type == VAL_STRUCT_DEF) {
         StructInstance* obj = malloc(sizeof(StructInstance));
         obj->def = e->value.structDef;
         obj->fields = row->items;
         // Note: we steal items from Array. 
         // But Array struct will be freed by VM (gc not impl, but value semantics).
         // Actually in unnarize arrayPush copies value. 
         // Value is struct `{type, union}`.
         // If we create a StructInstance, we need an array of Values. row->items is exactly that.
         // We can free the Array wrapper but keep items.
         free(row); // Shallow free wrapper? Array struct is malloced.
         
         Value vRet; vRet.type = VAL_STRUCT_INSTANCE; vRet.structInstance = obj;
         return vRet;
    }

    Value vArr; vArr.type = VAL_ARRAY; vArr.arrayVal = row;
    return vArr;
}

static Value uon_save_dummy(VM* vm, Value* args, int argCount) {
    (void)vm; (void)args; (void)argCount;
    printf("Warning: ucoreUon.save() disabled in lazy mode (read-only optimization).\n");
    return (Value){VAL_BOOL, .boolVal=true};
}

// Stubs for insert/delete (read-only for now or append-only later)
static Value uon_noop(VM* vm, Value* args, int argCount) {
    (void)vm; (void)args; (void)argCount;
    return (Value){VAL_BOOL, .boolVal=false};
}

void registerUCoreUON(VM* vm) {
    Module* mod = malloc(sizeof(Module));
    mod->name = strdup("ucoreUon");
    mod->source = NULL;
    mod->env = malloc(sizeof(Environment));
    memset(mod->env->buckets, 0, sizeof(mod->env->buckets));
    memset(mod->env->funcBuckets, 0, sizeof(mod->env->funcBuckets));

    // Register natives
    // parse
    {
        FuncEntry* fe = malloc(sizeof(FuncEntry)); fe->key = strdup("parse");
        unsigned int h = hash("parse", 5);
        fe->next = mod->env->funcBuckets[h]; mod->env->funcBuckets[h] = fe;
        Function* fn = malloc(sizeof(Function)); fn->isNative = true; fn->native = uon_parse; fn->paramCount=1;
        fn->name = (Token){TOKEN_IDENTIFIER, fe->key, 5, 0}; fe->function = fn;
    }
    // load
    {
        FuncEntry* fe = malloc(sizeof(FuncEntry)); fe->key = strdup("load");
        unsigned int h = hash("load", 4);
        fe->next = mod->env->funcBuckets[h]; mod->env->funcBuckets[h] = fe;
        Function* fn = malloc(sizeof(Function)); fn->isNative = true; fn->native = uon_load_impl; fn->paramCount=1; // Renamed
        fn->name = (Token){TOKEN_IDENTIFIER, fe->key, 4, 0}; fe->function = fn;
    }
    // get
    {
        FuncEntry* fe = malloc(sizeof(FuncEntry)); fe->key = strdup("get");
        unsigned int h = hash("get", 3);
        fe->next = mod->env->funcBuckets[h]; mod->env->funcBuckets[h] = fe;
        Function* fn = malloc(sizeof(Function)); fn->isNative = true; fn->native = uon_get_impl; fn->paramCount=1; // Renamed
        fn->name = (Token){TOKEN_IDENTIFIER, fe->key, 3, 0}; fe->function = fn;
    }
    // next
    {
        FuncEntry* fe = malloc(sizeof(FuncEntry)); fe->key = strdup("next");
        unsigned int h = hash("next", 4);
        fe->next = mod->env->funcBuckets[h]; mod->env->funcBuckets[h] = fe;
        Function* fn = malloc(sizeof(Function)); fn->isNative = true; fn->native = uon_next; fn->paramCount=1;
        fn->name = (Token){TOKEN_IDENTIFIER, fe->key, 4, 0}; fe->function = fn;
    }

    // save/insert/delete -> noops or dummies
    // We retain them to prevent crash but they might not work as expected in streaming mode yet.
    // Ideally we append to file. But for now, read-only focus.
    {
        FuncEntry* fe = malloc(sizeof(FuncEntry)); fe->key = strdup("save");
        unsigned int h = hash("save", 4);
        fe->next = mod->env->funcBuckets[h]; mod->env->funcBuckets[h] = fe;
        Function* fn = malloc(sizeof(Function)); fn->isNative = true; fn->native = uon_save_dummy; fn->paramCount=1;
        fn->name = (Token){TOKEN_IDENTIFIER, fe->key, 4, 0}; fe->function = fn;
    }
    {
         FuncEntry* fe = malloc(sizeof(FuncEntry)); fe->key = strdup("insert");
         unsigned int h = hash("insert", 6);
         fe->next = mod->env->funcBuckets[h]; mod->env->funcBuckets[h] = fe;
         Function* fn = malloc(sizeof(Function)); fn->isNative = true; fn->native = uon_noop; fn->paramCount=2;
         fn->name = (Token){TOKEN_IDENTIFIER, fe->key, 6, 0}; fe->function = fn;
    }

    // Add to global
    VarEntry* ve = malloc(sizeof(VarEntry));
    ve->key = strdup("ucoreUon");
    ve->keyLength = 8;
    ve->ownsKey = true;
    ve->value.type = VAL_MODULE;
    ve->value.moduleVal = mod;
    unsigned int h = hash("ucoreUon", 8);
    ve->next = vm->globalEnv->buckets[h];
    vm->globalEnv->buckets[h] = ve;
}
