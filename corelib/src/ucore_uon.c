#include "ucore_uon.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Internal storage
static Map* uonSchemas = NULL; // TableName -> StructDef
static Map* uonData = NULL;    // TableName -> Array (of Arrays)

static void ensureInit() {
    if (!uonSchemas) uonSchemas = newMap();
    if (!uonData) uonData = newMap();
}

// ---- Parser Utils ----

static char peek(const char* p) { return *p; }
// static char advance(const char** p) { return *((*p)++); } // Unused

static void skipSpace(const char** p) {
    while (peek(*p) && isspace(peek(*p))) (*p)++;
}

static bool matchStr(const char** p, const char* kw) {
    int len = strlen(kw);
    if (strncmp(*p, kw, len) == 0) {
        *p += len;
        return true;
    }
    return false;
}

// Parse identifier: alphanumeric + _
// Returns malloc'd string
static char* parseIdentifier(const char** p) {
    skipSpace(p);
    const char* start = *p;
    while (isalnum(peek(*p)) || peek(*p) == '_') (*p)++;
    int len = *p - start;
    if (len == 0) return NULL;
    return strndup(start, len);
}

// Parse value: int, string (quoted), or relation ref (ignored/treated as string/int?) 
// UON flow values: [1, "Kiann", "Dev"]
static Value parseValue(const char** p) {
    skipSpace(p);
    if (peek(*p) == '"') {
        const char* start = ++(*p);
        while (peek(*p) && peek(*p) != '"') (*p)++;
        int len = *p - start;
        if (peek(*p) == '"') (*p)++;
        Value v; v.type = VAL_STRING; v.stringVal = strndup(start, len);
        return v;
    }
    // Number
    if (isdigit(peek(*p)) || peek(*p) == '-') {
        const char* start = *p;
        while (isdigit(peek(*p)) || peek(*p) == '.') (*p)++;
        int len = *p - start;
        char* buf = strndup(start, len);
        // Simple heuristic: if dot, float, else int
        if (strchr(buf, '.')) {
            Value v; v.type = VAL_FLOAT; v.floatVal = atof(buf);
            free(buf); return v;
        } else {
            Value v; v.type = VAL_INT; v.intVal = atoi(buf);
            free(buf); return v;
        }
    }
    // Default/Error
    Value v; v.type = VAL_INT; v.intVal = 0; 
    return v;
}

