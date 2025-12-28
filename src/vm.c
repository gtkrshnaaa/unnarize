#include "vm.h"
#include "lexer.h"
#include "parser.h"
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dlfcn.h>
#include <time.h>

// Local Future struct definition (pointer type is exposed via vm.h)
typedef struct Future {
    bool done;              // completion flag
    Value result;           // resolved value
    pthread_mutex_t mu;     // mutex
    pthread_cond_t cv;      // condition variable
} Future;

// Forward declarations for Future helpers used by sleep thread
static Future* futureNew(void);
static void futureResolve(Future* f, Value v);
static Value futureAwait(Future* f);
static bool futureIsDone(Future* f);

// Args and thread routine for sleepAsync
typedef struct { Future* f; int ms; } SleepArgs;
static void* sleepThread(void* p) {
    SleepArgs* a = (SleepArgs*)p;
    if (a->ms > 0) {
        struct timespec ts;
        ts.tv_sec = a->ms / 1000;
        ts.tv_nsec = (long)(a->ms % 1000) * 1000000L;
        nanosleep(&ts, NULL);
    }
    Value rv; rv.type = VAL_INT; rv.intVal = 0;
    futureResolve(a->f, rv);
    free(a);
    return NULL;
}

// ---- Future helpers (blocking await) ----
static Future* futureNew(void) {
    Future* f = (Future*)malloc(sizeof(Future));
    if (!f) error("Memory allocation failed.", 0);
    f->done = false;
    // Initialize result to 0 int
    f->result.type = VAL_INT; f->result.intVal = 0;
    pthread_mutex_init(&f->mu, NULL);
    pthread_cond_init(&f->cv, NULL);
    return f;
}

static void futureResolve(Future* f, Value v) {
    pthread_mutex_lock(&f->mu);
    f->result = v;
    f->done = true;
    pthread_cond_broadcast(&f->cv);
    pthread_mutex_unlock(&f->mu);
}

static Value futureAwait(Future* f) {
    pthread_mutex_lock(&f->mu);
    while (!f->done) {
        pthread_cond_wait(&f->cv, &f->mu);
    }
    Value v = f->result;
    pthread_mutex_unlock(&f->mu);
    return v;
}

static bool futureIsDone(Future* f) {
    pthread_mutex_lock(&f->mu);
    bool d = f->done;
    pthread_mutex_unlock(&f->mu);
    return d;
}

// Optimized FNV-1a hash function for variables (faster than previous implementation)
static unsigned int hash(const char* key, int length) {
    unsigned int hash = 2166136261u;
    const unsigned char* data = (const unsigned char*)key;
    for (int i = 0; i < length; i++) {
        hash ^= data[i];
        hash *= 16777619;
    }
    return hash % TABLE_SIZE;
}

// Simple integer hash to bucket index
static unsigned int hashIntKey(int k) {
    unsigned int x = (unsigned int)k;
    x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16;
    return x % TABLE_SIZE;
}

// Module cache lookup/insert by logical module name
static ModuleEntry* findModuleEntry(VM* vm, const char* name, int length, bool insert) {
    unsigned int h = hash(name, length);
    ModuleEntry* e = vm->moduleBuckets[h];
    while (e) {
        if (strncmp(e->key, name, length) == 0 && strlen(e->key) == (size_t)length) {
            return e;
        }
        e = e->next;
    }
    if (!insert) return NULL;
    e = (ModuleEntry*)malloc(sizeof(ModuleEntry));
    if (!e) error("Memory allocation failed.", 0);
    e->key = strndup(name, length);
    if (!e->key) error("Memory allocation failed.", 0);
    e->module = NULL;
    e->next = vm->moduleBuckets[h];
    vm->moduleBuckets[h] = e;
    return e;
}

// Check if regular file exists
static bool fileExists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

// Find or insert variable with proper scope resolution
static VarEntry* findEntry(VM* vm, Token name, bool insert) {
    unsigned int h = hash(name.start, name.length);
    
    // First, search in current environment
    VarEntry* entry = vm->env->buckets[h];
    while (entry) {
        if (strncmp(entry->key, name.start, name.length) == 0 && strlen(entry->key) == (size_t)name.length) {
            return entry;
        }
        entry = entry->next;
    }
    
    // Next, if reading (not inserting), try definition environment (e.g., module env of the function)
    if (!insert && vm->defEnv && vm->defEnv != vm->env) {
        entry = vm->defEnv->buckets[h];
        while (entry) {
            if (strncmp(entry->key, name.start, name.length) == 0 && strlen(entry->key) == (size_t)name.length) {
                return entry;
            }
            entry = entry->next;
        }
    }
    
    // If not found and not inserting, search in global environment (if different from current)
    if (!insert && vm->env != vm->globalEnv) {
        entry = vm->globalEnv->buckets[h];
        while (entry) {
            if (strncmp(entry->key, name.start, name.length) == 0 && strlen(entry->key) == (size_t)name.length) {
                return entry;
            }
            entry = entry->next;
        }
    }
    
    // If inserting, create new entry in current environment
    if (insert) {
        entry = malloc(sizeof(VarEntry));
        if (!entry) {
            error("Memory allocation failed.", name.line);
        }
        entry->key = strndup(name.start, name.length);
        if (!entry->key) {
            free(entry);
            error("Memory allocation failed.", name.line);
        }
        entry->value.type = VAL_INT; // Default init
        entry->value.intVal = 0;
        entry->next = vm->env->buckets[h];
        vm->env->buckets[h] = entry;
    }
    return entry;
}

// Find or insert function in specific environment
static FuncEntry* findFuncEntry(Environment* env, Token name, bool insert) {
    unsigned int h = hash(name.start, name.length);
    FuncEntry* entry = env->funcBuckets[h];
    while (entry) {
        if (strncmp(entry->key, name.start, name.length) == 0 && strlen(entry->key) == (size_t)name.length) {
            return entry;
        }
        entry = entry->next;
    }
    if (insert) {
        entry = malloc(sizeof(FuncEntry));
        if (!entry) {
            error("Memory allocation failed.", name.line);
        }
        entry->key = strndup(name.start, name.length);
        if (!entry->key) {
            free(entry);
            error("Memory allocation failed.", name.line);
        }
        entry->function = NULL;
        entry->next = env->funcBuckets[h];
        env->funcBuckets[h] = entry;
    }
    return entry;
}

// Find function in global environment (for function calls)
static Function* findFunction(VM* vm, Token name) {
    unsigned int h = hash(name.start, name.length);
    FuncEntry* entry = vm->globalEnv->funcBuckets[h];
    while (entry) {
        if (strncmp(entry->key, name.start, name.length) == 0 && strlen(entry->key) == (size_t)name.length) {
            return entry->function;
        }
        entry = entry->next;
    }
    return NULL;
}

static Function* findFunctionInEnv(Environment* env, Token name) {
    unsigned int h = hash(name.start, name.length);
    FuncEntry* entry = env->funcBuckets[h];
    while (entry) {
        if (strncmp(entry->key, name.start, name.length) == 0 && strlen(entry->key) == (size_t)name.length) {
            return entry->function;
        }
        entry = entry->next;
    }
    return NULL;
}

