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

static void parseFromSource(const char* source) {
    ensureInit();
    const char* p = source;
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
}

static Value uon_parse(VM* vm, Value* args, int argCount) {
    (void)vm;
    if (argCount != 1 || args[0].type != VAL_STRING) {
        printf("Error: ucoreUon.parse expects string.\n"); // Or VM error
        Value nullVal = {VAL_INT, .intVal = 0}; return nullVal;
    }
    
    parseFromSource(args[0].stringVal);
    
    Value v; v.type = VAL_BOOL; v.boolVal = true;
    return v;
}

static Value uon_load(VM* vm, Value* args, int argCount) {
    (void)vm;
    if (argCount != 1 || args[0].type != VAL_STRING) {
        printf("Error: ucoreUon.load expects file path string.\n");
        return (Value){VAL_INT, .intVal = 0};
    }
    char* source = readFileAll(args[0].stringVal);
    if (!source) {
        printf("Error: ucoreUon.load could not read file '%s'.\n", args[0].stringVal);
        return (Value){VAL_INT, .intVal = 0};
    }
    
    parseFromSource(source);
    free(source);
    
    Value v; v.type = VAL_BOOL; v.boolVal = true;
    return v;
}

static Value uon_insert(VM* vm, Value* args, int argCount) {
    (void)vm;
    if (argCount != 2 || args[0].type != VAL_STRING || args[1].type != VAL_ARRAY) {
        printf("Error: ucoreUon.insert(tableName, rowArray)\n");
        return (Value){VAL_INT, .intVal = 0};
    }
    
    char* tableName = args[0].stringVal;
    ensureInit();
    
    // Check if table exists
    MapEntry* eData = mapFindEntry(uonData, tableName, strlen(tableName), NULL);
    if (!eData) {
        printf("Error: Table '%s' not found.\n", tableName);
        return (Value){VAL_BOOL, .boolVal = false};
    }
    
    // Append
    arrayPush(eData->value.arrayVal, args[1]);
    
    return (Value){VAL_BOOL, .boolVal = true};
}

