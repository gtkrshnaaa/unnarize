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
#if 0
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
#endif

// ---- Future helpers (blocking await) ----
static Future* futureNew(VM* vm) {
    Future* f = ALLOCATE_OBJ(vm, Future, OBJ_FUTURE);
    f->done = false;
    // Initialize result to 0 int
    f->result.type = VAL_INT; f->result.intVal = 0;
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
#if 0
static bool futureIsDone(Future* f) {
    pthread_mutex_lock(&f->mu);
    bool d = f->done;
    pthread_mutex_unlock(&f->mu);
    return d;
}
#endif

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
    object->isMarked = false;
    object->next = vm->objects;
    vm->objects = object;
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
         // strings are ObjStrings which are in vm->objects list and freed there?
         // No, vm->stringPool.strings stores char* pointers?
         // In internString: vm->stringPool.strings[i] = (char*)strObj;
         // So they ARE ObjStrings.
         // If we free them in object loop, do NOT free them here as pointers.
         // Just free the array.
         free(vm->stringPool.strings);
         free(vm->stringPool.hashes);
    }
    
    // Free all objects
    Obj* object = vm->objects;
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
    if (v.type == VAL_NIL) return false;
    if (v.type == VAL_BOOL) return v.boolVal;
    if (v.type == VAL_INT) return v.intVal != 0;
    if (v.type == VAL_FLOAT) return v.floatVal != 0.0;
    
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
    // Special handling for empty string
    if (length <= 0) length = 0;
    
    unsigned int h = hash(str, length);
    
    // Step 1: Find existing match
    for (int i = 0; i < vm->stringPool.count; i++) {
        ObjString* strObj = (ObjString*)vm->stringPool.strings[i];
         if (strObj->hash == h && strObj->length == length && memcmp(strObj->chars, str, length) == 0) {
             return strObj;
         }
    }
    
    // Step 2: Create new
    ObjString* strObj = ALLOCATE_OBJ(vm, ObjString, OBJ_STRING);
    strObj->length = length;
    strObj->hash = h;
    strObj->chars = malloc(length + 1);
    memcpy(strObj->chars, str, length);
    strObj->chars[length] = '\0';
    
    // Step 3: Add to pool
    if (vm->stringPool.count == vm->stringPool.capacity) {
        int oldCap = vm->stringPool.capacity;
        vm->stringPool.capacity = oldCap < 8 ? 8 : oldCap * 2;
        vm->stringPool.strings = realloc(vm->stringPool.strings, sizeof(char*) * vm->stringPool.capacity); 
        vm->stringPool.hashes = realloc(vm->stringPool.hashes, sizeof(unsigned int) * vm->stringPool.capacity);
        if (!vm->stringPool.strings || !vm->stringPool.hashes) error("Memory allocation failed for string pool.", 0);
    }
    vm->stringPool.strings[vm->stringPool.count] = (char*)strObj; // Hack cast
    vm->stringPool.hashes[vm->stringPool.count] = h; // Keep hashes array updated
    vm->stringPool.count++;
    
    return strObj;
}