// Find variable in a specific environment
static VarEntry* findVarInEnv(Environment* env, Token name) {
    unsigned int h = hash(name.start, name.length);
    VarEntry* entry = env->buckets[h];
    while (entry) {
        if (strncmp(entry->key, name.start, name.length) == 0 && strlen(entry->key) == (size_t)name.length) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}

// Read whole file
static char* readFileAll(const char* path) {
    FILE* file = fopen(path, "rb");
    if (!file) return NULL;
    fseek(file, 0L, SEEK_END);
    long size = ftell(file);
    rewind(file);
    char* buf = malloc(size + 1);
    if (!buf) { fclose(file); return NULL; }
    size_t n = fread(buf, 1, size, file);
    buf[n] = '\0';
    fclose(file);
    return buf;
}

// Recursively search for a filename under root. Returns malloc'd full path or NULL
static char* searchFileRecursive(const char* root, const char* filename) {
    DIR* dir = opendir(root);
    if (!dir) return NULL;
    struct dirent* ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        // Skip common build dirs
        if (strcmp(ent->d_name, ".unnat") == 0 || strcmp(ent->d_name, "bin") == 0 || strcmp(ent->d_name, "obj") == 0) continue;
        char path[2048];
        snprintf(path, sizeof(path), "%s/%s", root, ent->d_name);
        struct stat st;
        if (stat(path, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            char* found = searchFileRecursive(path, filename);
            if (found) { closedir(dir); return found; }
        } else if (S_ISREG(st.st_mode)) {
            if (strcmp(ent->d_name, filename) == 0) {
                char* full = strdup(path);
                closedir(dir);
                return full;
            }
        }
    }
    closedir(dir);
    return NULL;
}

// Convert 1-char string to int code if applicable
static bool tryCharCode(Value v, int* out) {
    if (v.type == VAL_STRING && v.stringVal && strlen(v.stringVal) == 1) {
        *out = (unsigned char)v.stringVal[0];
        return true;
    }
    return false;
}

// ---- Array helpers ----
static Array* newArray(void) {
    Array* a = (Array*)malloc(sizeof(Array));
    if (!a) error("Memory allocation failed.", 0);
    a->items = NULL; a->count = 0; a->capacity = 0;
    return a;
}
static void arrayEnsureCap(Array* a, int cap) {
    if (a->capacity >= cap) return;
    int nc = a->capacity < 8 ? 8 : a->capacity * 2;
    if (nc < cap) nc = cap;
    Value* ni = (Value*)realloc(a->items, sizeof(Value) * nc);
    if (!ni) error("Memory allocation failed.", 0);
    a->items = ni; a->capacity = nc;
}
static void arrayPush(Array* a, Value v) {
    arrayEnsureCap(a, a->count + 1);
    a->items[a->count++] = v;
}
static bool arrayPop(Array* a, Value* out) {
    if (a->count == 0) return false;
    *out = a->items[a->count - 1];
    a->count--;
    return true;
}

// ---- Map helpers ----
static Map* newMap(void) {
    Map* m = (Map*)malloc(sizeof(Map));
    if (!m) error("Memory allocation failed.", 0);
    for (int i = 0; i < TABLE_SIZE; i++) m->buckets[i] = NULL;
    return m;
}
static MapEntry* mapFindEntry(Map* m, const char* skey, int slen, int* bucketOut) {
    unsigned int h = hash(skey, slen);
    if (bucketOut) *bucketOut = (int)h;
    MapEntry* e = m->buckets[h];
    while (e) {
        if (!e->isIntKey && e->key && (int)strlen(e->key) == slen && strncmp(e->key, skey, slen) == 0) return e;
        e = e->next;
    }
    return NULL;
}
static MapEntry* mapFindEntryInt(Map* m, int ikey, int* bucketOut) {
    unsigned int h = hashIntKey(ikey);
    if (bucketOut) *bucketOut = (int)h;
    MapEntry* e = m->buckets[h];
    while (e) {
        if (e->isIntKey && e->intKey == ikey) return e;
        e = e->next;
    }
    return NULL;
}
static void mapSetStr(Map* m, const char* key, int len, Value v) {
    int b; MapEntry* e = mapFindEntry(m, key, len, &b);
    if (e) { e->value = v; return; }
    e = (MapEntry*)malloc(sizeof(MapEntry)); if (!e) error("Memory allocation failed.", 0);
    e->isIntKey = false; e->intKey = 0;
    e->key = strndup(key, len); if (!e->key) { free(e); error("Memory allocation failed.", 0);} 
    e->value = v; e->next = m->buckets[b]; m->buckets[b] = e;
}
static void mapSetInt(Map* m, int ikey, Value v) {
    int b; MapEntry* e = mapFindEntryInt(m, ikey, &b);
    if (e) { e->value = v; return; }
    e = (MapEntry*)malloc(sizeof(MapEntry)); if (!e) error("Memory allocation failed.", 0);
    e->isIntKey = true; e->intKey = ikey; e->key = NULL; e->value = v; e->next = m->buckets[b]; m->buckets[b] = e;
}
static bool mapDeleteStr(Map* m, const char* key, int len) {
    unsigned int b = hash(key, len);
    MapEntry* prev = NULL; MapEntry* e = m->buckets[b];
    while (e) {
        if (!e->isIntKey && e->key && (int)strlen(e->key) == len && strncmp(e->key, key, len) == 0) {
            if (prev) prev->next = e->next; else m->buckets[b] = e->next;
            free(e->key); free(e);
            return true;
        }
        prev = e; e = e->next;
    }
    return false;
}
static bool mapDeleteInt(Map* m, int ikey) {
    unsigned int b = hashIntKey(ikey);
    MapEntry* prev = NULL; MapEntry* e = m->buckets[b];
    while (e) {
        if (e->isIntKey && e->intKey == ikey) {
            if (prev) prev->next = e->next; else m->buckets[b] = e->next;
            free(e);
            return true;
        }
        prev = e; e = e->next;
    }
    return false;
}

// String interning functions for performance optimization
// Currently unused but kept for future optimization implementation
__attribute__((unused)) static char* internString(VM* vm, const char* str, int length) {
    if (!str) return NULL;
    if (length <= 0) {
        // Handle empty string case
        char* empty = malloc(1);
        if (!empty) return NULL;
        empty[0] = '\0';
        return empty;
    }
    
    unsigned int h = hash(str, length);
    
    // Check if string already exists in pool
    for (int i = 0; i < vm->stringPool.count; i++) {
        if (vm->stringPool.hashes[i] == h && 
            strlen(vm->stringPool.strings[i]) == (size_t)length &&
            strncmp(vm->stringPool.strings[i], str, length) == 0) {
            return vm->stringPool.strings[i];
        }
    }
    
    // Grow pool if needed
    if (vm->stringPool.count >= vm->stringPool.capacity) {
        vm->stringPool.capacity *= 2;
        vm->stringPool.strings = realloc(vm->stringPool.strings, 
                                        vm->stringPool.capacity * sizeof(char*));
        vm->stringPool.hashes = realloc(vm->stringPool.hashes, 
                                       vm->stringPool.capacity * sizeof(unsigned int));
        if (!vm->stringPool.strings || !vm->stringPool.hashes) {
            error("Memory allocation failed for string pool expansion.", 0);
        }
    }
    
    // Add new string to pool
    char* interned = strndup(str, length);
    if (!interned) error("Memory allocation failed for string interning.", 0);
    
    vm->stringPool.strings[vm->stringPool.count] = interned;
    vm->stringPool.hashes[vm->stringPool.count] = h;
    vm->stringPool.count++;
    
    return interned;
}

// Forward declarations
static Value evaluate(VM* vm, Node* node);
static void execute(VM* vm, Node* node);

// Exposed registration API for external libraries
void registerNativeFunction(VM* vm, const char* name, Value (*function)(VM*, Value* args, int argCount)) {
    if (!vm || !name || !*name || !function) {
        error("registerNativeFunction: invalid arguments.", 0);
    }
    Token t; t.type = TOKEN_IDENTIFIER; t.start = name; t.length = (int)strlen(name); t.line = 0;
    FuncEntry* entry = findFuncEntry(vm->globalEnv, t, true);
    if (!entry) error("registerNativeFunction: failed to allocate function entry.", 0);
    // Allocate or replace function
    if (entry->function) {
        free(entry->function);
        entry->function = NULL;
    }
    Function* func = (Function*)malloc(sizeof(Function));
    if (!func) error("Memory allocation failed.", 0);
    // Set function name token to point to entry->key which is owned by the table
    func->name.type = TOKEN_IDENTIFIER;
    func->name.start = entry->key;
    func->name.length = (int)strlen(entry->key);
    func->name.line = 0;
    func->params = NULL;
    func->paramCount = 0; // not enforced for native
    func->body = NULL;
    func->closure = NULL;
    func->isNative = true;
    func->native = function;
    entry->function = func;
}

// Execute function call
static Value callFunction(VM* vm, Function* func, Value* args, int argCount) {
    if (func->isNative) {
        // Direct call to native C function
        return func->native(vm, args, argCount);
    }
    if (vm->callStackTop >= CALL_STACK_MAX) {
        error("Call stack overflow.", 0);
    }
    
    // Create new environment for function execution
    Environment* funcEnv = malloc(sizeof(Environment));
    if (!funcEnv) error("Memory allocation failed.", 0);
    memset(funcEnv->buckets, 0, sizeof(funcEnv->buckets));
    memset(funcEnv->funcBuckets, 0, sizeof(funcEnv->funcBuckets));
    
    // Check parameter count
    if (argCount != func->paramCount) {
        char errorMsg[256];
        snprintf(errorMsg, sizeof(errorMsg), "Expected %d arguments but got %d.", func->paramCount, argCount);
        error(errorMsg, 0);
    }
    
    // Set up parameters in function environment
    for (int i = 0; i < argCount; i++) {
        VarEntry* paramEntry = malloc(sizeof(VarEntry));
        if (!paramEntry) error("Memory allocation failed.", 0);
        
        paramEntry->key = strndup(func->params[i].start, func->params[i].length);
        if (!paramEntry->key) {
            free(paramEntry);
            error("Memory allocation failed.", 0);
        }
        
        paramEntry->value = args[i];
        unsigned int hashVal = hash(paramEntry->key, strlen(paramEntry->key));
        paramEntry->next = funcEnv->buckets[hashVal];
        funcEnv->buckets[hashVal] = paramEntry;
    }
    
    // Push call frame
    CallFrame* frame = &vm->callStack[vm->callStackTop++];
    frame->env = vm->env;
    frame->hasReturned = false;
    frame->returnValue.type = VAL_INT;
    frame->returnValue.intVal = 0;
    
    // Switch to function environment and set definition env to function's closure
    Environment* oldEnv = vm->env;
    Environment* oldDef = vm->defEnv;
    vm->env = funcEnv;
    if (func->closure) vm->defEnv = func->closure;
    
    // Execute function body
    execute(vm, func->body);
    
    // Get return value
    Value returnValue = frame->returnValue;
    
    // Pop call frame and restore environments
    vm->callStackTop--;
    vm->env = oldEnv;
    vm->defEnv = oldDef;
    
    // Clean up function environment
    for (int i = 0; i < TABLE_SIZE; i++) {
        VarEntry* entry = funcEnv->buckets[i];
        while (entry) {
            VarEntry* next = entry->next;
            free(entry->key);
            // Do NOT free entry->value.stringVal here.
            // String values passed as arguments or assigned to locals may alias
            // memory owned by outer scopes; freeing here can cause double-free
            // or use-after-free when returning strings.
            free(entry);
            entry = next;
        }
        
        // Function buckets should be empty in local function environment
        FuncEntry* funcEntry = funcEnv->funcBuckets[i];
        while (funcEntry) {
            FuncEntry* next = funcEntry->next;
            free(funcEntry->key);
            free(funcEntry);
            funcEntry = next;
        }
    }
    free(funcEnv);
    
    // For async functions (MVP): wrap the result into an already-resolved Future
    if (func->isAsync) {
        Future* f = futureNew();
        futureResolve(f, returnValue);
        Value v; v.type = VAL_FUTURE; v.futureVal = f; return v;
    }
    return returnValue;
}

// Execute statement
static void execute(VM* vm, Node* node) {
    if (!node) {
        error("Null statement.", 0);
        return;
    }
    
    switch (node->type) {
        case NODE_STMT_VAR_DECL: {
            Value init = {VAL_INT, .intVal = 0}; // Default 0
            if (node->varDecl.initializer) {
                init = evaluate(vm, node->varDecl.initializer);
            }
            VarEntry* entry = findEntry(vm, node->varDecl.name, true);
            if (entry) {
                // Free old string value if exists
                if (entry->value.type == VAL_STRING && entry->value.stringVal) {
                    free(entry->value.stringVal);
                }
                entry->value = init;
            }
            break;
        }
        case NODE_STMT_ASSIGN: {
            Value value = evaluate(vm, node->assign.value);
            VarEntry* entry = findEntry(vm, node->assign.name, false);
            if (entry) {
                // Free old string value if exists
                if (entry->value.type == VAL_STRING && entry->value.stringVal) {
                    free(entry->value.stringVal);
                }
                entry->value = value;
            } else {
                error("Undefined variable.", node->assign.name.line);
            }
            break;
        }
        case NODE_STMT_PRINT: {
            Value value = evaluate(vm, node->print.expr);
            switch (value.type) {
                case VAL_INT: 
                    printf("%d\n", value.intVal); 
                    break;
                case VAL_FLOAT: 
                    printf("%.6g\n", value.floatVal); 
                    break;
                case VAL_STRING: 
                    printf("%s\n", value.stringVal ? value.stringVal : "(null)"); 
                    break;
                case VAL_BOOL: 
                    printf("%s\n", value.boolVal ? "true" : "false"); 
                    break;
                case VAL_MODULE:
                    printf("[module %s]\n", (value.moduleVal && value.moduleVal->name) ? value.moduleVal->name : "<anon>");
                    break;
                case VAL_ARRAY:
                    printf("[array length=%d]\n", value.arrayVal ? value.arrayVal->count : 0);
                    break;
                case VAL_MAP: {
                    // compute size
                    int sz = 0; if (value.mapVal) { for (int i = 0; i < TABLE_SIZE; i++) { MapEntry* e = value.mapVal->buckets[i]; while (e) { sz++; e = e->next; } } }
                    printf("{map size=%d}\n", sz);
                    break;
                }
                case VAL_FUTURE: {
                    bool d = value.futureVal ? futureIsDone(value.futureVal) : false;
                    printf("[future %s]\n", d ? "done" : "pending");
                    break;
                }
            }
            break;
        }
        case NODE_STMT_INDEX_ASSIGN: {
            Value target = evaluate(vm, node->indexAssign.target);
            Value idx = evaluate(vm, node->indexAssign.index);
            Value val = evaluate(vm, node->indexAssign.value);
            if (target.type == VAL_ARRAY) {
                if (idx.type != VAL_INT) error("Array index must be int.", 0);
                if (idx.intVal < 0 || idx.intVal >= (target.arrayVal ? target.arrayVal->count : 0)) {
                    error("Array index out of range.", 0);
                }
                target.arrayVal->items[idx.intVal] = val;
            } else if (target.type == VAL_MAP) {
                if (idx.type == VAL_INT) {
                    mapSetInt(target.mapVal, idx.intVal, val);
                } else if (idx.type == VAL_STRING) {
                    mapSetStr(target.mapVal, idx.stringVal ? idx.stringVal : "", (int)strlen(idx.stringVal ? idx.stringVal : ""), val);
                } else {
                    error("Map key must be int or string.", 0);
                }
            } else {
                error("Index assignment not supported for this type.", 0);
            }
            break;
        }
        case NODE_STMT_IF: {
            Value cond = evaluate(vm, node->ifStmt.condition);
            bool isTrue = false;
            switch (cond.type) {
                case VAL_BOOL: 
                    isTrue = cond.boolVal; 
                    break;
                case VAL_INT: 
                    isTrue = cond.intVal != 0; 
                    break;
                case VAL_FLOAT: 
                    isTrue = cond.floatVal != 0.0; 
                    break;
                case VAL_STRING: 
                    isTrue = cond.stringVal != NULL && strlen(cond.stringVal) > 0; 
                    break;
                case VAL_MODULE:
                    isTrue = true; // treat as truthy
                    break;
                case VAL_ARRAY:
                    isTrue = cond.arrayVal && cond.arrayVal->count > 0;
                    break;
                case VAL_MAP: {
                    int sz = 0;
                    if (cond.mapVal) {
                        for (int i = 0; i < TABLE_SIZE; i++) {
                            MapEntry* e = cond.mapVal->buckets[i];
                            while (e) { sz++; e = e->next; }
                        }
                    }
                    isTrue = sz > 0;
                    break;
                }
                case VAL_FUTURE:
                    error("Condition cannot be a Future. Use await.", 0);
                    break;
            }
            
            if (isTrue) {
                execute(vm, node->ifStmt.thenBranch);
            } else if (node->ifStmt.elseBranch) {
                execute(vm, node->ifStmt.elseBranch);
            }
            break;
        }
        case NODE_STMT_WHILE: {
            while (true) {
                Value cond = evaluate(vm, node->whileStmt.condition);
                bool isTrue = false;
                switch (cond.type) {
                    case VAL_BOOL: 
                        isTrue = cond.boolVal; 
                        break;
                    case VAL_INT: 
                        isTrue = cond.intVal != 0; 
                        break;
                    case VAL_FLOAT: 
                        isTrue = cond.floatVal != 0.0; 
                        break;
                    case VAL_STRING: 
                        isTrue = cond.stringVal != NULL && strlen(cond.stringVal) > 0; 
                        break;
                    case VAL_MODULE:
                        isTrue = true;
                        break;
                    case VAL_ARRAY:
                        isTrue = cond.arrayVal && cond.arrayVal->count > 0;
                        break;
                    case VAL_MAP: {
                        int sz = 0;
                        if (cond.mapVal) {
                            for (int i = 0; i < TABLE_SIZE; i++) {
                                MapEntry* e = cond.mapVal->buckets[i];
                                while (e) { sz++; e = e->next; }
                            }
                        }
                        isTrue = sz > 0;
                        break;
                    }
                    case VAL_FUTURE:
                        error("Condition cannot be a Future. Use await.", 0);
                        break;
                }
                if (!isTrue) break;
                execute(vm, node->whileStmt.body);
            }
            break;
        }
        case NODE_STMT_FOR: {
            if (node->forStmt.initializer) {
                execute(vm, node->forStmt.initializer);
            }
            
            while (true) {
                bool condTrue = true;
                if (node->forStmt.condition) {
                    Value cond = evaluate(vm, node->forStmt.condition);
                    switch (cond.type) {
                        case VAL_BOOL: 
                            condTrue = cond.boolVal; 
                            break;
                        case VAL_INT: 
                            condTrue = cond.intVal != 0; 
                            break;
                        case VAL_FLOAT: 
                            condTrue = cond.floatVal != 0.0; 
                            break;
                        case VAL_STRING: 
                            condTrue = cond.stringVal != NULL && strlen(cond.stringVal) > 0; 
                            break;
                        case VAL_MODULE:
                            condTrue = true;
                            break;
                        case VAL_ARRAY:
                            condTrue = cond.arrayVal && cond.arrayVal->count > 0;
                            break;
                        case VAL_MAP: {
                            int sz = 0;
                            if (cond.mapVal) {
                                for (int i = 0; i < TABLE_SIZE; i++) {
                                    MapEntry* e = cond.mapVal->buckets[i];
                                    while (e) { sz++; e = e->next; }
                                }
                            }
                            condTrue = sz > 0;
                            break;
                        }
                        case VAL_FUTURE:
                            error("Condition cannot be a Future. Use await.", 0);
                            break;
                    }
                }
                if (!condTrue) break;
                
                execute(vm, node->forStmt.body);
                
                if (node->forStmt.increment) {
                    execute(vm, node->forStmt.increment);
                }
            }
            break;
        }
        case NODE_STMT_BLOCK: {
            for (int i = 0; i < node->block.count; i++) {
                execute(vm, node->block.statements[i]);
                
                // Check if we returned from within the block
                if (vm->callStackTop > 0 && vm->callStack[vm->callStackTop - 1].hasReturned) {
                    break;
                }
            }
            break;
        }
        case NODE_STMT_FUNCTION: {
            // Register function in current definition environment (global or module)
            Environment* target = vm->defEnv ? vm->defEnv : vm->globalEnv;
            FuncEntry* entry = findFuncEntry(target, node->function.name, true);
            if (entry && !entry->function) {
                Function* func = malloc(sizeof(Function));
                if (!func) error("Memory allocation failed.", node->function.name.line);
                
                func->name = node->function.name;
                func->params = node->function.params;
                func->paramCount = node->function.paramCount;
                func->body = node->function.body;
                // Capture the environment where the function is defined (for module/global lookup)
                func->closure = target;
                func->isNative = false;
                func->native = NULL;
                func->isAsync = node->function.isAsync;
                entry->function = func;
            } else {
                error("Function already defined.", node->function.name.line);
            }
            break;
        }
        case NODE_STMT_LOADEXTERN: {
            Value pathVal = evaluate(vm, node->loadexternStmt.pathExpr);
            if (pathVal.type != VAL_STRING || !pathVal.stringVal) {
                error("loadextern() argument must be a string.", 0);
                break;
            }

            const char* libArg = pathVal.stringVal;
            char* fullPath = NULL;

            // If the argument contains a '/', treat it as an explicit path.
            // Otherwise, resolve similar to import: try UNNARIZE_LIB_PATH, then search projectRoot.
            if (strchr(libArg, '/') != NULL) {
                fullPath = strdup(libArg);
            } else {
                char candidate[2048];
                const char* lp = getenv("UNNARIZE_LIB_PATH");
                if (lp && *lp) {
                    const char* p = lp;
                    while (*p) {
                        char dirbuf[1024];
                        int di = 0;
                        while (*p && *p != ':' && di < (int)sizeof(dirbuf) - 1) {
                            dirbuf[di++] = *p++;
                        }
                        dirbuf[di] = '\0';
                        if (*p == ':') p++;
                        if (di == 0) continue;
                        snprintf(candidate, sizeof(candidate), "%s/%s", dirbuf, libArg);
                        if (fileExists(candidate)) { fullPath = strdup(candidate); break; }
                    }
                }
                if (!fullPath) {
                    fullPath = searchFileRecursive(vm->projectRoot, libArg);
                }
            }

            if (!fullPath) {
                char errorMsg[512];
                snprintf(errorMsg, sizeof(errorMsg),
                         "External library '%s' not found. Searched UNNARIZE_LIB_PATH and project root.",
                         libArg);
                error(errorMsg, 0);
                break;
            }

            void* handle = dlopen(fullPath, RTLD_NOW | RTLD_GLOBAL);
            if (!handle) {
                char errorMsg[512];
                snprintf(errorMsg, sizeof(errorMsg),
                         "Failed to load external library '%s': %s", fullPath, dlerror());
                free(fullPath);
                error(errorMsg, 0);
                break;
            }
            void (*regfn)(VM*) = (void (*)(VM*))dlsym(handle, "registerFunctions");
            if (!regfn) {
                char errorMsg[512];
                snprintf(errorMsg, sizeof(errorMsg),
                         "Could not find 'registerFunctions' in '%s': %s", fullPath, dlerror());
                free(fullPath);
                dlclose(handle);
                error(errorMsg, 0);
                break;
            }
            // Call the registration function
            regfn(vm);
            // Store handle for cleanup
            if (vm->externHandleCount >= TABLE_SIZE) {
                free(fullPath);
                dlclose(handle);
                error("Too many external libraries loaded (max 256).", 0);
                break;
            }
            vm->externHandles[vm->externHandleCount++] = handle;
            free(fullPath);
            break;
        }
        case NODE_STMT_IMPORT: {
            // Build filename <module>.unna and resolve via cache, UNNARIZE_PATH, then projectRoot
            char modName[256];
            snprintf(modName, sizeof(modName), "%.*s.unna", node->importStmt.module.length, node->importStmt.module.start);
            // Cache check by logical module name (not alias)
            ModuleEntry* mentry = findModuleEntry(vm, node->importStmt.module.start, node->importStmt.module.length, false);
            if (mentry && mentry->module) {
                VarEntry* aliasEntry = findEntry(vm, node->importStmt.alias, true);
                aliasEntry->value.type = VAL_MODULE;
                aliasEntry->value.moduleVal = mentry->module;
                break;
            }

            char* fullPath = NULL;
            char candidate[2048];
            // Try UNNARIZE_PATH first for speed and explicitness
            const char* gp = getenv("UNNARIZE_PATH");
            if (gp && *gp) {
                const char* p = gp;
                while (*p) {
                    char dirbuf[1024];
                    int di = 0;
                    while (*p && *p != ':' && di < (int)sizeof(dirbuf) - 1) {
                        dirbuf[di++] = *p++;
                    }
                    dirbuf[di] = '\0';
                    if (*p == ':') p++;
                    if (di == 0) continue;
                    snprintf(candidate, sizeof(candidate), "%s/%s", dirbuf, modName);
                    if (fileExists(candidate)) { fullPath = strdup(candidate); break; }
                }
            }
            // Fallback: search under projectRoot
            if (!fullPath) {
                fullPath = searchFileRecursive(vm->projectRoot, modName);
            }
            if (!fullPath) {
                error("Module file not found in project.", node->importStmt.module.line);
            }
            char* source = readFileAll(fullPath);
            if (!source) {
                free(fullPath);
                error("Failed to read module file.", node->importStmt.module.line);
            }

            // Prepare module environment
            Environment* moduleEnv = malloc(sizeof(Environment));
            if (!moduleEnv) error("Memory allocation failed.", node->importStmt.module.line);
            memset(moduleEnv->buckets, 0, sizeof(moduleEnv->buckets));
            memset(moduleEnv->funcBuckets, 0, sizeof(moduleEnv->funcBuckets));

            // Parse and execute module in its env
            Lexer lx; initLexer(&lx, source);
            Parser ps; initParser(&ps);
            while (1) {
                Token tk = scanToken(&lx);
                addToken(&ps, tk);
                if (tk.type == TOKEN_EOF) break;
            }
            Node* ast = parse(&ps);

            // Save current env/defEnv
            Environment* oldEnv = vm->env;
            Environment* oldDef = vm->defEnv;
            vm->env = moduleEnv;
            vm->defEnv = moduleEnv;
            execute(vm, ast);
            // Restore
            vm->env = oldEnv;
            vm->defEnv = oldDef;

            // Wrap module value
            Module* module = malloc(sizeof(Module));
            if (!module) error("Memory allocation failed.", node->importStmt.module.line);
            module->name = strndup(node->importStmt.alias.start, node->importStmt.alias.length);
            module->env = moduleEnv;
            module->source = source; // keep source alive for token/text lifetime

            VarEntry* aliasEntry = findEntry(vm, node->importStmt.alias, true);
            aliasEntry->value.type = VAL_MODULE;
            aliasEntry->value.moduleVal = module;

            // Store in cache by logical module name (not alias)
            ModuleEntry* store = findModuleEntry(vm, node->importStmt.module.start, node->importStmt.module.length, true);
            store->module = module;

            // Cleanup parser/ast (keep moduleEnv and source for module lifetime)
            // Free AST and parser tokens
            // We don't have freeAST here; import inside VM uses execute recursively, assume main frees AST of top-level only.
            // Minimal cleanup for module parsing structures:
            freeParser(&ps);
            free(fullPath);
            // Note: ast nodes are part of module functions' bodies, not executed again; freeing here would invalidate. So we intentionally leak small AST of module or could store for module lifetime.
            break;
        }
        case NODE_STMT_RETURN: {
            if (vm->callStackTop == 0) {
                error("Return statement outside function.", 0);
                break;
            }
            
            CallFrame* frame = &vm->callStack[vm->callStackTop - 1];
            frame->hasReturned = true;
            
            if (node->returnStmt.value) {
                frame->returnValue = evaluate(vm, node->returnStmt.value);
            } else {
                frame->returnValue.type = VAL_INT;
                frame->returnValue.intVal = 0;
            }
            break;
        }
        default:
            // Expression statement
            evaluate(vm, node);
            break;
    }
}

// Evaluate expression
static Value evaluate(VM* vm, Node* node) {
    if (!node) {
        error("Null expression.", 0);
        Value nullVal = {VAL_INT, .intVal = 0};
        return nullVal;
    }
    
    switch (node->type) {
        case NODE_EXPR_LITERAL: {
            Token t = node->literal.token;
            Value val;
            
            if (t.type == TOKEN_NUMBER) {
                char* str = strndup(t.start, t.length);
                if (!str) error("Memory allocation failed.", t.line);
                
                if (strchr(str, '.')) {
                    val.type = VAL_FLOAT;
                    val.floatVal = atof(str);
                } else {
                    val.type = VAL_INT;
                    val.intVal = atoi(str);
                }
                free(str);
            } else if (t.type == TOKEN_STRING) {
                val.type = VAL_STRING;
                // Skip quotes in string (start + 1, length - 2) - use original method for now
                if (t.length >= 2) {
                    val.stringVal = strndup(t.start + 1, t.length - 2);
                } else {
                    val.stringVal = strdup("");
                }
                if (!val.stringVal) error("Memory allocation failed.", t.line);
                if (!val.stringVal) error("Memory allocation failed.", t.line);
            } else if (t.type == TOKEN_TRUE) {
                val.type = VAL_BOOL;
                val.boolVal = true;
            } else if (t.type == TOKEN_FALSE) {
                val.type = VAL_BOOL;
                val.boolVal = false;
            } else {
                error("Invalid literal type.", t.line);
                val.type = VAL_INT;
                val.intVal = 0;
            }
            return val;
        }
        
        case NODE_EXPR_VAR: {
            VarEntry* entry = findEntry(vm, node->var.name, false);
            if (entry) {
                return entry->value;
            }
            error("Undefined variable.", node->var.name.line);
            Value nullVal = {VAL_INT, .intVal = 0};
            return nullVal;
        }
        
        case NODE_EXPR_UNARY: {
            Value expr = evaluate(vm, node->unary.expr);
            if (node->unary.op.type == TOKEN_MINUS) {
                if (expr.type == VAL_INT) {
                    expr.intVal = -expr.intVal;
                } else if (expr.type == VAL_FLOAT) {
                    expr.floatVal = -expr.floatVal;
                } else {
                    error("Cannot negate non-numeric value.", node->unary.op.line);
                }
            }
            return expr;
        }
        
        case NODE_EXPR_BINARY: {
            Value left = evaluate(vm, node->binary.left);
            Value right = evaluate(vm, node->binary.right);
            Value result;
            
            // Handle string concatenation with +
            if (node->binary.op.type == TOKEN_PLUS && (left.type == VAL_STRING || right.type == VAL_STRING)) {
                char leftStr[256], rightStr[256];
                
                // Convert left operand to string
                switch (left.type) {
                    case VAL_INT: 
                        snprintf(leftStr, sizeof(leftStr), "%d", left.intVal); 
                        break;
                    case VAL_FLOAT: 
                        snprintf(leftStr, sizeof(leftStr), "%.6g", left.floatVal); 
                        break;
                    case VAL_BOOL: 
                        strcpy(leftStr, left.boolVal ? "true" : "false"); 
                        break;
                    case VAL_STRING: 
                        strncpy(leftStr, left.stringVal ? left.stringVal : "", sizeof(leftStr) - 1); 
                        leftStr[sizeof(leftStr) - 1] = '\0';
                        break;
                    case VAL_MODULE:
                        strncpy(leftStr, "[module]", sizeof(leftStr) - 1);
                        leftStr[sizeof(leftStr) - 1] = '\0';
                        break;
                    case VAL_ARRAY: {
                        int l = (left.arrayVal ? left.arrayVal->count : 0);
                        snprintf(leftStr, sizeof(leftStr), "[array length=%d]", l);
                        break;
                    }
                    case VAL_MAP: {
                        int sz = 0; if (left.mapVal) { for (int i = 0; i < TABLE_SIZE; i++) { MapEntry* e = left.mapVal->buckets[i]; while (e) { sz++; e = e->next; } } }
                        snprintf(leftStr, sizeof(leftStr), "{map size=%d}", sz);
                        break;
                    }
                    case VAL_FUTURE: {
                        bool d = left.futureVal ? futureIsDone(left.futureVal) : false;
                        snprintf(leftStr, sizeof(leftStr), "[future %s]", d ? "done" : "pending");
                        break;
                    }
                }
                
                // Convert right operand to string
                switch (right.type) {
                    case VAL_INT: 
                        snprintf(rightStr, sizeof(rightStr), "%d", right.intVal); 
                        break;
                    case VAL_FLOAT: 
                        snprintf(rightStr, sizeof(rightStr), "%.6g", right.floatVal); 
                        break;
                    case VAL_BOOL: 
                        strcpy(rightStr, right.boolVal ? "true" : "false"); 
                        break;
                    case VAL_STRING: 
                        strncpy(rightStr, right.stringVal ? right.stringVal : "", sizeof(rightStr) - 1); 
                        rightStr[sizeof(rightStr) - 1] = '\0';
                        break;
                    case VAL_MODULE:
                        strncpy(rightStr, "[module]", sizeof(rightStr) - 1);
                        rightStr[sizeof(rightStr) - 1] = '\0';
                        break;
                    case VAL_ARRAY: {
                        int l = (right.arrayVal ? right.arrayVal->count : 0);
                        snprintf(rightStr, sizeof(rightStr), "[array length=%d]", l);
                        break;
                    }
                    case VAL_MAP: {
                        int sz = 0; if (right.mapVal) { for (int i = 0; i < TABLE_SIZE; i++) { MapEntry* e = right.mapVal->buckets[i]; while (e) { sz++; e = e->next; } } }
                        snprintf(rightStr, sizeof(rightStr), "{map size=%d}", sz);
                        break;
                    }
                    case VAL_FUTURE: {
                        bool d = right.futureVal ? futureIsDone(right.futureVal) : false;
                        snprintf(rightStr, sizeof(rightStr), "[future %s]", d ? "done" : "pending");
                        break;
                    }
                }
                
                result.type = VAL_STRING;
                result.stringVal = malloc(strlen(leftStr) + strlen(rightStr) + 1);
                if (!result.stringVal) error("Memory allocation failed.", node->binary.op.line);
                strcpy(result.stringVal, leftStr);
                strcat(result.stringVal, rightStr);
                
                return result;
            }
            
            // Handle comparison operations for different types
            if (node->binary.op.type == TOKEN_EQUAL_EQUAL || node->binary.op.type == TOKEN_BANG_EQUAL) {
                result.type = VAL_BOOL;
                bool isEqual = false;
                
                // Same type comparisons
                if (left.type == right.type) {
                    switch (left.type) {
                        case VAL_INT:
                            isEqual = left.intVal == right.intVal;
                            break;
                        case VAL_FLOAT:
                            isEqual = left.floatVal == right.floatVal;
                            break;
                        case VAL_BOOL:
                            isEqual = left.boolVal == right.boolVal;
                            break;
                        case VAL_STRING:
                            if (left.stringVal && right.stringVal) {
                                isEqual = strcmp(left.stringVal, right.stringVal) == 0;
                            } else {
                                isEqual = (left.stringVal == NULL && right.stringVal == NULL);
                            }
                            break;
                        case VAL_MODULE:
                            // Compare by module identity (pointer equality)
                            isEqual = left.moduleVal == right.moduleVal;
                            break;
                        case VAL_ARRAY:
                            // Compare by identity (pointer equality)
                            isEqual = left.arrayVal == right.arrayVal;
                            break;
                        case VAL_MAP:
                            // Compare by identity (pointer equality)
                            isEqual = left.mapVal == right.mapVal;
                            break;
                        case VAL_FUTURE:
                            // Compare by identity (pointer equality)
                            isEqual = left.futureVal == right.futureVal;
                            break;
                    }
                }
                
                result.boolVal = (node->binary.op.type == TOKEN_EQUAL_EQUAL) ? isEqual : !isEqual;
                return result;
            }
            
            // Numeric operations
            // Coerce 1-char strings to ints for arithmetic if needed
            if ((left.type == VAL_STRING && right.type != VAL_STRING) || (right.type == VAL_STRING && left.type != VAL_STRING)) {
                int code;
                if (left.type == VAL_STRING && tryCharCode(left, &code)) { left.type = VAL_INT; left.intVal = code; }
                if (right.type == VAL_STRING && tryCharCode(right, &code)) { right.type = VAL_INT; right.intVal = code; }
            } else if (left.type == VAL_STRING && right.type == VAL_STRING) {
                // If both are strings, try to coerce both when operator is not string concatenation
                int lc, rc;
                if (tryCharCode(left, &lc) && tryCharCode(right, &rc)) {
                    left.type = VAL_INT; left.intVal = lc;
                    right.type = VAL_INT; right.intVal = rc;
                }
            }

            if (left.type == VAL_INT && right.type == VAL_INT) {
                result.type = VAL_INT;
                switch (node->binary.op.type) {
                    case TOKEN_PLUS: 
                        result.intVal = left.intVal + right.intVal; 
                        break;
                    case TOKEN_MINUS: 
                        result.intVal = left.intVal - right.intVal; 
                        break;
                    case TOKEN_STAR: 
                        result.intVal = left.intVal * right.intVal; 
                        break;
                    case TOKEN_SLASH: 
                        if (right.intVal == 0) {
                            error("Division by zero.", node->binary.op.line);
                        }
                        result.intVal = left.intVal / right.intVal; 
                        break;
                    case TOKEN_PERCENT: 
                        if (right.intVal == 0) {
                            error("Modulo by zero.", node->binary.op.line);
                        }
                        result.intVal = left.intVal % right.intVal; 
                        break;
                    case TOKEN_GREATER: 
                        result.type = VAL_BOOL;
                        result.boolVal = left.intVal > right.intVal; 
                        break;
                    case TOKEN_GREATER_EQUAL: 
                        result.type = VAL_BOOL;
                        result.boolVal = left.intVal >= right.intVal; 
                        break;
                    case TOKEN_LESS: 
                        result.type = VAL_BOOL;
                        result.boolVal = left.intVal < right.intVal; 
                        break;
                    case TOKEN_LESS_EQUAL: 
                        result.type = VAL_BOOL;
                        result.boolVal = left.intVal <= right.intVal; 
                        break;
                    default: 
                        error("Invalid binary operator for integers.", node->binary.op.line);
                }
            } else if (left.type == VAL_FLOAT && right.type == VAL_FLOAT) {
                // Handle float operations
                switch (node->binary.op.type) {
                    case TOKEN_PLUS: 
                        result.type = VAL_FLOAT;
                        result.floatVal = left.floatVal + right.floatVal; 
                        break;
                    case TOKEN_MINUS: 
                        result.type = VAL_FLOAT;
                        result.floatVal = left.floatVal - right.floatVal; 
                        break;
                    case TOKEN_STAR: 
                        result.type = VAL_FLOAT;
                        result.floatVal = left.floatVal * right.floatVal; 
                        break;
                    case TOKEN_SLASH: 
                        if (right.floatVal == 0.0) {
                            error("Division by zero.", node->binary.op.line);
                        }
                        result.type = VAL_FLOAT;
                        result.floatVal = left.floatVal / right.floatVal; 
                        break;
                    case TOKEN_GREATER: 
                        result.type = VAL_BOOL;
                        result.boolVal = left.floatVal > right.floatVal; 
                        break;
                    case TOKEN_GREATER_EQUAL: 
                        result.type = VAL_BOOL;
                        result.boolVal = left.floatVal >= right.floatVal; 
                        break;
                    case TOKEN_LESS: 
                        result.type = VAL_BOOL;
                        result.boolVal = left.floatVal < right.floatVal; 
                        break;
                    case TOKEN_LESS_EQUAL: 
                        result.type = VAL_BOOL;
                        result.boolVal = left.floatVal <= right.floatVal; 
                        break;
                    default: 
                        error("Invalid binary operator for floats.", node->binary.op.line);
                }
            } else if ((left.type == VAL_INT && right.type == VAL_FLOAT) || 
                       (left.type == VAL_FLOAT && right.type == VAL_INT)) {
                // Mixed int/float operations - convert to float
                double leftVal = (left.type == VAL_INT) ? (double)left.intVal : left.floatVal;
                double rightVal = (right.type == VAL_INT) ? (double)right.intVal : right.floatVal;
                
                switch (node->binary.op.type) {
                    case TOKEN_PLUS: 
                        result.type = VAL_FLOAT;
                        result.floatVal = leftVal + rightVal; 
                        break;
                    case TOKEN_MINUS: 
                        result.type = VAL_FLOAT;
                        result.floatVal = leftVal - rightVal; 
                        break;
                    case TOKEN_STAR: 
                        result.type = VAL_FLOAT;
                        result.floatVal = leftVal * rightVal; 
                        break;
                    case TOKEN_SLASH: 
                        if (rightVal == 0.0) {
                            error("Division by zero.", node->binary.op.line);
                        }
                        result.type = VAL_FLOAT;
                        result.floatVal = leftVal / rightVal; 
                        break;
                    case TOKEN_GREATER: 
                        result.type = VAL_BOOL;
                        result.boolVal = leftVal > rightVal; 
                        break;
                    case TOKEN_GREATER_EQUAL: 
                        result.type = VAL_BOOL;
                        result.boolVal = leftVal >= rightVal; 
                        break;
                    case TOKEN_LESS: 
                        result.type = VAL_BOOL;
                        result.boolVal = leftVal < rightVal; 
                        break;
                    case TOKEN_LESS_EQUAL: 
                        result.type = VAL_BOOL;
                        result.boolVal = leftVal <= rightVal; 
                        break;
                    default: 
                        error("Invalid binary operator for mixed numeric types.", node->binary.op.line);
                }
            } else {
                error("Type mismatch in binary operation.", node->binary.op.line);
            }
            
            return result;
        }
        
        case NODE_EXPR_CALL: {
            // Resolve function from callee expression
            Function* func = NULL;
            int errLine = 0;
            if (node->call.callee->type == NODE_EXPR_VAR) {
                // Try global functions first
                func = findFunction(vm, node->call.callee->var.name);
                // Then try functions in current definition environment (e.g., current module)
                if (!func && vm->defEnv) {
                    func = findFunctionInEnv(vm->defEnv, node->call.callee->var.name);
                }
                errLine = node->call.callee->var.name.line;
                // Built-ins: intercept by name if not found as user function
                if (!func) {
                    // name buffer
                    char fname[64]; int flen = node->call.callee->var.name.length;
                    if (flen >= (int)sizeof(fname)) flen = (int)sizeof(fname) - 1;
                    memcpy(fname, node->call.callee->var.name.start, flen); fname[flen] = '\0';

                    // Evaluate arguments first (max 16 as below)
                    Value args[16]; int argCount = 0; Node* argN = node->call.arguments;
                    while (argN && argCount < 16) { args[argCount++] = evaluate(vm, argN); argN = argN->next; }
                    if (argCount >= 16 && argN) { error("Too many arguments (max 16).", errLine); }

                    // array()
                    if (strcmp(fname, "array") == 0) {
                        if (argCount != 0) error("array() takes 0 arguments.", errLine);
                        Value v; v.type = VAL_ARRAY; v.arrayVal = newArray(); return v;
                    }
                    // map()
                    if (strcmp(fname, "map") == 0) {
                        if (argCount != 0) error("map() takes 0 arguments.", errLine);
                        Value v; v.type = VAL_MAP; v.mapVal = newMap(); return v;
                    }
                    // length(x)
                    if (strcmp(fname, "length") == 0) {
                        if (argCount != 1) error("length(x) takes 1 argument.", errLine);
                        Value v; v.type = VAL_INT; v.intVal = 0;
                        if (args[0].type == VAL_STRING) v.intVal = args[0].stringVal ? (int)strlen(args[0].stringVal) : 0;
                        else if (args[0].type == VAL_ARRAY) v.intVal = args[0].arrayVal ? args[0].arrayVal->count : 0;
                        else if (args[0].type == VAL_MAP) {
                            int sz = 0; if (args[0].mapVal) { for (int i = 0; i < TABLE_SIZE; i++) { MapEntry* e = args[0].mapVal->buckets[i]; while (e) { sz++; e = e->next; } } }
                            v.intVal = sz;
                        } else error("length() unsupported type.", errLine);
                        return v;
                    }
                    // push(a, v) -> returns new length
                    if (strcmp(fname, "push") == 0) {
                        if (argCount != 2) error("push(a, v) takes 2 arguments.", errLine);
                        if (args[0].type != VAL_ARRAY || !args[0].arrayVal) error("push() requires array as first arg.", errLine);
                        arrayPush(args[0].arrayVal, args[1]);
                        Value v; v.type = VAL_INT; v.intVal = args[0].arrayVal->count; return v;
                    }
                    // pop(a) -> returns popped value
                    if (strcmp(fname, "pop") == 0) {
                        if (argCount != 1) error("pop(a) takes 1 argument.", errLine);
                        if (args[0].type != VAL_ARRAY || !args[0].arrayVal) error("pop() requires array.", errLine);
                        Value out; if (!arrayPop(args[0].arrayVal, &out)) error("pop() on empty array.", errLine);
                        return out;
                    }
                    // has(m, k) -> bool
                    if (strcmp(fname, "has") == 0) {
                        if (argCount != 2) error("has(m, k) takes 2 arguments.", errLine);
                        if (args[0].type != VAL_MAP || !args[0].mapVal) error("has() requires map.", errLine);
                        bool present = false;
                        if (args[1].type == VAL_INT) { present = mapFindEntryInt(args[0].mapVal, args[1].intVal, NULL) != NULL; }
                        else if (args[1].type == VAL_STRING) { const char* s = args[1].stringVal ? args[1].stringVal : ""; present = mapFindEntry(args[0].mapVal, s, (int)strlen(s), NULL) != NULL; }
                        else error("has() key must be int or string.", errLine);
                        Value v; v.type = VAL_BOOL; v.boolVal = present; return v;
                    }
                    // delete(m, k) -> bool (true if removed)
                    if (strcmp(fname, "delete") == 0) {
                        if (argCount != 2) error("delete(m, k) takes 2 arguments.", errLine);
                        if (args[0].type != VAL_MAP || !args[0].mapVal) error("delete() requires map.", errLine);
                        bool removed = false;
                        if (args[1].type == VAL_INT) removed = mapDeleteInt(args[0].mapVal, args[1].intVal);
                        else if (args[1].type == VAL_STRING) { const char* s = args[1].stringVal ? args[1].stringVal : ""; removed = mapDeleteStr(args[0].mapVal, s, (int)strlen(s)); }
                        else error("delete() key must be int or string.", errLine);
                        Value v; v.type = VAL_BOOL; v.boolVal = removed; return v;
                    }
                    // keys(m) -> array of string keys (int keys converted to decimal strings)
                    if (strcmp(fname, "keys") == 0) {
                        if (argCount != 1) error("keys(m) takes 1 argument.", errLine);
                        if (args[0].type != VAL_MAP || !args[0].mapVal) error("keys() requires map.", errLine);
                        Array* arr = newArray();
                        for (int i = 0; i < TABLE_SIZE; i++) {
                            MapEntry* e = args[0].mapVal->buckets[i];
                            while (e) {
                                Value sv; sv.type = VAL_STRING;
                                if (e->isIntKey) {
                                    char buf[32]; snprintf(buf, sizeof(buf), "%d", e->intKey);
                                    sv.stringVal = strdup(buf); if (!sv.stringVal) error("Memory allocation failed.", errLine);
                                } else {
                                    sv.stringVal = strdup(e->key ? e->key : ""); if (!sv.stringVal) error("Memory allocation failed.", errLine);
                                }
                                arrayPush(arr, sv);
                                e = e->next;
                            }
                        }
                        Value v; v.type = VAL_ARRAY; v.arrayVal = arr; return v;
                    }
                    // sleepAsync(ms) -> Future that resolves after ms milliseconds
                    if (strcmp(fname, "sleepAsync") == 0) {
                        if (argCount != 1 || args[0].type != VAL_INT) error("sleepAsync(ms) takes 1 int argument.", errLine);
                        Future* f = futureNew();
                        SleepArgs* sa = (SleepArgs*)malloc(sizeof(SleepArgs));
                        if (!sa) error("Memory allocation failed.", errLine);
                        sa->f = f; sa->ms = args[0].intVal;
                        pthread_t tid; pthread_create(&tid, NULL, sleepThread, sa); pthread_detach(tid);
                        Value fv; fv.type = VAL_FUTURE; fv.futureVal = f; return fv;
                    }
                }
            } else if (node->call.callee->type == NODE_EXPR_GET) {
                Value obj = evaluate(vm, node->call.callee->get.object);
                if (obj.type == VAL_MODULE) {
                    func = findFunctionInEnv(obj.moduleVal->env, node->call.callee->get.name);
                    errLine = node->call.callee->get.name.line;
                } else {
                    error("Only modules support method calls.", node->call.callee->get.name.line);
                }
            } else {
                error("Invalid call target.", 0);
            }
            if (!func) {
                error("Undefined function.", errLine);
                Value nullVal = {VAL_INT, .intVal = 0};
                return nullVal;
            }

            // Evaluate arguments
            Value args[16];
            int argCount = 0;
            Node* arg = node->call.arguments;
            while (arg && argCount < 16) {
                args[argCount++] = evaluate(vm, arg);
                arg = arg->next;
            }
            if (argCount >= 16 && arg) {
                error("Too many arguments (max 16).", errLine);
            }
            return callFunction(vm, func, args, argCount);
        }
        case NODE_EXPR_AWAIT: {
            Value v = evaluate(vm, node->unary.expr);
            if (v.type == VAL_FUTURE && v.futureVal) {
                return futureAwait(v.futureVal);
            }
            // Not a future: passthrough
            return v;
        }
        case NODE_EXPR_GET: {
            Value obj = evaluate(vm, node->get.object);
            // String property: length
            if (obj.type == VAL_STRING) {
                if (strncmp(node->get.name.start, "length", node->get.name.length) == 0 && strlen("length") == (size_t)node->get.name.length) {
                    Value v; v.type = VAL_INT; v.intVal = obj.stringVal ? (int)strlen(obj.stringVal) : 0; return v;
                }
                error("Unknown string property.", node->get.name.line);
            } else if (obj.type == VAL_MODULE) {
                // module constant/variable or function name as value isn't supported; only variables returned.
                VarEntry* ve = findVarInEnv(obj.moduleVal->env, node->get.name);
                if (ve) return ve->value;
                // If not a variable, allow chained call to resolve function. Here return int 0 to keep flow if used wrongly.
                error("Unknown module member.", node->get.name.line);
            } else {
                error("Property access not supported on this type.", node->get.name.line);
            }
            Value nullVal = {VAL_INT, .intVal = 0};
            return nullVal;
        }
        case NODE_EXPR_INDEX: {
            Value target = evaluate(vm, node->index.target);
            Value idx = evaluate(vm, node->index.index);
            if (target.type == VAL_STRING && idx.type == VAL_INT) {
                int len = target.stringVal ? (int)strlen(target.stringVal) : 0;
                if (idx.intVal < 0 || idx.intVal >= len) {
                    error("String index out of range.", 0);
                }
                char* s = malloc(2);
                if (!s) error("Memory allocation failed.", 0);
                s[0] = target.stringVal[idx.intVal];
                s[1] = '\0';
                Value v; v.type = VAL_STRING; v.stringVal = s; return v;
            } else if (target.type == VAL_ARRAY && idx.type == VAL_INT) {
                if (!target.arrayVal) { Value v; v.type = VAL_INT; v.intVal = 0; return v; }
                if (idx.intVal < 0 || idx.intVal >= target.arrayVal->count) error("Array index out of range.", 0);
                return target.arrayVal->items[idx.intVal];
            } else if (target.type == VAL_MAP) {
                if (!target.mapVal) { Value v; v.type = VAL_INT; v.intVal = 0; return v; }
                if (idx.type == VAL_INT) {
                    MapEntry* e = mapFindEntryInt(target.mapVal, idx.intVal, NULL);
                    if (!e) error("Map key not found.", 0);
                    return e->value;
                } else if (idx.type == VAL_STRING) {
                    const char* s = idx.stringVal ? idx.stringVal : "";
                    MapEntry* e = mapFindEntry(target.mapVal, s, (int)strlen(s), NULL);
                    if (!e) error("Map key not found.", 0);
                    return e->value;
                } else {
                    error("Map index must be int or string.", 0);
                }
            }
            error("Indexing not supported for this type.", 0);
            Value nullVal = {VAL_INT, .intVal = 0};
            return nullVal;
        }

        case NODE_EXPR_ARRAY_LITERAL: {
            Array* arr = newArray();
            Node* elem = node->arrayLiteral.elements;
            while (elem) {
                Value v = evaluate(vm, elem);
                arrayPush(arr, v);
                elem = elem->next;
            }
            Value v;
            v.type = VAL_ARRAY;
            v.arrayVal = arr;
            return v;
        }
        
        default:
            error("Invalid expression type.", 0);
            Value nullVal = {VAL_INT, .intVal = 0};
            return nullVal;
    }
}

// Initialize VM
void initVM(VM* vm) {
    vm->stackTop = 0;
    vm->callStackTop = 0;
    
    // Create global environment
    vm->globalEnv = malloc(sizeof(Environment));
    if (!vm->globalEnv) error("Memory allocation failed.", 0);
    memset(vm->globalEnv->buckets, 0, sizeof(vm->globalEnv->buckets));
    memset(vm->globalEnv->funcBuckets, 0, sizeof(vm->globalEnv->funcBuckets));
    memset(vm->moduleBuckets, 0, sizeof(vm->moduleBuckets));
    
    // Set current environment to global initially
    vm->env = vm->globalEnv;
    vm->defEnv = vm->globalEnv;
    // Set project root from current working directory
    if (!getcwd(vm->projectRoot, sizeof(vm->projectRoot))) {
        strcpy(vm->projectRoot, ".");
    }
    // Initialize extern handles
    vm->externHandleCount = 0;
    for (int i = 0; i < TABLE_SIZE; i++) vm->externHandles[i] = NULL;
    
    // Initialize string pool for performance optimization
    vm->stringPool.strings = malloc(64 * sizeof(char*));
    vm->stringPool.hashes = malloc(64 * sizeof(unsigned int));
    vm->stringPool.count = 0;
    vm->stringPool.capacity = 64;
    if (!vm->stringPool.strings || !vm->stringPool.hashes) {
        error("Memory allocation failed for string pool.", 0);
    }
    
    // Initialize value pool for basic types
    vm->valuePool.capacity = 128;
    vm->valuePool.values = malloc(vm->valuePool.capacity * sizeof(Value));
    vm->valuePool.free_list = malloc(vm->valuePool.capacity * sizeof(int));
    vm->valuePool.next_free = 0;
    if (!vm->valuePool.values || !vm->valuePool.free_list) {
        error("Memory allocation failed for value pool.", 0);
    }
    // Initialize free list
    for (int i = 0; i < vm->valuePool.capacity - 1; i++) {
        vm->valuePool.free_list[i] = i + 1;
    }
    vm->valuePool.free_list[vm->valuePool.capacity - 1] = -1;
}

// Free VM memory
void freeVM(VM* vm) {
    if (!vm) return;
    // Close external libraries
    for (int i = 0; i < vm->externHandleCount; i++) {
        if (vm->externHandles[i]) dlclose(vm->externHandles[i]);
        vm->externHandles[i] = NULL;
    }
    vm->externHandleCount = 0;
    
    // Free string pool
    if (vm->stringPool.strings) {
        for (int i = 0; i < vm->stringPool.count; i++) {
            free(vm->stringPool.strings[i]);
        }
        free(vm->stringPool.strings);
        free(vm->stringPool.hashes);
    }
    
    // Free value pool
    if (vm->valuePool.values) {
        free(vm->valuePool.values);
        free(vm->valuePool.free_list);
    }
    
    // Existing cleanup is intentionally minimal due to ownership complexities.
}

// Interpret AST
void interpret(VM* vm, Node* ast) {
    execute(vm, ast);
}