static void parseSchemaBlock(const char** p) {
    skipSpace(p);
    if (peek(*p) != '{') return;
    (*p)++;
    
    // entries: tableName: [col, col]
    while (peek(*p) && peek(*p) != '}') {
        skipSpace(p);
        if (peek(*p) == '}') break;
        
        char* tableName = parseIdentifier(p);
        if (!tableName) break;
        
        skipSpace(p);
        if (peek(*p) == ':') (*p)++;
        
        skipSpace(p);
        if (peek(*p) == '[') (*p)++;
        
        // Cols
        StructDef* def = malloc(sizeof(StructDef));
        def->name = tableName; // Takes ownership
        def->fields = malloc(16 * sizeof(char*));
        def->fieldCount = 0;
        int cap = 16;
        
        while (peek(*p) && peek(*p) != ']') {
            skipSpace(p);
            char* col = parseIdentifier(p);
            if (col) {
                // Check if relation (> something), ignore rest until comma or ]
                skipSpace(p);
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
        
        // Register schema
        Value vDef; vDef.type = VAL_STRUCT_DEF; vDef.structDef = def;
        mapSetStr(uonSchemas, tableName, strlen(tableName), vDef);
        
        skipSpace(p);
        if (peek(*p) == ',') (*p)++;
    }
    if (peek(*p) == '}') (*p)++;
}

static void parseFlowBlock(const char** p) {
    skipSpace(p);
    if (peek(*p) != '{') return;
    (*p)++;
    
    // entries: tableName: [ [row], [row] ]
    while (peek(*p) && peek(*p) != '}') {
        skipSpace(p);
        if (peek(*p) == '}') break;
        
        char* tableName = parseIdentifier(p);
        if (!tableName) break;
        
        skipSpace(p);
        if (peek(*p) == ':') (*p)++;
        
        skipSpace(p); 
        if (peek(*p) != '[') { free(tableName); continue; } // Error
        (*p)++; // Open list of rows
        
        Array* tableRows = newArray();
        
        while (peek(*p) && peek(*p) != ']') {
             skipSpace(p);
             if (peek(*p) == '[') {
                 // Row
                 (*p)++;
                 Array* row = newArray();
                 while (peek(*p) && peek(*p) != ']') {
                     Value v = parseValue(p);
                     arrayPush(row, v);
                     skipSpace(p);
                     if (peek(*p) == ',') (*p)++;
                 }
                 if (peek(*p) == ']') (*p)++;
                 
                 Value vRow; vRow.type = VAL_ARRAY; vRow.arrayVal = row;
                 arrayPush(tableRows, vRow);
             }
             skipSpace(p);
             if (peek(*p) == ',') (*p)++;
        }
        if (peek(*p) == ']') (*p)++;
        
        // Store
        Value vData; vData.type = VAL_ARRAY; vData.arrayVal = tableRows;
        mapSetStr(uonData, tableName, strlen(tableName), vData);
        free(tableName);
        
        skipSpace(p);
        if (peek(*p) == ',') (*p)++;
    }
    if (peek(*p) == '}') (*p)++;
}

static Value uon_parse(VM* vm, Value* args, int argCount) {
    (void)vm;
    if (argCount != 1 || args[0].type != VAL_STRING) {
        printf("Error: ucoreUon.parse expects string.\n"); // Or VM error
        Value nullVal = {VAL_INT, .intVal = 0}; return nullVal;
    }
    
    ensureInit();
    const char* p = args[0].stringVal;
    
    while (peek(p)) {
        skipSpace(&p);
        if (matchStr(&p, "@schema")) {
            parseSchemaBlock(&p);
        } else if (matchStr(&p, "@flow")) {
            parseFlowBlock(&p);
        } else {
            if (peek(p)) p++; // skip unknown or end
        }
    }
    
    Value v; v.type = VAL_BOOL; v.boolVal = true;
    return v;
}

static Value uon_get(VM* vm, Value* args, int argCount) {
    (void)vm;
    if (argCount != 1 || args[0].type != VAL_STRING) return (Value){VAL_INT, .intVal=0};
    
    ensureInit();
    char* tableName = args[0].stringVal;
    int len = strlen(tableName);
    
    MapEntry* eSchema = mapFindEntry(uonSchemas, tableName, len, NULL);
    MapEntry* eData = mapFindEntry(uonData, tableName, len, NULL);
    
    if (!eSchema || !eData) return (Value){VAL_INT, .intVal=0};
    
    StructDef* def = eSchema->value.structDef;
    Array* rows = eData->value.arrayVal;
    
    // Create array of StructInstances
    Array* resultList = newArray();
    for (int i = 0; i < rows->count; i++) {
        Value rowVal = rows->items[i];
        if (rowVal.type != VAL_ARRAY) continue;
        
        StructInstance* inst = malloc(sizeof(StructInstance));
        inst->def = def;
        inst->fields = rowVal.arrayVal->items; // Zero-copy View!
        
        Value vInst; vInst.type = VAL_STRUCT_INSTANCE; vInst.structInstance = inst;
        arrayPush(resultList, vInst);
    }
    
    Value vRes; vRes.type = VAL_ARRAY; vRes.arrayVal = resultList;
    return vRes;
}

void registerUCoreUON(VM* vm) {
    Module* mod = malloc(sizeof(Module));
    mod->name = strdup("ucoreUon");
    mod->source = NULL;
    mod->env = malloc(sizeof(Environment));
    memset(mod->env->buckets, 0, sizeof(mod->env->buckets));
    memset(mod->env->funcBuckets, 0, sizeof(mod->env->funcBuckets));
    
    // Register parse
    unsigned int hParse = hash("parse", 5);
    FuncEntry* feParse = malloc(sizeof(FuncEntry));
    feParse->key = strdup("parse");
    feParse->next = mod->env->funcBuckets[hParse];
    mod->env->funcBuckets[hParse] = feParse;
    
    Function* fnParse = malloc(sizeof(Function));
    fnParse->isNative = true;
    fnParse->native = uon_parse;
    fnParse->paramCount = 1;
    fnParse->name = (Token){TOKEN_IDENTIFIER, feParse->key, 5, 0};
    feParse->function = fnParse;
    
    // Register get
    unsigned int hGet = hash("get", 3);
    FuncEntry* feGet = malloc(sizeof(FuncEntry));
    feGet->key = strdup("get");
    feGet->next = mod->env->funcBuckets[hGet];
    mod->env->funcBuckets[hGet] = feGet;
    
    Function* fnGet = malloc(sizeof(Function));
    fnGet->isNative = true;
    fnGet->native = uon_get;
    fnGet->paramCount = 1;
    fnGet->name = (Token){TOKEN_IDENTIFIER, feGet->key, 3, 0};
    feGet->function = fnGet;
    
    // Add to global
    VarEntry* ve = malloc(sizeof(VarEntry));
    ve->key = strdup("ucoreUon");
    ve->value.type = VAL_MODULE;
    ve->value.moduleVal = mod;
    unsigned int h = hash("ucoreUon", 8);
    ve->next = vm->globalEnv->buckets[h];
    vm->globalEnv->buckets[h] = ve;
}