// Basic helper to intern a token's valid string range
static void internToken(VM* vm, Token* token) {
    if (token->length > 0) {
        // internToken
        // internString returns ObjString*. We need char* for current Map/implementation?
        // But Token doesn't store interned string directly?
        // Wait, where is this used?
        // "char* interned = internString(vm, token->start, token->length);"
        // Should be:
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
        case NODE_STMT_LOADEXTERN:
            internAST(vm, node->loadexternStmt.pathExpr);
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
#if 0
static ModuleEntry* findModuleEntry(VM* vm, const char* name, int length, bool insert) {
    // For ModuleEntry, we intern the key first
    char* key = internString(vm, name, length)->chars;
    
    // Simple hash
    int idx = 0; // Simplified hash for bucket
    // Actually using `hash` function
    unsigned int h = hash(name, length);
    idx = h % TABLE_SIZE;
    
    ModuleEntry* e = vm->moduleBuckets[idx];
    while (e) {
        if (e->name == key || (e->nameLen == length && memcmp(e->name, name, length) == 0)) {
            return e;
        }
        e = e->next;
    }
    
    if (!insert) return NULL;

    e = malloc(sizeof(ModuleEntry));
    if (!e) error("Memory allocation failed.", 0);
    // e->name = key; // Should use interned char* if we trust it stays alive by being in ObjString rooted?
    // ObjString might be collected if not rooted!
    // We should duplicate name or root the ObjString.
    // For now, duplicate to be safe
    e->name = malloc(length + 1);
    memcpy(e->name, name, length); e->name[length] = '\0';
    e->nameLen = length;
    e->module = NULL;
    e->next = vm->moduleBuckets[idx];
    vm->moduleBuckets[idx] = e;
    return e;
}
#endif

// Check if regular file exists
/*
static bool fileExists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}
*/
#if 0
static bool fileExists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}
#endif

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
    
    // If inserting, search only current environment (already done above? No, checked buckets[h] for return?)
    // Wait, insert logic below assumes we want to return existing if found? 
    // Usually defineVar checks exists.
    // Let's stick to: if insert, we check current env.
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
        entry->keyLength = name.length;
        entry->ownsKey = false; // Owned by StringPool
        
        entry->value.type = VAL_INT; // Default init
        entry->value.intVal = 0;
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
#if 0
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
#endif

// Convert 1-char string to int code if applicable
/*
static bool tryCharCode(Value v, int* out) {
    if (v.type == VAL_INT) { *out = v.intVal; return true; }
    return false; 
}
*/
#if 0
static bool tryCharCode(Value v, int* out) {
    if (IS_STRING(v) && AS_STRING(v)->length == 1) {
        *out = (unsigned char)AS_CSTRING(v)[0];
        return true;
    }
    return false;
}
#endif

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
        a->items = (Value*)reallocate(vm, a->items, sizeof(Value) * oldCapacity, sizeof(Value) * a->capacity);
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
static bool arrayPop(Array* a, Value* out) {
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
#if 0
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
#endif

/*
static bool mapDeleteStr(Map* m, const char* key, int len) {
    unsigned int h = hash(key, len) % TABLE_SIZE;
    // ... impl
    return false;
}
*/
#if 0
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
#endif


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
        return (Value){VAL_NIL, .intVal=0};
    }

    // Push arguments to stack (They become locals 0..N-1 for the new frame)
    int oldStackTop = vm->stackTop; // Save to restore later? No, return value replaces them
    for (int i = 0; i < argCount; i++) {
        vm->stack[vm->stackTop++] = args[i];
    }
    
    // Save current frame setup
    if (vm->callStackTop >= CALL_STACK_MAX) {
        error("Maximum call stack depth exceeded.", func->name.line); 
        return (Value){VAL_NIL, .intVal=0}; 
    }
    
    CallFrame* frame = &vm->callStack[vm->callStackTop++];
    frame->env = vm->env;
    frame->fp = vm->fp;
    frame->hasReturned = false;
    frame->returnValue = (Value){VAL_NIL, .intVal=0};
    
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
             
             VarEntry* ve = malloc(sizeof(VarEntry));
             ve->key = internString(vm, buf, len)->chars;
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
        Value v; v.type = VAL_OBJ; v.obj = (Obj*)f; return v;
    }
    return ret;
}

// Helper for print (forward decl)