static Value uon_delete(VM* vm, Value* args, int argCount) {
    (void)vm;
    if (argCount != 2 || args[0].type != VAL_STRING) {
        printf("Error: ucoreUon.delete(tableName, id)\n");
        return (Value){VAL_BOOL, .boolVal = false};
    }
    
    char* tableName = args[0].stringVal;
    Value idVal = args[1];
    ensureInit();
    
    MapEntry* eData = mapFindEntry(uonData, tableName, strlen(tableName), NULL);
    if (!eData) return (Value){VAL_BOOL, .boolVal = false};
    
    Array* rows = eData->value.arrayVal;
    for (int i = 0; i < rows->count; i++) {
        Value row = rows->items[i];
        if (row.type != VAL_ARRAY || row.arrayVal->count == 0) continue;
        
        Value rowId = row.arrayVal->items[0];
        
        bool match = false;
        if (idVal.type == VAL_INT && rowId.type == VAL_INT && idVal.intVal == rowId.intVal) match = true;
        else if (idVal.type == VAL_STRING && rowId.type == VAL_STRING && strcmp(idVal.stringVal, rowId.stringVal) == 0) match = true;
        
        if (match) {
            // Swap remove
            rows->items[i] = rows->items[rows->count - 1];
            rows->count--;
            return (Value){VAL_BOOL, .boolVal = true};
        }
    }
    
    return (Value){VAL_BOOL, .boolVal = false};
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

static void fprintValue(FILE* f, Value v) {
    switch (v.type) {
        case VAL_INT: fprintf(f, "%d", v.intVal); break;
        case VAL_FLOAT: fprintf(f, "%g", v.floatVal); break;
        case VAL_STRING: fprintf(f, "\"%s\"", v.stringVal); break; // TODO: Escape quotes
        case VAL_BOOL: fprintf(f, "%s", v.boolVal ? "true" : "false"); break;
        default: fprintf(f, "null"); break;
    }
}

static Value uon_save(VM* vm, Value* args, int argCount) {
    (void)vm;
    if (argCount != 1 || args[0].type != VAL_STRING) {
        printf("Error: ucoreUon.save(path) expects path string.\n");
        return (Value){VAL_BOOL, .boolVal = false};
    }
    
    const char* path = args[0].stringVal;
    FILE* f = fopen(path, "w");
    if (!f) {
        printf("Error: Could not open file '%s' for writing.\n", path);
        return (Value){VAL_BOOL, .boolVal = false};
    }
    
    ensureInit();
    
    // Write Schema
    fprintf(f, "@schema {\n");
    for (int i = 0; i < TABLE_SIZE; i++) {
        MapEntry* e = uonSchemas->buckets[i];
        while (e) {
            StructDef* def = e->value.structDef;
            fprintf(f, "    %s: [", def->name);
            for (int k = 0; k < def->fieldCount; k++) {
                fprintf(f, "%s%s", def->fields[k], k < def->fieldCount - 1 ? ", " : "");
            }
            fprintf(f, "]");
            e = e->next;
            if (e || i < TABLE_SIZE - 1) fprintf(f, ","); // Technically trailing comma might be allowed or we just use comma for all but last... 
            // Simplified: always comma if not last in this bucket or next buckets exist. 
            // Just simple comma always is fine if parser supports it, or newline.
            // Our parser requires comma separation? 
            // Parser logic: `skipSpace(p); if (peek(*p) == ',') (*p)++;` -> yes, comma is optional/consumed if present.
            fprintf(f, "\n"); 
        }
    }
    fprintf(f, "}\n");
    
    // Write Flow
    fprintf(f, "@flow {\n");
    for (int i = 0; i < TABLE_SIZE; i++) {
        MapEntry* e = uonData->buckets[i];
        while (e) {
            char* tableName = e->key; // MapEntry has key
            Array* rows = e->value.arrayVal;
            
            fprintf(f, "    %s: [\n", tableName);
            for (int r = 0; r < rows->count; r++) {
                Value row = rows->items[r];
                if (row.type != VAL_ARRAY) continue;
                fprintf(f, "        [");
                for (int c = 0; c < row.arrayVal->count; c++) {
                    fprintValue(f, row.arrayVal->items[c]);
                    if (c < row.arrayVal->count - 1) fprintf(f, ", ");
                }
                fprintf(f, "]");
                if (r < rows->count - 1) fprintf(f, ",");
                fprintf(f, "\n");
            }
            fprintf(f, "    ]");
            
            e = e->next;
             fprintf(f, ",\n");
        }
    }
    fprintf(f, "}\n");
    
    fclose(f);
    return (Value){VAL_BOOL, .boolVal = true};
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
    
    // Register load
    unsigned int hLoad = hash("load", 4);
    FuncEntry* feLoad = malloc(sizeof(FuncEntry));
    feLoad->key = strdup("load");
    feLoad->next = mod->env->funcBuckets[hLoad];
    mod->env->funcBuckets[hLoad] = feLoad;
    
    Function* fnLoad = malloc(sizeof(Function));
    fnLoad->isNative = true;
    fnLoad->native = uon_load;
    fnLoad->paramCount = 1;
    fnLoad->name = (Token){TOKEN_IDENTIFIER, feLoad->key, 4, 0};
    feLoad->function = fnLoad;
    
    // Register insert
    unsigned int hIns = hash("insert", 6);
    FuncEntry* feIns = malloc(sizeof(FuncEntry)); feIns->key = strdup("insert");
    feIns->next = mod->env->funcBuckets[hIns]; mod->env->funcBuckets[hIns] = feIns;
    Function* fnIns = malloc(sizeof(Function)); fnIns->isNative = true; fnIns->native = uon_insert; fnIns->paramCount = 2;
    fnIns->name = (Token){TOKEN_IDENTIFIER, feIns->key, 6, 0}; feIns->function = fnIns;

    // Register delete
    unsigned int hDel = hash("delete", 6);
    FuncEntry* feDel = malloc(sizeof(FuncEntry)); feDel->key = strdup("delete");
    feDel->next = mod->env->funcBuckets[hDel]; mod->env->funcBuckets[hDel] = feDel;
    Function* fnDel = malloc(sizeof(Function)); fnDel->isNative = true; fnDel->native = uon_delete; fnDel->paramCount = 2;
    fnDel->name = (Token){TOKEN_IDENTIFIER, feDel->key, 6, 0}; feDel->function = fnDel;

    // Register save
    unsigned int hSav = hash("save", 4);
    FuncEntry* feSav = malloc(sizeof(FuncEntry)); feSav->key = strdup("save");
    feSav->next = mod->env->funcBuckets[hSav]; mod->env->funcBuckets[hSav] = feSav;
    Function* fnSav = malloc(sizeof(Function)); fnSav->isNative = true; fnSav->native = uon_save; fnSav->paramCount = 1;
    fnSav->name = (Token){TOKEN_IDENTIFIER, feSav->key, 4, 0}; feSav->function = fnSav;

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
