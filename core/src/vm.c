#include "lexer.h"
#include "parser.h"
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "vm.h"
#include "resolver.h"
#include <dlfcn.h>
#include <time.h>
#include <math.h>
#include <stdint.h>

// TABLE_SIZE and CALL_STACK_MAX are in vm.h

// Local Future struct definition
// Local Future struct definition removed (moved to vm.h)

// Forward declarations for Future helpers used by sleep thread
static Future* futureNew(VM* vm);
static void futureResolve(Future* f, Value v);
static Value futureAwait(Future* f);
// static bool futureIsDone(Future* f);

// Args and thread routine for sleepAsync
typedef struct { Future* f; int ms; } SleepArgs;
/*
static void* sleepThread(void* p) {
    long ms = (long)p;
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, NULL);
    return NULL;
}
*/


// ---- Future helpers (blocking await) ----
static Future* futureNew(VM* vm) {
    Future* f = ALLOCATE_OBJ(vm, Future, OBJ_FUTURE);
    f->done = false;
    // Initialize result to 0 int
    f->result = INT_VAL(0);
    // Reinit mutex/cv
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

/*
static bool futureIsDone(Future* f) {
    if (!f) return true;
    bool done = false;
    pthread_mutex_lock(&f->mutex);
    done = f->completed;
    pthread_mutex_unlock(&f->mutex);
    return done;
}
*/


// Optimized FNV-1a hash function for variables (faster than previous implementation)
unsigned int hash(const char* key, int length) {
    unsigned int hash = 2166136261u;
    const unsigned char* data = (const unsigned char*)key;
    for (int i = 0; i < length; i++) {
        hash ^= data[i];
        hash *= 16777619;
    }
    return hash % TABLE_SIZE;
}

// Set script directory from script path
void setScriptDir(VM* vm, const char* scriptPath) {
    if (!scriptPath) {
        vm->scriptDir[0] = '\0';
        return;
    }
    
    // Find last slash
    const char* lastSlash = strrchr(scriptPath, '/');
    if (lastSlash) {
        size_t dirLen = lastSlash - scriptPath;
        if (dirLen >= sizeof(vm->scriptDir)) {
            dirLen = sizeof(vm->scriptDir) - 1;
        }
        strncpy(vm->scriptDir, scriptPath, dirLen);
        vm->scriptDir[dirLen] = '\0';
    } else {
        // No slash, script is in current directory
        strcpy(vm->scriptDir, ".");
    }
}

// Resolve path relative to script directory
// Returns newly allocated string - caller must free()
char* resolvePath(VM* vm, const char* path) {
    if (!path) return NULL;
    
    // Strict Mode: ALL paths are relative to scriptDir.
    // e.g. "/etc/passwd" -> "<scriptDir>/etc/passwd"
    
    // If scriptDir is empty or ".", return path as-is
    if (vm->scriptDir[0] == '\0' || 
        (vm->scriptDir[0] == '.' && vm->scriptDir[1] == '\0')) {
        return strdup(path);
    }
    
    // Build scriptDir + "/" + path
    // Remove leading slash from path if present to avoid path confusion
    const char* p = path;
    if (p[0] == '/') p++;
    
    size_t dirLen = strlen(vm->scriptDir);
    size_t pathLen = strlen(p);
    size_t totalLen = dirLen + 1 + pathLen + 1; // +1 for slash, +1 for null
    
    char* result = malloc(totalLen);
    if (!result) return strdup(path);
    
    snprintf(result, totalLen, "%s/%s", vm->scriptDir, p);
    return result;
}

// ==========================
// GC & Memory Management
// ==========================

#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

// Forward declarations
/* GC Functions moved to gc.c */

Obj* allocateObject(VM* vm, size_t size, ObjType type) {
    Obj* object = (Obj*)reallocate(vm, NULL, 0, size);
    object->type = type;
    object->isMarked = (vm->gcPhase == 1 || isGCActive()); // Allocate Black during Marking to prevent Stack leaks
    object->isPermanent = false; // Default: subject to GC
    object->generation = 0;
    
    // Generational allocation: add to nursery
    object->next = vm->nursery;
    vm->nursery = object;
    vm->nurseryCount++;
    
    return object;
}

/* markObject moved to gc.c */

/* markValue moved to gc.c */

/* markArray moved to gc.c */

/* markEnvironment moved to gc.c */

/* blackenObject moved to gc.c */

/* traceReferences moved to gc.c */

/* freeObject moved to gc.c */

/* sweep moved to gc.c */

/* markRoots moved to gc.c */

/* collectGarbage moved to gc.c */

// reallocate, markObject, markValue, collectGarbage removed from here.

void freeVM(VM* vm) {
    if (!vm) return;
    
    // Close external libraries
    for (int i = 0; i < vm->externHandleCount; i++) {
        if (vm->externHandles[i]) dlclose(vm->externHandles[i]);
        vm->externHandles[i] = NULL;
    }
    vm->externHandleCount = 0;



    if (vm->stringPool.strings) {
         // Strings are managed as ObjStrings and will be freed by the object loop
         free(vm->stringPool.strings);
         free(vm->stringPool.hashes);
         pthread_mutex_destroy(&vm->stringPool.lock);
    }
    
    // Free all objects (Old Gen)
    Obj* object = vm->objects;
    while (object != NULL) {
        Obj* next = object->next;
        freeObject(vm, object);
        object = next;
    }
    
    // Free nursery objects (Young Gen)
    object = vm->nursery;
    while (object != NULL) {
        Obj* next = object->next;
        freeObject(vm, object);
        object = next;
    }
    
    // Free gray stack
    if (vm->grayStack) free(vm->grayStack);
    
     // Free value pool
    if (vm->valuePool.values) {
        free(vm->valuePool.values);
        free(vm->valuePool.free_list);
    }
}
// Helper to check truthiness
static bool isTruthy(Value v) {
    if (IS_NIL(v)) return false;
    if (IS_BOOL(v)) return AS_BOOL(v);
    if (IS_INT(v)) return AS_INT(v) != 0;
    if (IS_FLOAT(v)) return AS_FLOAT(v) != 0.0;
    
    if (IS_OBJ(v)) {
        Obj* o = AS_OBJ(v);
        if (o->type == OBJ_STRING) return ((ObjString*)o)->length > 0;
        // Arrays/Maps: empty? strict?
        // Let's say all objects are true for now to match typical dynamic langs, or specific check.
        // Unnarize original logic: true.
        return true;
    }
    return true; 
}

static unsigned int hashIntKey(int k) {
    unsigned int x = (unsigned int)k;
    x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16;
    return x % TABLE_SIZE;
}

// Pointer hash for fast lookups (O(1))
static unsigned int hashPointer(const void* ptr) {
    uintptr_t x = (uintptr_t)ptr;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    return x % TABLE_SIZE;
}

ObjString* internString(VM* vm, const char* str, int length) {
    if (!str) return NULL;
    if (length <= 0) length = 0;

    // Long strings are NOT interned: they live only on the GC heap and are
    // collected as soon as they become unreachable. This prevents the pool
    // from pinning megabytes of intermediate concatenation results.
    if (length > 256) {
        ObjString* strObj = ALLOCATE_OBJ(vm, ObjString, OBJ_STRING);
        strObj->length = length;
        strObj->hash   = hash(str, length);
        strObj->chars  = malloc(length + 1);
        memcpy(strObj->chars, str, length);
        strObj->chars[length] = '\0';
        return strObj;
    }

    unsigned int h = hash(str, length);

    // Step 1: Find existing match (Lock)
    pthread_mutex_lock(&vm->stringPool.lock);
    for (int i = 0; i < vm->stringPool.count; i++) {
        ObjString* strObj = (ObjString*)vm->stringPool.strings[i];
         if (strObj->hash == h && strObj->length == length && memcmp(strObj->chars, str, length) == 0) {
             pthread_mutex_unlock(&vm->stringPool.lock);
             return strObj;
         }
    }
    pthread_mutex_unlock(&vm->stringPool.lock);
    
    // Step 2: Create new (No Lock, might trigger GC)
    ObjString* strObj = ALLOCATE_OBJ(vm, ObjString, OBJ_STRING);
    strObj->length = length;
    strObj->hash = h;
    strObj->chars = malloc(length + 1);
    memcpy(strObj->chars, str, length);
    strObj->chars[length] = '\0';
    
    // Step 3: Add to pool (Lock again)
    pthread_mutex_lock(&vm->stringPool.lock);
    
    // Double Check (in case another thread inserted it while we were allocating)
    for (int i = 0; i < vm->stringPool.count; i++) {
        ObjString* existing = (ObjString*)vm->stringPool.strings[i];
         if (existing->hash == h && existing->length == length && memcmp(existing->chars, str, length) == 0) {
             pthread_mutex_unlock(&vm->stringPool.lock);
             // Lost the race — abandon our duplicate; GC will collect it.
             return existing;
         }
    }

    if (vm->stringPool.count == vm->stringPool.capacity) {
        int oldCap = vm->stringPool.capacity;
        vm->stringPool.capacity = oldCap < 8 ? 8 : oldCap * 2;
        vm->stringPool.strings = (char**)realloc(vm->stringPool.strings, sizeof(char*) * vm->stringPool.capacity);
        vm->stringPool.hashes = (unsigned int*)realloc(vm->stringPool.hashes, sizeof(unsigned int) * vm->stringPool.capacity);
        if (!vm->stringPool.strings || !vm->stringPool.hashes) error("Memory allocation failed for string pool.", 0);
    }
    vm->stringPool.strings[vm->stringPool.count] = (char*)strObj;
    vm->stringPool.hashes[vm->stringPool.count] = h;
    vm->stringPool.count++;
    pthread_mutex_unlock(&vm->stringPool.lock);
    
    return strObj;
}


// Basic helper to intern a token's valid string range
static void internToken(VM* vm, Token* token) {
    if (token->length > 0) {
        // Intern the token string content
        ObjString* internedObj = internString(vm, token->start, token->length);
        char* interned = internedObj->chars;
        // DANGEROUS CAST: We are modifying the AST in place.
        // This is safe because we own the AST and it's read-only execution after parsing.
        ((Token*)token)->start = interned;
    }
}

// Recursively intern all identifiers and string literals in AST
static void internAST(VM* vm, Node* node) {
    if (!node) return;
    
    switch (node->type) {
        case NODE_EXPR_LITERAL:
            if (node->literal.token.type == TOKEN_STRING) internToken(vm, &node->literal.token);
            break;
        case NODE_EXPR_BINARY:
            internAST(vm, node->binary.left);
            internAST(vm, node->binary.right);
            break;
        case NODE_EXPR_UNARY:
            internAST(vm, node->unary.expr);
            break;
        case NODE_EXPR_VAR:
            internToken(vm, &node->var.name);
            break;
        case NODE_EXPR_CALL:
            internAST(vm, node->call.callee);
            internAST(vm, node->call.arguments);
            break;
        case NODE_EXPR_GET:
            internAST(vm, node->get.object);
            internToken(vm, &node->get.name);
            break;
        case NODE_EXPR_INDEX:
            internAST(vm, node->index.target);
            internAST(vm, node->index.index);
            break;
        case NODE_STMT_VAR_DECL:
            internToken(vm, &node->varDecl.name);
            internAST(vm, node->varDecl.initializer);
            break;
        case NODE_STMT_ASSIGN:
            internToken(vm, &node->assign.name);
            internAST(vm, node->assign.value);
            break;
        case NODE_STMT_INDEX_ASSIGN:
            internAST(vm, node->indexAssign.target);
            internAST(vm, node->indexAssign.index);
            internAST(vm, node->indexAssign.value);
            break;
        case NODE_STMT_PRINT:
            internAST(vm, node->print.expr);
            break;
        case NODE_STMT_IF:
            internAST(vm, node->ifStmt.condition);
            internAST(vm, node->ifStmt.thenBranch);
            internAST(vm, node->ifStmt.elseBranch);
            break;
        case NODE_STMT_WHILE:
            internAST(vm, node->whileStmt.condition);
            internAST(vm, node->whileStmt.body);
            break;
        case NODE_STMT_FOR:
            internAST(vm, node->forStmt.initializer);
            internAST(vm, node->forStmt.condition);
            internAST(vm, node->forStmt.increment);
            internAST(vm, node->forStmt.body);
            break;
        case NODE_STMT_BLOCK:
            for (int i = 0; i < node->block.count; i++) {
                internAST(vm, node->block.statements[i]);
            }
            break;
        case NODE_STMT_FUNCTION:
            internToken(vm, &node->function.name);
            for (int i = 0; i < node->function.paramCount; i++) {
                internToken(vm, &node->function.params[i]);
            }
            internAST(vm, node->function.body);
            break;
        case NODE_STMT_RETURN:
            internAST(vm, node->returnStmt.value);
            break;
        case NODE_STMT_IMPORT:
            internToken(vm, &node->importStmt.module);
            internToken(vm, &node->importStmt.alias);
            break;
        case NODE_EXPR_ARRAY_LITERAL:
            internAST(vm, node->arrayLiteral.elements);
            break;
        case NODE_STMT_FOREACH:
            internToken(vm, &node->foreachStmt.iterator);
            internAST(vm, node->foreachStmt.collection);
            internAST(vm, node->foreachStmt.body);
            break;
        case NODE_STMT_STRUCT_DECL:
            internToken(vm, &node->structDecl.name);
            for (int i = 0; i < node->structDecl.fieldCount; i++) {
                internToken(vm, &node->structDecl.fields[i]);
            }
            break;
        case NODE_STMT_PROP_ASSIGN:
            internAST(vm, node->propAssign.object);
            internToken(vm, &node->propAssign.name);
            internAST(vm, node->propAssign.value);
            break;
        default:
            break;
    }
    internAST(vm, node->next);
}

// Module cache lookup/insert by logical module name
/*
static ModuleEntry* findModuleEntry(VM* vm, const char* name, int length, bool insert) {
     unsigned int h = hashPointer(name) % TABLE_SIZE; // Assumes name is interned?
     // Actually name might not be interned if searching.
     // But for module registry, we typically intern keys.
     // However, simpler hash:
     // unsigned int h = hash(name, length) % TABLE_SIZE;
     // vm->modules is Simple map?
     // Let's rely on string interning for modules too?
     return NULL;
}
*/


// Check if regular file exists
/*
static bool fileExists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}
*/


// Find or insert variable with proper scope resolution
static VarEntry* findEntry(VM* vm, Token name, bool insert) {
    // Assumption: name.start IS interned via internAST
    unsigned int h = hashPointer(name.start) % TABLE_SIZE;
    
    // If not inserting, search up the chain
    if (!insert) {
        Environment* env = vm->env;
        while (env) {
            VarEntry* entry = env->buckets[h];
            while (entry) {
                if (entry->key == name.start) return entry;
                entry = entry->next;
            }
            env = env->enclosing;
        }
        return NULL;
    }
    
    // If inserting, search only current environment
    VarEntry* entry = vm->env->buckets[h];
    while (entry) {
        if (entry->key == name.start) return entry;
        entry = entry->next;
    }
    
    // If inserting, create new entry in current environment
    if (insert) {
        entry = malloc(sizeof(VarEntry));
        if (!entry) {
            error("Memory allocation failed.", name.line);
        }
        
        entry->key = (char*)name.start; // Already interned
        entry->keyString = NULL; // Cannot get ObjString from char* here
        entry->keyLength = name.length;
        entry->ownsKey = false; // Owned by StringPool
        
        entry->value = INT_VAL(0); // Default init
        entry->next = vm->env->buckets[h];
        vm->env->buckets[h] = entry;
        return entry;
    }
    return NULL;
}

// Find or insert function in specific environment
static FuncEntry* findFuncEntry(Environment* env, Token name, bool insert) {
    // Assumption: name.start is interned
    unsigned int h = hashPointer(name.start) % TABLE_SIZE;
    FuncEntry* entry = env->funcBuckets[h];
    while (entry) {
        if (entry->key == name.start) {
            return entry;
        }
        entry = entry->next;
    }
    if (insert) {
        entry = malloc(sizeof(FuncEntry));
        if (!entry) {
            error("Memory allocation failed.", name.line);
        }
        entry->key = (char*)name.start; // Already interned
        entry->function = NULL;
        entry->next = env->funcBuckets[h];
        env->funcBuckets[h] = entry;
        return entry;
    }
    return NULL; // Not found
}



Function* findFunctionByName(VM* vm, const char* name) {
    // Intern key first to enable pointer lookup
    char* key = internString(vm, name, (int)strlen(name))->chars;
    unsigned int h = hashPointer(key) % TABLE_SIZE;
    FuncEntry* entry = vm->globalEnv->funcBuckets[h];
    while (entry) {
        if (entry->key == key) {
            return entry->function;
        }
        entry = entry->next;
    }
    return NULL;
}

static Function* findFunction(VM* vm, Token name) {
    // name.start is interned
    unsigned int h = hashPointer(name.start) % TABLE_SIZE;
    FuncEntry* entry = vm->globalEnv->funcBuckets[h];
    while (entry) {
        if (entry->key == name.start) {
            return entry->function;
        }
        entry = entry->next;
    }
    return NULL;
}

static Function* findFunctionInEnv(Environment* env, Token name) {
    unsigned int h = hashPointer(name.start) % TABLE_SIZE;
    FuncEntry* entry = env->funcBuckets[h];
    while (entry) {
        if (entry->key == name.start) {
            return entry->function;
        }
        entry = entry->next;
    }
    return NULL;
}

// Find variable in a specific environment
static VarEntry* findVarInEnv(Environment* env, Token name) {
    unsigned int h = hashPointer(name.start) % TABLE_SIZE;
    VarEntry* entry = env->buckets[h];
    while (entry) {
        if (entry->key == name.start) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}

// Read whole file
char* readFileAll(const char* path) {
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
/*
static char* searchFileRecursive(const char* root, const char* filename) {
    DIR* d = opendir(root);
    if (!d) return NULL;
    struct dirent* dir;
    char* found = NULL;
    while ((dir=readdir(d)) != NULL) {
         if (dir->d_type == DT_REG && strcmp(dir->d_name, filename)==0) {
             found = malloc(strlen(root)+strlen(filename)+2);
             sprintf(found, "%s/%s", root, filename);
             break;
         }
         if (dir->d_type == DT_DIR && strcmp(dir->d_name, ".")!=0 && strcmp(dir->d_name, "..")!=0) {
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s", root, dir->d_name);
            found = searchFileRecursive(path, filename);
            if (found) break;
         }
    }
    closedir(d);
    return found;
}
*/


// Convert 1-char string to int code if applicable
/*
static bool tryCharCode(Value v, int* out) {
    if (IS_INT(v)) { *out = v.intVal; return true; }
    return false; 
}
*/


// ---- Array helpers ----
// Helper to create a new array
Array* newArray(VM* vm) {
    Array* arr = ALLOCATE_OBJ(vm, Array, OBJ_ARRAY);
    arr->items = NULL;
    arr->count = 0;
    arr->capacity = 0;
    return arr;
}

void arrayPush(VM* vm, Array* a, Value v) {
    if (a->count + 1 > a->capacity) {
        int oldCapacity = a->capacity;
        a->capacity = GROW_CAPACITY(oldCapacity);
        Value* newItems = (Value*)reallocate(vm, a->items, sizeof(Value) * oldCapacity, sizeof(Value) * a->capacity);
        if (!newItems) {
            printf("Fatal Error: Array allocation failed.\n");
            exit(1);
        }
        a->items = newItems;
    }
    a->items[a->count++] = v;
}
/*
static bool arrayPop(Array* a, Value* out) {
    if (a->count == 0) return false;
    *out = a->items[--a->count];
    return true;
}
*/
bool arrayPop(Array* a, Value* out) {
    if (a->count == 0) return false;
    *out = a->items[a->count - 1];
    a->count--;
    return true;
}

// ---- Map helpers ----
Map* newMap(VM* vm) {
    Map* m = ALLOCATE_OBJ(vm, Map, OBJ_MAP);
    for (int i = 0; i < TABLE_SIZE; i++) m->buckets[i] = NULL;
    return m;
}
MapEntry* mapFindEntry(Map* m, const char* skey, int slen, int* bucketOut) {
    unsigned int h = hash(skey, slen);
    if (bucketOut) *bucketOut = (int)h;
    MapEntry* e = m->buckets[h];
    while (e) {
        if (!e->isIntKey && e->key && (int)strlen(e->key) == slen && strncmp(e->key, skey, slen) == 0) return e;
        e = e->next;
    }
    return NULL;
}
MapEntry* mapFindEntryInt(Map* m, int ikey, int* bucketOut) {
    unsigned int h = hashIntKey(ikey);
    if (bucketOut) *bucketOut = (int)h;
    MapEntry* e = m->buckets[h];
    while (e) {
        if (e->isIntKey && e->intKey == ikey) return e;
        e = e->next;
    }
    return NULL;
}
void mapSetStr(Map* m, const char* key, int len, Value v) {
    int b; MapEntry* e = mapFindEntry(m, key, len, &b);
    if (e) { e->value = v; return; }
    e = (MapEntry*)malloc(sizeof(MapEntry)); if (!e) error("Memory allocation failed.", 0);
    e->isIntKey = false; e->intKey = 0;
    e->key = strndup(key, len); if (!e->key) { free(e); error("Memory allocation failed.", 0);} 
    e->value = v; e->next = m->buckets[b]; m->buckets[b] = e;
}
void mapSetInt(Map* m, int ikey, Value v) {
    int b; MapEntry* e = mapFindEntryInt(m, ikey, &b);
    if (e) { e->value = v; return; }
    e = (MapEntry*)malloc(sizeof(MapEntry)); if (!e) error("Memory allocation failed.", 0);
    e->isIntKey = true; e->intKey = ikey; e->key = NULL; e->value = v; e->next = m->buckets[b]; m->buckets[b] = e;
}
/*
static bool mapDeleteInt(Map* m, int ikey) {
    unsigned int h = hashInt(ikey) % TABLE_SIZE;
    MapEntry* entry = m->buckets[h];
    MapEntry* prev = NULL;
    while (entry) {
        if (entry->isIntKey && entry->intKey == ikey) {
            if (prev) prev->next = entry->next;
            else m->buckets[h] = entry->next;
            free(entry);
            m->count--;
            return true;
        }
        prev = entry;
        entry = entry->next;
    }
    return false;
}
*/


/*
static bool mapDeleteStr(Map* m, const char* key, int len) {
    unsigned int h = hash(key, len) % TABLE_SIZE;
    // ... impl
    return false;
}
*/



// Forward declarations
static Value evaluate(VM* vm, Node* node);
static void execute(VM* vm, Node* node);
static void defineInEnv(VM* vm, Environment* env, Token name, Value value);

// Exposed registration API for external libraries
void registerNativeFunction(VM* vm, const char* name, Value (*function)(VM*, Value* args, int argCount)) {
    if (!vm || !name || !*name || !function) {
        error("registerNativeFunction: invalid arguments.", 0);
    }
    // Phase 2: Intern key for pointer lookup
    char* key = internString(vm, name, (int)strlen(name))->chars;
    
    Token t; t.type = TOKEN_IDENTIFIER; t.start = key; t.length = (int)strlen(key); t.line = 0;
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
        func->native = function;
    entry->function = func;
}

// Helper to call a function
Value callFunction(VM* vm, Function* func, Value* args, int argCount) {
    if (func->isNative) {
        // Direct call to native C function
        return func->native(vm, args, argCount);
    }
    if (vm->callStackTop >= CALL_STACK_MAX) {
        error("Call stack overflow.", 0);
    }
    
    // Debug depth
    // printf("Call depth: %d\n", vm->callStackTop);
    // printf("Calling %.*s\n", func->name.length, func->name.start);
    
    // Create new environment for function execution
    Environment* funcEnv = ALLOCATE_OBJ(vm, Environment, OBJ_ENVIRONMENT);
    funcEnv->enclosing = func->closure; // Lexical scoping from closure
    memset(funcEnv->buckets, 0, sizeof(funcEnv->buckets));
    memset(funcEnv->funcBuckets, 0, sizeof(funcEnv->funcBuckets));
    
    // Check parameter count
    if (argCount != func->paramCount) {
        char errorMsg[256];
        snprintf(errorMsg, sizeof(errorMsg), "Expected %d arguments but got %d.", func->paramCount, argCount);
        error(errorMsg, 0);
    }
    // Check stack overflow
    if (vm->stackTop + argCount + 64 > STACK_MAX) { // +64 safety margin
        error("Stack overflow.", func->name.line); 
        return NIL_VAL;
    }

    // Push arguments to stack (They become locals 0..N-1 for the new frame)
    int oldStackTop = vm->stackTop; // Save to restore later? No, return value replaces them
    for (int i = 0; i < argCount; i++) {
        vm->stack[vm->stackTop++] = args[i];
    }
    
    // Save current frame setup
    if (vm->callStackTop >= CALL_STACK_MAX) {
        error("Maximum call stack depth exceeded.", func->name.line); 
        return NIL_VAL; 
    }
    
    CallFrame* frame = &vm->callStack[vm->callStackTop++];
    frame->env = vm->env;
    frame->fp = vm->fp;
    frame->hasReturned = false;
    frame->returnValue = NIL_VAL;
    frame->function = func; // Root the function
    
    // Setup new frame
    vm->fp = oldStackTop; // New frame starts where arguments began
    vm->env = funcEnv;
    
    // Bind arguments to environment (Hybrid approach: Stack AND Env for now to support non-slot access)
    // Actually, if we use slots, we don't need Env entries for params?
    // Resolver assigns slots to params. 
    // We strictly put them on Stack. 
    // AND we probably should allow Env fallback for now to be safe with existing lookups?
    // Let's populate Env too just in case (redundant but safe for transition).
    if (func->params) {
        for (int i = 0; i < argCount; i++) {
             Token param = func->params[i];
             // Define in env (buckets)
             char buf[64];
             int len = param.length; if(len>63)len=63;
             memcpy(buf, param.start, len); buf[len]=0;
             
             ObjString* keyStrObj = internString(vm, buf, len);
             VarEntry* ve = malloc(sizeof(VarEntry));
             ve->key = keyStrObj->chars;
             ve->keyString = keyStrObj; // Store for GC marking
             ve->keyLength = len;
             ve->ownsKey = false;
             ve->value = args[i];
             ve->next = funcEnv->buckets[hashPointer(ve->key)%TABLE_SIZE];
             funcEnv->buckets[hashPointer(ve->key)%TABLE_SIZE] = ve;
        }
    }
    
    // Execute body
    execute(vm, func->body);
    
    // Capture return value
    Value ret = frame->returnValue;
    
    // Restore frame
    vm->env = frame->env;
    vm->fp = frame->fp;
    vm->stackTop = oldStackTop; // Pop args/locals
    vm->callStackTop--;
    
    // Environment is GC'd.
    
    // For async functions (MVP): wrap the result into an already-resolved Future
    if (func->isAsync) {
        Future* f = futureNew(vm);
        futureResolve(f, ret);
        Value v = OBJ_VAL(f); return v;
    }
    return ret;
}

// Helper for print (forward decl)


// printValue implementation
void printValue(Value val) {
    if (IS_NIL(val)) { printf("nil"); return; }
    switch (getValueType(val)) {
        case VAL_BOOL: printf(AS_BOOL(val) ? "true" : "false"); break;
        case VAL_INT: printf("%ld", (long)AS_INT(val)); break;
        case VAL_FLOAT: printf("%.6g", AS_FLOAT(val)); break;
        case VAL_OBJ: {
            Obj* o = AS_OBJ(val);
            if (IS_STRING(val)) {
                printf("%s", AS_CSTRING(val));
            } else if (o->type == OBJ_ARRAY) {
                // Print array contents
                Array* arr = (Array*)o;
                printf("[");
                for (int i = 0; i < arr->count; i++) {
                    if (i > 0) printf(", ");
                    printValue(arr->items[i]);
                }
                printf("]");
            } else if (o->type == OBJ_MAP) {
                printf("<map>");
            } else if (o->type == OBJ_FUNCTION) {
                 Function* f = (Function*)o;
                 if(f->name.start) printf("<fn %.*s>", f->name.length, f->name.start); 
                 else printf("<script>");
            } else {
                printf("<obj>");
            }
            break;
        }
        default: printf("<unknown>"); break;
    }
}
// Execute statement
static void execute(VM* vm, Node* node) {
    if (!node) {
        // error("Null statement.", 0);
        return;
    }
    
    switch (node->type) {
        case NODE_STMT_VAR_DECL: {
            Value val;
            if (node->varDecl.initializer) {
                val = evaluate(vm, node->varDecl.initializer);
            } else {
                val = NIL_VAL;
            }
            
            if (node->varDecl.slot != -1) {
                // Local variable
                vm->stack[vm->fp + node->varDecl.slot] = val;
                // Hybrid: also put in Env for closures
                defineInEnv(vm, vm->env, node->varDecl.name, val);
            } else {
                // Global variable
                defineGlobal(vm, node->varDecl.name.start, val);
            }
            break;
        }
        
        case NODE_STMT_ASSIGN: {
            Value val = evaluate(vm, node->assign.value);
            
            if (node->assign.slot != -1) {
                // Local variable
                if (node->assign.operator.type == TOKEN_EQUAL) {
                    vm->stack[vm->fp + node->assign.slot] = val;
                    // Hybrid: sync to Env if exists
                    VarEntry* e = findEntry(vm, node->assign.name, false); 
                    if (e) e->value = val;
                } else {
                    Value current = vm->stack[vm->fp + node->assign.slot];
                    Value newVal;
                    
                    int currentInt = (IS_INT(current)) ? AS_INT(current) : 0; 
                    int valInt = (IS_INT(val)) ? AS_INT(val) : 0;
                    
                    switch (node->assign.operator.type) {
                        case TOKEN_PLUS_EQUAL: newVal = INT_VAL(currentInt + valInt); break;
                        case TOKEN_MINUS_EQUAL: newVal = INT_VAL(currentInt - valInt); break;
                        case TOKEN_STAR_EQUAL: newVal = INT_VAL(currentInt * valInt); break;
                        case TOKEN_SLASH_EQUAL: newVal = INT_VAL(currentInt / (valInt ? valInt : 1)); break;
                        default: newVal = val; break;
                    }
                    vm->stack[vm->fp + node->assign.slot] = newVal;
                }
            } else {
                // Global variable
                if (node->assign.operator.type == TOKEN_EQUAL) {
                    defineGlobal(vm, node->assign.name.start, val);
                } else {
                     VarEntry* entry = findEntry(vm, node->assign.name, false);
                     if (!entry) {
                         // Try defineGlobal if it's implicitly allowed? No, += requires value.
                         // But if we want to be safe:
                         // error("Undefined variable.", 0);
                         // For now, assume 0/nil if not found?
                         // "Undefined variable" is better.
                         // But wait, findEntry checks current env chain.
                         // If we are in function, we might finding global.
                         // My generic resolveAST fix made top-level vars globals.
                     }
                     
                     Value current = entry ? entry->value : INT_VAL(0); 
                     Value newVal;
                     
                     int currentInt = (IS_INT(current)) ? AS_INT(current) : 0; 
                     int valInt = (IS_INT(val)) ? AS_INT(val) : 0;
                     
                     switch (node->assign.operator.type) {
                         case TOKEN_PLUS_EQUAL: newVal = INT_VAL(currentInt + valInt); break;
                         case TOKEN_MINUS_EQUAL: newVal = INT_VAL(currentInt - valInt); break;
                         case TOKEN_STAR_EQUAL: newVal = INT_VAL(currentInt * valInt); break;
                         case TOKEN_SLASH_EQUAL: newVal = INT_VAL(currentInt / (valInt ? valInt : 1)); break;
                         default: newVal = val; break;
                     }
                     defineGlobal(vm, node->assign.name.start, newVal);
                }
            }
            break;
        }
        
        case NODE_STMT_INDEX_ASSIGN: {
            Value target = evaluate(vm, node->indexAssign.target);
            Value idx = evaluate(vm, node->indexAssign.index);
            Value val = evaluate(vm, node->indexAssign.value);
            
            if (IS_ARRAY(target) && IS_INT(idx)) {
                Array* a = (Array*)AS_OBJ(target);
                if (AS_INT(idx) >= 0 && AS_INT(idx) < a->count) {
                    a->items[AS_INT(idx)] = val;
                } else {
                    error("Index out of bounds.", 0);
                }
            } else if (IS_MAP(target)) {
                Map* m = (Map*)AS_OBJ(target);
                if (IS_INT(idx)) {
                    mapSetInt(m, AS_INT(idx), val);
                } else if (IS_STRING(idx)) {
                    ObjString* s = (ObjString*)AS_OBJ(idx);
                    mapSetStr(m, s->chars, s->length, val);
                } else {
                    error("Invalid map key.", 0);
                }
            } else {
                error("Index assignment not supported.", 0);
            }
            break;
        }
        
        case NODE_STMT_PRINT: {
            Value val = evaluate(vm, node->print.expr);
            printValue(val);
            printf("\n");
            break;
        }
        
        case NODE_STMT_IF: {
            Value cond = evaluate(vm, node->ifStmt.condition);
            if (isTruthy(cond)) {
                execute(vm, node->ifStmt.thenBranch);
            } else if (node->ifStmt.elseBranch) {
                execute(vm, node->ifStmt.elseBranch);
            }
            break;
        }
        
        case NODE_STMT_WHILE: {
            while (isTruthy(evaluate(vm, node->whileStmt.condition))) {
                execute(vm, node->whileStmt.body);
            }
            break;
        }
        
        case NODE_STMT_FOR: {
            if (node->forStmt.initializer) execute(vm, node->forStmt.initializer);
            while (true) {
                if (node->forStmt.condition) {
                   if (!isTruthy(evaluate(vm, node->forStmt.condition))) break;
                }
                execute(vm, node->forStmt.body);
                if (node->forStmt.increment) execute(vm, node->forStmt.increment);
            }
            break;
        }
        
        case NODE_STMT_FOREACH: {
             Value collection = evaluate(vm, node->foreachStmt.collection);
             if (IS_ARRAY(collection)) {
                 Array* arr = (Array*)AS_OBJ(collection);
                 for (int i = 0; i < arr->count; i++) {
                     Value val = arr->items[i];
                     int slot = node->foreachStmt.slot;
                     if (slot != -1) {
                         vm->stack[vm->fp + slot] = val;
                         defineInEnv(vm, vm->env, node->foreachStmt.iterator, val);
                     } else {
                         defineGlobal(vm, node->foreachStmt.iterator.start, val);
                     }
                     execute(vm, node->foreachStmt.body);
                     if (vm->callStackTop > 0 && vm->callStack[vm->callStackTop - 1].hasReturned) break;
                 }
             }
             break;
        }
        
        case NODE_STMT_BLOCK: {
            for (int i = 0; i < node->block.count; i++) {
                execute(vm, node->block.statements[i]);
                if (vm->callStackTop > 0 && vm->callStack[vm->callStackTop - 1].hasReturned) break;
            }
            break;
        }
        
        case NODE_STMT_FUNCTION: {
             Function* func = ALLOCATE_OBJ(vm, Function, OBJ_FUNCTION);
             func->name = node->function.name;
             func->isNative = false;
             func->closure = vm->env; // Capture current environment
             func->paramCount = node->function.paramCount;
             func->body = node->function.body;
             
             Value v = OBJ_VAL(func);
             
             char buf[64];
             int len = node->function.name.length;
             if (len >= 64) len = 63;
             memcpy(buf, node->function.name.start, len); buf[len] = '\0';
             
             defineGlobal(vm, buf, v);
             break;
        }
        
        case NODE_STMT_STRUCT_DECL: {
             StructDef* s = ALLOCATE_OBJ(vm, StructDef, OBJ_STRUCT_DEF);
             
             char buf[64];
             int len = node->structDecl.name.length;
             if (len >= 64) len = 63;
             memcpy(buf, node->structDecl.name.start, len); buf[len] = '\0';
             
             s->name = internString(vm, buf, len)->chars;
             s->fieldCount = node->structDecl.fieldCount;
             s->fields = malloc(sizeof(char*) * s->fieldCount);
             for(int i=0; i<s->fieldCount; i++) {
                 Token ft = node->structDecl.fields[i];
                 char* f = malloc(ft.length + 1);
                 memcpy(f, ft.start, ft.length); f[ft.length]=0;
                 s->fields[i] = f;
             }
             
             Value v = OBJ_VAL(s);
             defineGlobal(vm, buf, v);
             break;
        }
        
        case NODE_STMT_PROP_ASSIGN: {
             Value obj = evaluate(vm, node->propAssign.object);
             Value val = evaluate(vm, node->propAssign.value);
             
             if (IS_OBJ(obj) && AS_OBJ(obj)->type == OBJ_STRUCT_INSTANCE) {
                 StructInstance* inst = (StructInstance*)AS_OBJ(obj);
                 int idx = -1;
                 for(int i=0; i<inst->def->fieldCount; i++) {
                     if (inst->def->fields[i] && 
                         strncmp(inst->def->fields[i], node->propAssign.name.start, node->propAssign.name.length)==0) {
                         idx = i; break;
                     }
                 }
                 if (idx != -1) {
                     inst->fields[idx] = val;
                 } else {
                     error("Unknown field assignment.", 0);
                 }
             } else {
                 error("Property assignment requires struct instance.", 0);
             }
             break;
        }
        
        case NODE_STMT_IMPORT: {
             // Simplified import
             // In real impl, we load source, parse, execute.
             // Here we just define a module placeholder if not exists.
             if (node->importStmt.alias.length > 0) {
                 char buf[64];
                 int len = node->importStmt.alias.length;
                 if (len >= 64) len = 63;
                 memcpy(buf, node->importStmt.alias.start, len); buf[len] = '\0';
                 
                 Module* m = ALLOCATE_OBJ(vm, Module, OBJ_MODULE);
                 m->name = internString(vm, buf, len)->chars;
                 m->env = malloc(sizeof(Environment)); // Should use GC?
                 memset(m->env, 0, sizeof(Environment));
                 
                 Value v = OBJ_VAL(m);
                 defineGlobal(vm, buf, v);
             }
             break;
        }
        
        case NODE_STMT_RETURN: {
            if (vm->callStackTop == 0) {
                 error("Return outside function.", 0);
                 break;
            }
            CallFrame* frame = &vm->callStack[vm->callStackTop - 1];
            frame->hasReturned = true;
            if (node->returnStmt.value) {
                frame->returnValue = evaluate(vm, node->returnStmt.value);
            } else {
                frame->returnValue = NIL_VAL;
            }
            break;
        }
        
        default:
             evaluate(vm, node);
             break;
    }
}

// Evaluate expression
static Value evaluate(VM* vm, Node* node) {
    if (!node) {
        error("Null expression.", 0);
        return NIL_VAL;
    }

    switch (node->type) {
        case NODE_EXPR_LITERAL: {
            if (node->literal.token.type == TOKEN_NUMBER) {
                // Heuristic: if contains dot, float
                bool isFloat = false;
                for (int i = 0; i < node->literal.token.length; i++) {
                    if (node->literal.token.start[i] == '.') { isFloat = true; break; }
                }
                char buf[64];
                int len = node->literal.token.length;
                if (len < 0) len = 0;
                if (len >= 64) len = 63;
                memcpy(buf, node->literal.token.start, (size_t)len);
                buf[len] = '\0';
                
                if (isFloat) {
                    return FLOAT_VAL(strtod(buf, NULL));
                } else {
                    return INT_VAL(atoi(buf));
                }
            } else if (node->literal.token.type == TOKEN_STRING) {
                // Strip quotes
                const char* start = node->literal.token.start + 1;
                int len = node->literal.token.length - 2;
                ObjString* s = internString(vm, start, len);
                Value v = OBJ_VAL(s);
                return v;
            } else if (node->literal.token.type == TOKEN_TRUE) {
                return BOOL_VAL(true);
            } else if (node->literal.token.type == TOKEN_FALSE) {
                return BOOL_VAL(false);
            } else if (node->literal.token.type == TOKEN_NIL) {
                return NIL_VAL;
            }
            return NIL_VAL;
        }

        case NODE_EXPR_VAR: {
            if (node->var.slot != -1) {
                return vm->stack[vm->fp + node->var.slot];
            }
            VarEntry* entry = findEntry(vm, node->var.name, false);
            if (entry) return entry->value;
            errorAtToken(node->var.name, "Undefined variable.");
            return NIL_VAL;
        }

        case NODE_EXPR_UNARY: {
            Value expr = evaluate(vm, node->unary.expr);
            if (node->unary.op.type == TOKEN_MINUS) {
                if (IS_INT(expr)) expr = INT_VAL(-AS_INT(expr));
                else if (IS_FLOAT(expr)) expr = FLOAT_VAL(-AS_FLOAT(expr));
                else errorAtToken(node->unary.op, "Cannot negate non-numeric value.");
            } else if (node->unary.op.type == TOKEN_PLUS) {
                if (!IS_INT(expr) && !IS_FLOAT(expr)) errorAtToken(node->unary.op, "Unary '+' requires numeric value.");
            } else if (node->unary.op.type == TOKEN_BANG) {
                return BOOL_VAL(!isTruthy(expr));
            }
            return expr;
        }

        case NODE_EXPR_BINARY: {
             // Logical AND
             if (node->binary.op.type == TOKEN_AND) {
                 Value left = evaluate(vm, node->binary.left);
                 if (!isTruthy(left)) return BOOL_VAL(false);
                 Value right = evaluate(vm, node->binary.right);
                 return BOOL_VAL(isTruthy(right));
             }
             // Logical OR
             if (node->binary.op.type == TOKEN_OR) {
                 Value left = evaluate(vm, node->binary.left);
                 if (isTruthy(left)) return BOOL_VAL(true);
                 Value right = evaluate(vm, node->binary.right);
                 return BOOL_VAL(isTruthy(right));
             }

             Value left = evaluate(vm, node->binary.left);
             Value right = evaluate(vm, node->binary.right);

             // Concatenation
             if (node->binary.op.type == TOKEN_PLUS && (IS_STRING(left) || IS_STRING(right))) {
                 char* lStr = NULL; char* rStr = NULL;
                 char lBuf[64], rBuf[64];

                 if (IS_STRING(left)) lStr = AS_CSTRING(left);
                 else {
                     if (IS_INT(left)) { snprintf(lBuf, 64, "%ld", (long)AS_INT(left)); lStr = lBuf; }
                     else if (IS_FLOAT(left)) { snprintf(lBuf, 64, "%.6g", AS_FLOAT(left)); lStr = lBuf; }
                     else if (IS_BOOL(left)) lStr = AS_BOOL(left) ? "true" : "false"; 
                     else if (IS_NIL(left)) lStr = "nil";
                     else lStr = "[object]"; 
                 }

                 if (IS_STRING(right)) rStr = AS_CSTRING(right);
                 else {
                     if (IS_INT(right)) { snprintf(rBuf, 64, "%ld", (long)AS_INT(right)); rStr = rBuf; }
                     else if (IS_FLOAT(right)) { snprintf(rBuf, 64, "%.6g", AS_FLOAT(right)); rStr = rBuf; }
                     else if (IS_BOOL(right)) rStr = AS_BOOL(right) ? "true" : "false";
                     else if (IS_NIL(right)) rStr = "nil";
                     else rStr = "[object]";
                 }
                 
                 int len = strlen(lStr) + strlen(rStr);
                 char* combined = malloc(len + 1);
                 strcpy(combined, lStr); strcat(combined, rStr);
                 ObjString* s = internString(vm, combined, len);
                 free(combined);
                 Value v = OBJ_VAL(s); return v;
             }

             // Equality
             if (node->binary.op.type == TOKEN_EQUAL_EQUAL || node->binary.op.type == TOKEN_BANG_EQUAL) {
                 bool eq = false;
                 if (getValueType(left) != getValueType(right)) {
                     // Numeric mixed
                     if ((IS_INT(left) || IS_FLOAT(left)) && (IS_INT(right) || IS_FLOAT(right))) {
                         double a = (IS_INT(left)) ? (double)AS_INT(left) : AS_FLOAT(left);
                         double b = (IS_INT(right)) ? (double)AS_INT(right) : AS_FLOAT(right);
                         eq = (a == b);
                     } else eq = false;
                 } else {
                     switch (getValueType(left)) {
                         case VAL_BOOL: eq = (AS_BOOL(left) == AS_BOOL(right)); break;
                         case VAL_INT: eq = (AS_INT(left) == AS_INT(right)); break;
                         case VAL_FLOAT: eq = (AS_FLOAT(left) == AS_FLOAT(right)); break;
                         case VAL_NIL: eq = true; break;
                         case VAL_OBJ: eq = (AS_OBJ(left) == AS_OBJ(right)); break;
                         default: eq = false; break;
                     }
                 }
                 if (node->binary.op.type == TOKEN_BANG_EQUAL) eq = !eq;
                 return BOOL_VAL(eq);
             }

             // Numeric
             if ((IS_INT(left) || IS_FLOAT(left)) && (IS_INT(right) || IS_FLOAT(right))) {
                 if (IS_INT(left) && IS_INT(right)) {
                     int a = AS_INT(left); int b = AS_INT(right);
                     switch (node->binary.op.type) {
                         case TOKEN_PLUS: return INT_VAL(a + b);
                         case TOKEN_MINUS: return INT_VAL(a - b);
                         case TOKEN_STAR: return INT_VAL(a * b);
                         case TOKEN_SLASH: if(b==0) error("Div by zero",0); return INT_VAL(a / b);
                         case TOKEN_GREATER: return BOOL_VAL(a > b);
                         case TOKEN_GREATER_EQUAL: return BOOL_VAL(a >= b);
                         case TOKEN_LESS: return BOOL_VAL(a < b);
                         case TOKEN_LESS_EQUAL: return BOOL_VAL(a <= b);
                         default: break;
                     }
                 }
                 double a = (IS_INT(left)) ? (double)AS_INT(left) : AS_FLOAT(left);
                 double b = (IS_INT(right)) ? (double)AS_INT(right) : AS_FLOAT(right);
                 
                 switch(node->binary.op.type) {
                     case TOKEN_PLUS: return FLOAT_VAL(a + b);
                     case TOKEN_MINUS: return FLOAT_VAL(a - b);
                     case TOKEN_STAR: return FLOAT_VAL(a * b);
                     case TOKEN_SLASH: return FLOAT_VAL(a / b);
                     case TOKEN_GREATER: return BOOL_VAL(a > b);
                     case TOKEN_GREATER_EQUAL: return BOOL_VAL(a >= b);
                     case TOKEN_LESS: return BOOL_VAL(a < b);
                     case TOKEN_LESS_EQUAL: return BOOL_VAL(a <= b);
                     default: break;
                 }
             }
             error("Invalid binary op or type.", node->binary.op.line);
             return NIL_VAL;
        }

        case NODE_EXPR_CALL: {
            Function* func = NULL;
            // Struct instantiation check
            if (node->call.callee->type == NODE_EXPR_VAR) {
                 VarEntry* ve = findEntry(vm, node->call.callee->var.name, false);
                 if (ve && IS_OBJ(ve->value) && AS_OBJ(ve->value)->type == OBJ_STRUCT_DEF) {
                     StructDef* def = (StructDef*)AS_OBJ(ve->value);
                     // Instantiation logic
                     Value argVals[16]; int ac=0; Node* arg = node->call.arguments;
                     while(arg && ac < 16) { argVals[ac++] = evaluate(vm, arg); arg = arg->next; }
                     if (ac != def->fieldCount) error("Struct inst arg count mismatch", 0);
                     StructInstance* inst = ALLOCATE_OBJ(vm, StructInstance, OBJ_STRUCT_INSTANCE);
                     inst->def = def;
                     inst->fields = malloc(sizeof(Value) * def->fieldCount);
                     for(int i=0; i<def->fieldCount; i++) inst->fields[i] = argVals[i];
                     
                     Value v = OBJ_VAL(inst); return v;
                 } else if (ve && IS_OBJ(ve->value) && AS_OBJ(ve->value)->type == OBJ_FUNCTION) {
                     func = (Function*)AS_OBJ(ve->value);
                 }
                 
                 if (!func) func = findFunction(vm, node->call.callee->var.name);
                 if (!func && vm->defEnv) func = findFunctionInEnv(vm->defEnv, node->call.callee->var.name);
                 
                 if (!func) {
                     // Native builtins manually handled
                     // Native builtins manually handled
                     char fname[64]; int flen = node->call.callee->var.name.length;
                     if(flen>63) flen=63; 
                     memcpy(fname, node->call.callee->var.name.start, flen); 
                     fname[flen]=0;
                     
                     Value args[16]; int ac=0; Node* arg = node->call.arguments;
                     while(arg && ac < 16) { args[ac++] = evaluate(vm, arg); arg = arg->next; }
                     
                     if (strcmp(fname, "array")==0) { Value v = OBJ_VAL(newArray(vm)); return v; }
                     if (strcmp(fname, "map")==0) { Value v = OBJ_VAL(newMap(vm)); return v; }
                     if (strcmp(fname, "length")==0 && ac==1) {
                         Value v; 
                         int res = 0;
                         if (IS_STRING(args[0])) res = ((ObjString*)AS_OBJ(args[0]))->length;
                         else if (IS_ARRAY(args[0])) res = ((Array*)AS_OBJ(args[0]))->count;
                         v = INT_VAL(res);
                         return v;
                     }
                     if (strcmp(fname, "push")==0 && ac==2 && IS_ARRAY(args[0])) {
                         arrayPush(vm, (Array*)AS_OBJ(args[0]), args[1]);
                         Value v = INT_VAL(((Array*)AS_OBJ(args[0]))->count); return v;
                     }
                     if (strcmp(fname, "pop")==0 && ac==1 && IS_ARRAY(args[0])) {
                         Value v;
                         if (arrayPop((Array*)AS_OBJ(args[0]), &v)) return v;
                         return NIL_VAL;
                     }
                     if (strcmp(fname, "has")==0 && ac==2 && IS_MAP(args[0])) {
                         Map* m = (Map*)AS_OBJ(args[0]);
                         bool found = false;
                         if (IS_INT(args[1])) found = (mapFindEntryInt(m, AS_INT(args[1]), NULL) != NULL);
                         else if (IS_STRING(args[1])) found = (mapFindEntry(m, AS_CSTRING(args[1]), ((ObjString*)AS_OBJ(args[1]))->length, NULL) != NULL);
                         return BOOL_VAL(found);
                     }
                     if (strcmp(fname, "keys")==0 && ac==1 && IS_MAP(args[0])) {
                         Map* m = (Map*)AS_OBJ(args[0]);
                         Array* a = newArray(vm);
                         for(int i=0; i<TABLE_SIZE; i++) {
                             MapEntry* e = m->buckets[i];
                             while(e) {
                                 Value k; 
                                 if(e->isIntKey) { k = INT_VAL(e->intKey); }
                                 else { 
                                     ObjString* s = internString(vm, e->key, (int)strlen(e->key));
                                     k = OBJ_VAL(s); 
                                 }
                                 arrayPush(vm, a, k);
                                 e = e->next;
                             }
                         }
                         Value v = OBJ_VAL(a); return v;
                     }
                 }
            } else if (node->call.callee->type == NODE_EXPR_GET) {
               // Method call (modules only for now)
               Value obj = evaluate(vm, node->call.callee->get.object);
               if (IS_OBJ(obj) && AS_OBJ(obj)->type == OBJ_MODULE) {
                   func = findFunctionInEnv(((Module*)AS_OBJ(obj))->env, node->call.callee->get.name);
               }
            }

            if (!func) { error("Undefined function or method.", 0); return NIL_VAL; }
            
            Value args[16]; int ac=0; Node* arg = node->call.arguments;
            while(arg && ac < 16) { args[ac++] = evaluate(vm, arg); arg = arg->next; }
            return callFunction(vm, func, args, ac);
        }

        case NODE_EXPR_AWAIT: {
            Value v = evaluate(vm, node->unary.expr);
            if (IS_OBJ(v) && AS_OBJ(v)->type == OBJ_FUTURE) return futureAwait((Future*)AS_OBJ(v));
            return v;
        }

        case NODE_EXPR_GET: {
            Value obj = evaluate(vm, node->get.object);
            if (IS_OBJ(obj)) {
                Obj* o = AS_OBJ(obj);
                if (o->type == OBJ_MODULE) {
                     VarEntry* ve = findVarInEnv(((Module*)o)->env, node->get.name);
                     if (ve) return ve->value;
                } else if (o->type == OBJ_STRUCT_INSTANCE) {
                     StructInstance* inst = (StructInstance*)o;
                     for(int i=0; i<inst->def->fieldCount; i++) {
                         if (inst->def->fields[i] == node->get.name.start) return inst->fields[i]; // Pointer comp if interned
                         // If not strictly interned in struct def (it is):
                         if (strncmp(inst->def->fields[i], node->get.name.start, node->get.name.length)==0) return inst->fields[i];
                     }
                } else if (o->type == OBJ_STRING) {
                     if (node->get.name.length == 6 && strncmp(node->get.name.start, "length", 6) == 0) {
                         return INT_VAL(((ObjString*)o)->length);
                     }
                } else if (o->type == OBJ_ARRAY) {
                     if (node->get.name.length == 6 && strncmp(node->get.name.start, "length", 6) == 0) {
                         return INT_VAL(((Array*)o)->count);
                     }
                }
            }
            error("Invalid property access.", 0);
            return NIL_VAL;
        }

        case NODE_EXPR_INDEX: {
             Value t = evaluate(vm, node->index.target);
             Value i = evaluate(vm, node->index.index);
             if (IS_ARRAY(t) && IS_INT(i)) {
                 Array* a = (Array*)AS_OBJ(t);
                 int idx = (int)AS_INT(i);
                 if (idx >= 0 && idx < a->count) return a->items[idx];
                 error("Index out of bounds",0);
             }
             if (IS_MAP(t)) {
                 Map* m = (Map*)AS_OBJ(t);
                 MapEntry* e = NULL;
                 if (IS_INT(i)) e = mapFindEntryInt(m, AS_INT(i), NULL);
                 else if (IS_STRING(i)) e = mapFindEntry(m, AS_CSTRING(i), ((ObjString*)AS_OBJ(i))->length, NULL);
                 if (e) return e->value;
                 error("Key not found",0);
             }
             error("Invalid index opr",0);
             return NIL_VAL;
        }

        case NODE_EXPR_ARRAY_LITERAL: {
            Array* a = newArray(vm);
            Node* el = node->arrayLiteral.elements;
            while(el) {
                arrayPush(vm, a, evaluate(vm, el));
                el = el->next;
            }
            Value v = OBJ_VAL(a); return v;
        }

        default:
            return NIL_VAL;
    }
}

// Initialize VM
void initVM(VM* vm) {
    // Initialize GC State FIRST
    vm->objects = NULL;
    vm->grayStack = NULL;
    vm->grayCount = 0;
    vm->grayCapacity = 0;
    vm->bytesAllocated = 0;
    vm->nextGC = 1024 * 1024; // Start GC at 1MB


    vm->stackTop = 0;
    vm->callStackTop = 0;
    vm->fp = 0;
    vm->argc = 0;
    vm->argv = NULL;
    
    // Create global environment (Starts GC allocation!)
    vm->globalEnv = ALLOCATE_OBJ(vm, Environment, OBJ_ENVIRONMENT);
    vm->globalEnv->enclosing = NULL;
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
    pthread_mutex_init(&vm->stringPool.lock, NULL);
    
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
    
    // Initialize async task queue
    vm->taskQueue = NULL;
    vm->taskCount = 0;
    vm->taskCapacity = 0;
    
    // Initialize GC statistics
    vm->gcPhase = 0;  // GC_IDLE
    vm->gcCollectCount = 0;
    vm->gcTotalPauseUs = 0;
    vm->gcLastPauseUs = 0;
    vm->gcTotalFreed = 0;
    vm->gcPeakMemory = 0;
    vm->gcLastCollectTime = 0;
    vm->gcBytesAllocSinceGC = 0;
    
    // Generational GC
    vm->nursery = NULL;
    vm->nurseryCount = 0;
    vm->nurseryThreshold = 1000;  // Minor GC after 1000 young objects

}

// Free VM memory
// Old freeVM implementation removed (consolidated above)

// Interpret AST
void interpret(VM* vm, Node* ast) {
    if (!ast) return;
    
    // Phase 0: Intern Strings (Critical for hashPointer)
    internAST(vm, ast);
    
    // Phase 1: Resolve Locals (Calculate stack slots)
    if (!resolveAST(vm, ast)) {
        printf("Resolution failed.\n"); // Or handle error
        return;
    }
    
    execute(vm, ast);
}// Define a global variable with interning
void defineGlobal(VM* vm, const char* name, Value value) {
    ObjString* keyObj = internString(vm, name, (int)strlen(name));
    char* key = keyObj->chars;
    unsigned int h = keyObj->hash % TABLE_SIZE;
    
    // Check if exists
    VarEntry* entry = vm->globalEnv->buckets[h];
    while (entry) {
        if (entry->key == key) {
            entry->value = value;
            return;
        }
        entry = entry->next;
    }
    
    // Create new
    entry = malloc(sizeof(VarEntry));
    if (!entry) error("Memory allocation failed.", 0);
    entry->key = key;
    entry->keyString = keyObj; // Store for GC marking
    entry->keyLength = (int)strlen(key);
    entry->ownsKey = false;
    entry->value = value;
    entry->next = vm->globalEnv->buckets[h];
    vm->globalEnv->buckets[h] = entry;
}

// Helper to define variable in current environment (for hybrid stack/env)
static void defineInEnv(VM* vm, Environment* env, Token name, Value value) {
    if (!env) return;
    (void)vm;
    // name.start is interned
    unsigned int h = hashPointer(name.start) % TABLE_SIZE;
    
    // Check/Update existing in this env
    VarEntry* entry = env->buckets[h];
    while (entry) {
        if (entry->key == name.start) {
            entry->value = value;
            return;
        }
        entry = entry->next;
    }
    
    // Create new
    entry = malloc(sizeof(VarEntry));
    if (!entry) error("Memory allocation failed.", name.line);
    entry->key = (char*)name.start;
    entry->keyString = NULL; // Cannot get ObjString from Token here
    entry->keyLength = name.length;
    entry->ownsKey = false;
    entry->value = value;
    entry->next = env->buckets[h];
    env->buckets[h] = entry;
}

// Define a native function in a specific environment with interning
void defineNative(VM* vm, Environment* env, const char* name, NativeFn fn, int arity) {
    ObjString* keyObj = internString(vm, name, (int)strlen(name));
    char* key = keyObj->chars;
    unsigned int h = keyObj->hash % TABLE_SIZE;
    
    FuncEntry* fe = malloc(sizeof(FuncEntry));
    fe->key = key;
    fe->keyString = keyObj; // Store for GC marking
    fe->next = env->funcBuckets[h];
    env->funcBuckets[h] = fe;
    
    Function* func = ALLOCATE_OBJ(vm, Function, OBJ_FUNCTION);
    func->isNative = true;
    func->native = fn;
    func->paramCount = arity;
    func->name = (Token){TOKEN_IDENTIFIER, key, (int)strlen(key), 0};
    func->body = NULL;
    func->closure = NULL;
    func->obj.isMarked = true; // PERMANENT ROOT
    func->obj.isPermanent = true; // Never sweep
    
    fe->function = func;

    // ALSO register as a variable for Bytecode VM (OP_LOAD_GLOBAL checks variable buckets)
    // We treat the function as a first-class object value
    Value funcVal = OBJ_VAL(func);
    
    // Check/Update existing in this env
    VarEntry* ve = env->buckets[h];
    while (ve) {
        if (ve->key == key) {
            ve->value = funcVal;
            return;
        }
        ve = ve->next;
    }
    
    // Create new variable entry
    ve = malloc(sizeof(VarEntry));
    ve->key = key;
    ve->keyString = keyObj; // Store for GC marking
    ve->keyLength = (int)strlen(key);
    ve->ownsKey = false;
    ve->value = funcVal;
    ve->next = env->buckets[h];
    env->buckets[h] = ve;
}

// --- Built-in Natives Implementation ---

static Value nativeHas(VM* vm, Value* args, int argCount) {
    (void)vm;
    if (argCount < 2) return NIL_VAL;
    if (!IS_MAP(args[0])) return BOOL_VAL(false);
    
    Map* map = (Map*)AS_OBJ(args[0]);
    
    if (IS_STRING(args[1])) {
        ObjString* key = AS_STRING(args[1]);
        int bucket;
        MapEntry* e = mapFindEntry(map, key->chars, key->length, &bucket);
        return BOOL_VAL(e != NULL);
    } 
    return BOOL_VAL(false);
}

static Value nativeKeys(VM* vm, Value* args, int argCount) {
    if (argCount < 1) return NIL_VAL;
    if (!IS_MAP(args[0])) return NIL_VAL;
    
    Map* map = (Map*)AS_OBJ(args[0]);
    Array* keys = newArray(vm);
    
    for (int i = 0; i < TABLE_SIZE; i++) {
        MapEntry* e = map->buckets[i];
        while (e) {
            if (e->key) {
                 ObjString* s = internString(vm, e->key, (int)strlen(e->key));
                 arrayPush(vm, keys, OBJ_VAL(s));
            } else if (e->isIntKey) {
                 arrayPush(vm, keys, INT_VAL(e->intKey));
            }
            e = e->next;
        }
    }
    return OBJ_VAL(keys);
}

static Value nativeLength(VM* vm, Value* args, int argCount) {
    (void)vm; // Unused parameter
    if (argCount != 1) return NIL_VAL;
    if (IS_STRING(args[0])) return INT_VAL(((ObjString*)AS_OBJ(args[0]))->length);
    if (IS_ARRAY(args[0])) return INT_VAL(((Array*)AS_OBJ(args[0]))->count);
    return INT_VAL(0);
}

static Value nativePush(VM* vm, Value* args, int argCount) {
    if (argCount != 2) return NIL_VAL;
    if (!IS_ARRAY(args[0])) return NIL_VAL;
    Array* arr = (Array*)AS_OBJ(args[0]);
    arrayPush(vm, arr, args[1]);
    return INT_VAL(arr->count);
}

static Value nativePop(VM* vm, Value* args, int argCount) {
    (void)vm; // Unused parameter
    if (argCount != 1) return NIL_VAL;
    if (!IS_ARRAY(args[0])) return NIL_VAL;
    Value v;
    if (arrayPop((Array*)AS_OBJ(args[0]), &v)) return v;
    return NIL_VAL;
}

void registerBuiltins(VM* vm) {
    defineNative(vm, vm->globalEnv, "has", nativeHas, 2);
    defineNative(vm, vm->globalEnv, "keys", nativeKeys, 1);
    defineNative(vm, vm->globalEnv, "length", nativeLength, 1);
    defineNative(vm, vm->globalEnv, "push", nativePush, 2);
    defineNative(vm, vm->globalEnv, "pop", nativePop, 1);
}

// String concatenation helper (exposed for VM)
Value vm_concatenate(VM* vm, Value a, Value b) {
    char bufferA[64], bufferB[64];
    const char* strA;
    const char* strB;
    
    // Convert a to string
    if (IS_STRING(a)) {
        strA = AS_CSTRING(a);
    } else if (IS_INT(a)) {
        snprintf(bufferA, sizeof(bufferA), "%ld", (long)AS_INT(a));
        strA = bufferA;
    } else if (IS_FLOAT(a)) {
        snprintf(bufferA, sizeof(bufferA), "%g", AS_FLOAT(a));
        strA = bufferA;
    } else if (IS_BOOL(a)) {
        strA = AS_BOOL(a) ? "true" : "false";
    } else if (IS_NIL(a)) {
        strA = "nil";
    } else {
        strA = "[object]"; // Simple fallback
    }
    
    // Convert b to string
    if (IS_STRING(b)) {
        strB = AS_CSTRING(b);
    } else if (IS_INT(b)) {
        snprintf(bufferB, sizeof(bufferB), "%ld", (long)AS_INT(b));
        strB = bufferB;
    } else if (IS_FLOAT(b)) {
        snprintf(bufferB, sizeof(bufferB), "%g", AS_FLOAT(b));
        strB = bufferB;
    } else if (IS_BOOL(b)) {
        strB = AS_BOOL(b) ? "true" : "false";
    } else if (IS_NIL(b)) {
        strB = "nil";
    } else {
        strB = "[object]";
    }
    
    // Concatenate
    size_t lenA = strlen(strA);
    size_t lenB = strlen(strB);
    char* result = malloc(lenA + lenB + 1);
    // TODO: malloc check
    if (!result) return NIL_VAL; // Should handle OOM
    
    memcpy(result, strA, lenA);
    memcpy(result + lenA, strB, lenB);
    result[lenA + lenB] = '\0';
    
    // Intern result string checks for GC
    ObjString* str = internString(vm, result, lenA + lenB);
    free(result); // free temporary buffer (ObjString made a copy or we should transfer ownership? internString usually copies or owns? internString uses Table, keys are usually copied? Wait.)
    
    // internString(vm, ...) usually copies usage.
    // Let's verify internString logic later, but usage in interpreter had free(result).
    
    // Free temp buffers if allocated (Array logic had specific buffers, here simplified)
    // Here strA/strB point to either bufferA/bufferB or existing ObjString chars. 
    // bufferA/bufferB stack allocated. No free needed.
    
    return OBJ_VAL(str);
}