// printValue implementation
void printValue(Value val) {
    if (val.type == VAL_NIL) { printf("nil"); return; }
    switch (val.type) {
        case VAL_BOOL: printf(val.boolVal ? "true" : "false"); break;
        case VAL_INT: printf("%d", val.intVal); break;
        case VAL_FLOAT: printf("%.6g", val.floatVal); break;
        case VAL_OBJ: {
            Obj* o = AS_OBJ(val);
            if (IS_STRING(val)) printf("%s", AS_CSTRING(val));
            else if (o->type == OBJ_ARRAY) printf("<array>");
            else if (o->type == OBJ_MAP) printf("<map>");
            else if (o->type == OBJ_FUNCTION) {
                 Function* f = (Function*)o;
                 if(f->name.start) printf("<fn %.*s>", f->name.length, f->name.start); 
                 else printf("<script>");
            }
            else printf("<obj>");
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
                val.type = VAL_NIL;
                val.intVal = 0;
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
                    newVal.type = VAL_INT;
                    
                    int currentInt = (current.type == VAL_INT) ? current.intVal : 0; 
                    int valInt = (val.type == VAL_INT) ? val.intVal : 0;
                    // Note: Simplified arithmetic for brevity, should handle float/string
                    
                    switch (node->assign.operator.type) {
                        case TOKEN_PLUS_EQUAL: newVal.intVal = currentInt + valInt; break;
                        case TOKEN_MINUS_EQUAL: newVal.intVal = currentInt - valInt; break;
                        case TOKEN_STAR_EQUAL: newVal.intVal = currentInt * valInt; break;
                        case TOKEN_SLASH_EQUAL: newVal.intVal = currentInt / (valInt ? valInt : 1); break;
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
                     
                     Value current = entry ? entry->value : (Value){VAL_INT, .intVal=0}; 
                     Value newVal;
                     newVal.type = VAL_INT;
                     
                     int currentInt = (current.type == VAL_INT) ? current.intVal : 0; 
                     int valInt = (val.type == VAL_INT) ? val.intVal : 0;
                     
                     switch (node->assign.operator.type) {
                         case TOKEN_PLUS_EQUAL: newVal.intVal = currentInt + valInt; break;
                         case TOKEN_MINUS_EQUAL: newVal.intVal = currentInt - valInt; break;
                         case TOKEN_STAR_EQUAL: newVal.intVal = currentInt * valInt; break;
                         case TOKEN_SLASH_EQUAL: newVal.intVal = currentInt / (valInt ? valInt : 1); break;
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
            
            if (IS_ARRAY(target) && idx.type == VAL_INT) {
                Array* a = (Array*)AS_OBJ(target);
                if (idx.intVal >= 0 && idx.intVal < a->count) {
                    a->items[idx.intVal] = val;
                } else {
                    error("Index out of bounds.", 0);
                }
            } else if (IS_MAP(target)) {
                Map* m = (Map*)AS_OBJ(target);
                if (idx.type == VAL_INT) {
                    mapSetInt(m, idx.intVal, val);
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
             
             Value v; v.type = VAL_OBJ; v.obj = (Obj*)func;
             
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
             
             Value v; v.type = VAL_OBJ; v.obj = (Obj*)s;
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
        
        case NODE_STMT_LOADEXTERN: {
            Value p = evaluate(vm, node->loadexternStmt.pathExpr);
            if (!IS_STRING(p)) { error("loadextern path must be string", 0); break; }
            ObjString* s = (ObjString*)AS_OBJ(p);
            
            void* handle = dlopen(s->chars, RTLD_NOW);
            if (!handle) {
                printf("Failed to load extern: %s\n", dlerror());
                break;
            }
             if (vm->externHandleCount < 256) {
                 vm->externHandles[vm->externHandleCount++] = handle;
             }
             
             typedef void (*InitFn)(VM*);
             InitFn init = (InitFn)dlsym(handle, "init_unnarize_module");
             if (init) init(vm);
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
                 
                 Value v; v.type = VAL_OBJ; v.obj = (Obj*)m;
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
                frame->returnValue.type = VAL_NIL; frame->returnValue.intVal=0;
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
        return (Value){VAL_NIL, .intVal = 0};
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
                    return (Value){VAL_FLOAT, .floatVal = strtod(buf, NULL)};
                } else {
                    return (Value){VAL_INT, .intVal = atoi(buf)};
                }
            } else if (node->literal.token.type == TOKEN_STRING) {
                // Strip quotes
                const char* start = node->literal.token.start + 1;
                int len = node->literal.token.length - 2;
                ObjString* s = internString(vm, start, len);
                Value v; v.type = VAL_OBJ; v.obj = (Obj*)s;
                return v;
            } else if (node->literal.token.type == TOKEN_TRUE) {
                return (Value){VAL_BOOL, .boolVal = true};
            } else if (node->literal.token.type == TOKEN_FALSE) {
                return (Value){VAL_BOOL, .boolVal = false};
            } else if (node->literal.token.type == TOKEN_NIL) {
                return (Value){VAL_NIL, .intVal = 0};
            }
            return (Value){VAL_NIL, .intVal = 0};
        }

        case NODE_EXPR_VAR: {
            if (node->var.slot != -1) {
                return vm->stack[vm->fp + node->var.slot];
            }
            VarEntry* entry = findEntry(vm, node->var.name, false);
            if (entry) return entry->value;
            errorAtToken(node->var.name, "Undefined variable.");
            return (Value){VAL_NIL, .intVal = 0};
        }

        case NODE_EXPR_UNARY: {
            Value expr = evaluate(vm, node->unary.expr);
            if (node->unary.op.type == TOKEN_MINUS) {
                if (expr.type == VAL_INT) expr.intVal = -expr.intVal;
                else if (expr.type == VAL_FLOAT) expr.floatVal = -expr.floatVal;
                else errorAtToken(node->unary.op, "Cannot negate non-numeric value.");
            } else if (node->unary.op.type == TOKEN_PLUS) {
                if (expr.type != VAL_INT && expr.type != VAL_FLOAT) errorAtToken(node->unary.op, "Unary '+' requires numeric value.");
            } else if (node->unary.op.type == TOKEN_BANG) {
                return (Value){VAL_BOOL, .boolVal = !isTruthy(expr)};
            }
            return expr;
        }

        case NODE_EXPR_BINARY: {
             // Logical AND
             if (node->binary.op.type == TOKEN_AND) {
                 Value left = evaluate(vm, node->binary.left);
                 if (!isTruthy(left)) return (Value){VAL_BOOL, .boolVal = false};
                 Value right = evaluate(vm, node->binary.right);
                 return (Value){VAL_BOOL, .boolVal = isTruthy(right)};
             }
             // Logical OR
             if (node->binary.op.type == TOKEN_OR) {
                 Value left = evaluate(vm, node->binary.left);
                 if (isTruthy(left)) return (Value){VAL_BOOL, .boolVal = true};
                 Value right = evaluate(vm, node->binary.right);
                 return (Value){VAL_BOOL, .boolVal = isTruthy(right)};
             }

             Value left = evaluate(vm, node->binary.left);
             Value right = evaluate(vm, node->binary.right);

             // Concatenation
             if (node->binary.op.type == TOKEN_PLUS && (IS_STRING(left) || IS_STRING(right))) {
                 char* lStr = NULL; char* rStr = NULL;
                 char lBuf[64], rBuf[64];

                 if (IS_STRING(left)) lStr = AS_CSTRING(left);
                 else {
                     if (left.type == VAL_INT) { snprintf(lBuf, 64, "%d", left.intVal); lStr = lBuf; }
                     else if (left.type == VAL_FLOAT) { snprintf(lBuf, 64, "%.6g", left.floatVal); lStr = lBuf; }
                     else if (left.type == VAL_BOOL) lStr = left.boolVal ? "true" : "false"; 
                     else if (left.type == VAL_NIL) lStr = "nil";
                     else lStr = "[object]"; 
                 }

                 if (IS_STRING(right)) rStr = AS_CSTRING(right);
                 else {
                     if (right.type == VAL_INT) { snprintf(rBuf, 64, "%d", right.intVal); rStr = rBuf; }
                     else if (right.type == VAL_FLOAT) { snprintf(rBuf, 64, "%.6g", right.floatVal); rStr = rBuf; }
                     else if (right.type == VAL_BOOL) rStr = right.boolVal ? "true" : "false";
                     else if (right.type == VAL_NIL) rStr = "nil";
                     else rStr = "[object]";
                 }
                 
                 int len = strlen(lStr) + strlen(rStr);
                 char* combined = malloc(len + 1);
                 strcpy(combined, lStr); strcat(combined, rStr);
                 ObjString* s = internString(vm, combined, len);
                 free(combined);
                 Value v; v.type = VAL_OBJ; v.obj = (Obj*)s; return v;
             }

             // Equality
             if (node->binary.op.type == TOKEN_EQUAL_EQUAL || node->binary.op.type == TOKEN_BANG_EQUAL) {
                 bool eq = false;
                 if (left.type != right.type) {
                     // Numeric mixed
                     if ((left.type == VAL_INT || left.type == VAL_FLOAT) && (right.type == VAL_INT || right.type == VAL_FLOAT)) {
                         double a = (left.type == VAL_INT) ? (double)left.intVal : left.floatVal;
                         double b = (right.type == VAL_INT) ? (double)right.intVal : right.floatVal;
                         eq = (a == b);
                     } else eq = false;
                 } else {
                     switch (left.type) {
                         case VAL_BOOL: eq = (left.boolVal == right.boolVal); break;
                         case VAL_INT: eq = (left.intVal == right.intVal); break;
                         case VAL_FLOAT: eq = (left.floatVal == right.floatVal); break;
                         case VAL_NIL: eq = true; break;
                         case VAL_OBJ: eq = (left.obj == right.obj); break;
                         default: eq = false; break;
                     }
                 }
                 if (node->binary.op.type == TOKEN_BANG_EQUAL) eq = !eq;
                 return (Value){VAL_BOOL, .boolVal = eq};
             }

             // Numeric
             if ((left.type == VAL_INT || left.type == VAL_FLOAT) && (right.type == VAL_INT || right.type == VAL_FLOAT)) {
                 if (left.type == VAL_INT && right.type == VAL_INT) {
                     int a = left.intVal; int b = right.intVal;
                     switch (node->binary.op.type) {
                         case TOKEN_PLUS: return (Value){VAL_INT, .intVal = a + b};
                         case TOKEN_MINUS: return (Value){VAL_INT, .intVal = a - b};
                         case TOKEN_STAR: return (Value){VAL_INT, .intVal = a * b};
                         case TOKEN_SLASH: if(b==0) error("Div by zero",0); return (Value){VAL_INT, .intVal = a / b};
                         case TOKEN_GREATER: return (Value){VAL_BOOL, .boolVal = a > b};
                         case TOKEN_GREATER_EQUAL: return (Value){VAL_BOOL, .boolVal = a >= b};
                         case TOKEN_LESS: return (Value){VAL_BOOL, .boolVal = a < b};
                         case TOKEN_LESS_EQUAL: return (Value){VAL_BOOL, .boolVal = a <= b};
                         default: break;
                     }
                 }
                 double a = (left.type == VAL_INT) ? (double)left.intVal : left.floatVal;
                 double b = (right.type == VAL_INT) ? (double)right.intVal : right.floatVal;
                 switch(node->binary.op.type) {
                     case TOKEN_PLUS: return (Value){VAL_FLOAT, .floatVal = a + b};
                     case TOKEN_MINUS: return (Value){VAL_FLOAT, .floatVal = a - b};
                     case TOKEN_STAR: return (Value){VAL_FLOAT, .floatVal = a * b};
                     case TOKEN_SLASH: return (Value){VAL_FLOAT, .floatVal = a / b};
                     case TOKEN_GREATER: return (Value){VAL_BOOL, .boolVal = a > b};
                     case TOKEN_GREATER_EQUAL: return (Value){VAL_BOOL, .boolVal = a >= b};
                     case TOKEN_LESS: return (Value){VAL_BOOL, .boolVal = a < b};
                     case TOKEN_LESS_EQUAL: return (Value){VAL_BOOL, .boolVal = a <= b};
                     default: break;
                 }
             }
             error("Invalid binary op or type.", node->binary.op.line);
             return (Value){VAL_NIL, .intVal=0};
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
                     Value v; v.type = VAL_OBJ; v.obj = (Obj*)inst; return v;
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
                     
                     if (strcmp(fname, "array")==0) { Value v; v.type = VAL_OBJ; v.obj = (Obj*)newArray(vm); return v; }
                     if (strcmp(fname, "map")==0) { Value v; v.type = VAL_OBJ; v.obj = (Obj*)newMap(vm); return v; }
                     if (strcmp(fname, "length")==0 && ac==1) {
                         Value v; v.type = VAL_INT; 
                         if (IS_STRING(args[0])) v.intVal = ((ObjString*)AS_OBJ(args[0]))->length;
                         else if (IS_ARRAY(args[0])) v.intVal = ((Array*)AS_OBJ(args[0]))->count;
                         else v.intVal = 0; // map size?
                         return v;
                     }
                     if (strcmp(fname, "push")==0 && ac==2 && IS_ARRAY(args[0])) {
                         arrayPush(vm, (Array*)AS_OBJ(args[0]), args[1]);
                         Value v; v.type = VAL_INT; v.intVal = ((Array*)AS_OBJ(args[0]))->count; return v;
                     }
                     if (strcmp(fname, "pop")==0 && ac==1 && IS_ARRAY(args[0])) {
                         Value v;
                         if (arrayPop((Array*)AS_OBJ(args[0]), &v)) return v;
                         return (Value){VAL_NIL, .intVal=0};
                     }
                     if (strcmp(fname, "has")==0 && ac==2 && IS_MAP(args[0])) {
                         Map* m = (Map*)AS_OBJ(args[0]);
                         bool found = false;
                         if (args[1].type == VAL_INT) found = (mapFindEntryInt(m, args[1].intVal, NULL) != NULL);
                         else if (IS_STRING(args[1])) found = (mapFindEntry(m, AS_CSTRING(args[1]), ((ObjString*)AS_OBJ(args[1]))->length, NULL) != NULL);
                         return (Value){VAL_BOOL, .boolVal=found};
                     }
                     if (strcmp(fname, "keys")==0 && ac==1 && IS_MAP(args[0])) {
                         Map* m = (Map*)AS_OBJ(args[0]);
                         Array* a = newArray(vm);
                         for(int i=0; i<TABLE_SIZE; i++) {
                             MapEntry* e = m->buckets[i];
                             while(e) {
                                 Value k; 
                                 if(e->isIntKey) { k.type=VAL_INT; k.intVal=e->intKey; }
                                 else { 
                                     ObjString* s = internString(vm, e->key, (int)strlen(e->key));
                                     k.type=VAL_OBJ; k.obj=(Obj*)s; 
                                 }
                                 arrayPush(vm, a, k);
                                 e = e->next;
                             }
                         }
                         Value v; v.type=VAL_OBJ; v.obj=(Obj*)a; return v;
                     }
                 }
            } else if (node->call.callee->type == NODE_EXPR_GET) {
               // Method call (modules only for now)
               Value obj = evaluate(vm, node->call.callee->get.object);
               if (IS_OBJ(obj) && AS_OBJ(obj)->type == OBJ_MODULE) {
                   func = findFunctionInEnv(((Module*)AS_OBJ(obj))->env, node->call.callee->get.name);
               }
            }

            if (!func) { error("Undefined function or method.", 0); return (Value){VAL_NIL, .intVal=0}; }
            
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
                if (obj.obj->type == OBJ_MODULE) {
                     VarEntry* ve = findVarInEnv(((Module*)obj.obj)->env, node->get.name);
                     if (ve) return ve->value;
                } else if (obj.obj->type == OBJ_STRUCT_INSTANCE) {
                     StructInstance* inst = (StructInstance*)obj.obj;
                     for(int i=0; i<inst->def->fieldCount; i++) {
                         if (inst->def->fields[i] == node->get.name.start) return inst->fields[i]; // Pointer comp if interned
                         // If not strictly interned in struct def (it is):
                         if (strncmp(inst->def->fields[i], node->get.name.start, node->get.name.length)==0) return inst->fields[i];
                     }
                } else if (obj.obj->type == OBJ_STRING) {
                     if (node->get.name.length == 6 && strncmp(node->get.name.start, "length", 6) == 0) {
                         return (Value){VAL_INT, .intVal = ((ObjString*)obj.obj)->length};
                     }
                } else if (obj.obj->type == OBJ_ARRAY) {
                     if (node->get.name.length == 6 && strncmp(node->get.name.start, "length", 6) == 0) {
                         return (Value){VAL_INT, .intVal = ((Array*)obj.obj)->count};
                     }
                }
            }
            error("Invalid property access.", 0);
            return (Value){VAL_NIL, .intVal=0};
        }

        case NODE_EXPR_INDEX: {
             Value t = evaluate(vm, node->index.target);
             Value i = evaluate(vm, node->index.index);
             if (IS_ARRAY(t) && i.type == VAL_INT) {
                 Array* a = (Array*)AS_OBJ(t);
                 if (i.intVal >= 0 && i.intVal < a->count) return a->items[i.intVal];
                 error("Index out of bounds",0);
             }
             if (IS_MAP(t)) {
                 Map* m = (Map*)AS_OBJ(t);
                 MapEntry* e = NULL;
                 if (i.type == VAL_INT) e = mapFindEntryInt(m, i.intVal, NULL);
                 else if (IS_STRING(i)) e = mapFindEntry(m, AS_CSTRING(i), ((ObjString*)AS_OBJ(i))->length, NULL);
                 if (e) return e->value;
                 error("Key not found",0);
             }
             error("Invalid index opr",0);
             return (Value){VAL_NIL, .intVal=0};
        }

        case NODE_EXPR_ARRAY_LITERAL: {
            Array* a = newArray(vm);
            Node* el = node->arrayLiteral.elements;
            while(el) {
                arrayPush(vm, a, evaluate(vm, el));
                el = el->next;
            }
            Value v; v.type = VAL_OBJ; v.obj = (Obj*)a; return v;
        }

        default:
            return (Value){VAL_NIL, .intVal = 0};
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
    
    vm->valuePool.free_list[vm->valuePool.capacity - 1] = -1;
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
    unsigned int h = hashPointer(key) % TABLE_SIZE;
    
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
    unsigned int h = hashPointer(key) % TABLE_SIZE;
    
    FuncEntry* fe = malloc(sizeof(FuncEntry));
    fe->key = key;
    fe->next = env->funcBuckets[h];
    env->funcBuckets[h] = fe;
    
    Function* func = malloc(sizeof(Function));
    func->isNative = true;
    func->native = fn;
    func->paramCount = arity;
    func->name = (Token){TOKEN_IDENTIFIER, key, (int)strlen(key), 0};
    func->body = NULL;
    func->closure = NULL;
    
    fe->function = func;
}
