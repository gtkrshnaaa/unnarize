#include "vm.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include "bytecode/chunk.h"

// Concurrent GC state
static pthread_mutex_t gcMutex = PTHREAD_MUTEX_INITIALIZER;
static volatile int gcConcurrentActive = 0;

// GC Helpers
void markObject(VM* vm, Obj* object);
void markValue(VM* vm, Value value);
static void markRoots(VM* vm);
static void traceReferences(VM* vm);
static size_t sweep(VM* vm, Obj** listHead);
static void garbageCollect(VM* vm) { collectGarbage(vm); } // Wrapper or just rename prototypes

void* reallocate(VM* vm, void* pointer, size_t oldSize, size_t newSize) {
    vm->bytesAllocated += newSize - oldSize;
    
    if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
        garbageCollect(vm);
#endif
        if (vm->bytesAllocated > vm->nextGC) {
            garbageCollect(vm);
        }
    }

    if (newSize == 0) {
        free(pointer);
        return NULL;
    }

    void* result = realloc(pointer, newSize);
    if (result == NULL) exit(1);
    return result;
}

void markObject(VM* vm, Obj* object) {
    if (object == NULL) return;
    if (object->isMarked) return;
    
    // Thread Safety: Lock if concurrent GC is active
    if (gcConcurrentActive) pthread_mutex_lock(&gcMutex);
    
    // Double check after lock
    if (object->isMarked) {
        if (gcConcurrentActive) pthread_mutex_unlock(&gcMutex);
        return;
    }
    
    object->isMarked = true;
    
    if (vm->grayCapacity < vm->grayCount + 1) {
        vm->grayCapacity = GROW_CAPACITY(vm->grayCapacity);
        vm->grayStack = (Obj**)realloc(vm->grayStack, sizeof(Obj*) * vm->grayCapacity);
        if (vm->grayStack == NULL) exit(1);
    }
    
    vm->grayStack[vm->grayCount++] = object;
    
    if (gcConcurrentActive) pthread_mutex_unlock(&gcMutex);
}

// Re-gray a potentially black object (for Write Barrier)
void grayObject(VM* vm, Obj* object) {
    if (object == NULL) return;
    
    // Thread Safety
    if (gcConcurrentActive) pthread_mutex_lock(&gcMutex);
    
    // Force push to gray stack even if already marked
    if (vm->grayCapacity < vm->grayCount + 1) {
        vm->grayCapacity = GROW_CAPACITY(vm->grayCapacity);
        vm->grayStack = (Obj**)realloc(vm->grayStack, sizeof(Obj*) * vm->grayCapacity);
        if (vm->grayStack == NULL) exit(1);
    }
    
    vm->grayStack[vm->grayCount++] = object;
    
    if (gcConcurrentActive) pthread_mutex_unlock(&gcMutex);
}

// ... (markValue, etc same)



void markValue(VM* vm, Value value) {
    if (IS_OBJ(value)) markObject(vm, AS_OBJ(value));
}

static void markArray(VM* vm, Array* array) {
    for (int i = 0; i < array->count; i++) {
        markValue(vm, array->items[i]);
    }
}

static void blackenObject(VM* vm, Obj* object) {
    switch (object->type) {
        case OBJ_STRING:
        case OBJ_NATIVE:
        case OBJ_RESOURCE:
            break;
            
        case OBJ_UPVALUE:
            // markValue(vm, ((ObjUpvalue*)object)->closed);
            break;
            
        case OBJ_FUNCTION: {
            Function* function = (Function*)object;
            // markObject(vm, (Obj*)function->name); // Token strings are interned? Interned strings are Objs?
            // Existing Logic: Token.start points to interned char*. 
            // New Logic: Token.start should point to ObjString->chars ??
            // OR we keep StringPool separate?
            // If we use GC, StringStrings are Objects.
            // Tokens usually point to source code OR interned strings.
            // If we intern as ObjString, we need to handle that.
            // For now, let's assume Function references other objects via constant pool (if compiled) 
            // but Unnarize is AST-based. The AST itself holds references?
            // AST Nodes have Tokens. Token.start is char*.
            // If that char* is malloc'd, it should be an ObjString?
            // This is the tricky part of converting an AST walker to GC.
            // The AST nodes themselves are malloc'd. Are they GC'd?
            // User didn't ask to GC the AST. "Create a unified Obj struct ... strings created in a while loop".
            // So AST might remain manual?
            // Keep AST manual. Runtime values are GC'd.
            if (function->closure) {
                 markObject(vm, (Obj*)function->closure); 
            }
            if (function->bytecodeChunk) {
                BytecodeChunk* chunk = function->bytecodeChunk;
                for (int i = 0; i < chunk->constantCount; i++) {
                    markValue(vm, chunk->constants[i]);
                }
            }
            break;
        }
        
        case OBJ_ARRAY: {
            Array* array = (Array*)object;
            markArray(vm, array);
            break;
        }
        
        case OBJ_MAP: {
            Map* map = (Map*)object;
            for (int i = 0; i < TABLE_SIZE; i++) {
                MapEntry* entry = map->buckets[i];
                while (entry) {
                    markValue(vm, entry->value);
                    entry = entry->next;
                }
            }
            break;
        }
        
        case OBJ_ENVIRONMENT: {
            Environment* env = (Environment*)object;
            // Mark parent? Yes if it's reachable.
            markObject(vm, (Obj*)env->enclosing);
            
            // Mark values and key strings in buckets
            for (int i = 0; i < TABLE_SIZE; i++) {
                VarEntry* entry = env->buckets[i];
                while (entry) {
                    markValue(vm, entry->value);
                    // CRITICAL: Mark the key string to prevent pruning
                    if (entry->keyString) markObject(vm, (Obj*)entry->keyString);
                    entry = entry->next;
                }
                
                // Functions in env
                FuncEntry* funcEntry = env->funcBuckets[i];
                while (funcEntry) {
                    if (funcEntry->function) markObject(vm, (Obj*)funcEntry->function);
                    // CRITICAL: Mark the key string to prevent pruning
                    if (funcEntry->keyString) markObject(vm, (Obj*)funcEntry->keyString);
                    funcEntry = funcEntry->next;
                }
            }
            break;
        }

        case OBJ_MODULE: {
            Module* mod = (Module*)object;
            markObject(vm, (Obj*)mod->env);
            break;
        }

        default: break;
    }
}

static void markRoots(VM* vm) {
    for (Value* slot = vm->stack; slot < vm->stack + vm->stackTop; slot++) {
        markValue(vm, *slot);
    }

    // Mark Call Frames and their environments
    for (int i = 0; i < vm->callStackTop; i++) {
        markObject(vm, (Obj*)vm->callStack[i].env);
        if (vm->callStack[i].function) markObject(vm, (Obj*)vm->callStack[i].function);
    }
    
    // Mark Global Environment
    markObject(vm, (Obj*)vm->globalEnv);
    markObject(vm, (Obj*)vm->defEnv);

    // Mark Global Handles (if any)
}

static void traceReferences(VM* vm) {
    while (vm->grayCount > 0) {
        Obj* object = vm->grayStack[--vm->grayCount];
        blackenObject(vm, object);
    }
}

// Incremental tracing: process up to 'workUnits' objects per call
// Returns number of remaining gray objects
static int traceReferencesIncremental(VM* vm, int workUnits) {
    int processed = 0;
    while (vm->grayCount > 0 && processed < workUnits) {
        Obj* object = vm->grayStack[--vm->grayCount];
        blackenObject(vm, object);
        processed++;
    }
    return vm->grayCount;
}

void freeObject(VM* vm, Obj* object) {
    (void)vm;
    switch (object->type) {
        case OBJ_STRING: {
            ObjString* string = (ObjString*)object;
            free(string->chars);
            free(object);
            break;
        }
        case OBJ_ARRAY: {
            Array* array = (Array*)object;
            free(array->items);
            free(object);
            break;
        }
        case OBJ_MAP: {
            Map* map = (Map*)object;
            for (int i = 0; i < TABLE_SIZE; i++) {
                MapEntry* entry = map->buckets[i];
                while (entry) {
                    MapEntry* next = entry->next;
                    free(entry->key); 
                    free(entry);
                    entry = next;
                }
            }
            free(object);
            break;
        }
        case OBJ_FUNCTION: {
            Function* func = (Function*)object;
            // Free bytecode chunk if it exists
            if (func->bytecodeChunk) {
                freeChunk(func->bytecodeChunk);
                free(func->bytecodeChunk);
                func->bytecodeChunk = NULL;
            }
            // Free params array if allocated
            if (func->params) {
                // params is allocated in parser, typically part of AST
                // Don't free here as it's part of AST lifecycle
            }
            free(object);
            break;
        }
        case OBJ_STRUCT_DEF: {
            StructDef* def = (StructDef*)object;
            free(def->fields); 
            free(object);
            break;
        }
        case OBJ_STRUCT_INSTANCE: {
            StructInstance* inst = (StructInstance*)object;
            free(inst->fields);
            free(object);
            break;
        }
        case OBJ_MODULE: {
            Module* mod = (Module*)object;
            free(mod->name); // We strdup'd name in register functions
            // We should free Env if we allocated it?
            // Currently manual Env allocation in register functions.
            // Ideally define freeEnvironment helper.
            // Leaving Env leak for now significantly safer than double free if alias shared.
            // But we malloc'd it.
            // free(mod->env); // TODO: safe verify
            free(object);
            break;
        }
        case OBJ_RESOURCE: {
            ObjResource* res = (ObjResource*)object;
            if (res->cleanup) res->cleanup(res->data);
            free(object);
            break;
        }
        case OBJ_FUTURE: {
            Future* f = (Future*)object;
            pthread_mutex_destroy(&f->mu);
            pthread_cond_destroy(&f->cv);
            free(object);
            break;
        }
        case OBJ_ENVIRONMENT: {
            Environment* env = (Environment*)object;
            for (int i = 0; i < TABLE_SIZE; i++) {
                VarEntry* entry = env->buckets[i];
                while (entry) {
                    VarEntry* next = entry->next;
                    if (entry->key && entry->ownsKey) free(entry->key);
                    free(entry);
                    entry = next;
                }
                
                FuncEntry* fEntry = env->funcBuckets[i];
                while (fEntry) {
                    FuncEntry* next = fEntry->next;
                    // fEntry->key is assumed interned or managed?
                    // According to vm.c registerNativeFunction, key is internString()->chars.
                    // But FuncEntry allocation in findFuncEntry uses interned key.
                    // However, we should check ownsKey equivalents or if we dup'd it.
                    // In findModuleEntry we have issues.
                    // Generally functions use interned keys.
                    // Just free the entry struct.
                    if (fEntry->key && fEntry->function && fEntry->function->isNative && !fEntry->function->name.start) {
                        // Special case? No.
                    }
                     free(fEntry); // Keys are usually shared/interned.
                     fEntry = next;
                }
            }
            free(object);
            break;
        }

        default:
            free(object);
            break;
    }
}

// Remove unmarked strings from the pool (must happen before sweep frees them)
static void pruneStringPool(VM* vm) {
    pthread_mutex_lock(&vm->stringPool.lock);
    for (int i = 0; i < vm->stringPool.count; i++) {
        ObjString* str = (ObjString*)vm->stringPool.strings[i];
        if (!((Obj*)str)->isMarked) {
            // Remove from pool (swap with last)
            vm->stringPool.strings[i] = vm->stringPool.strings[vm->stringPool.count - 1];
            vm->stringPool.hashes[i] = vm->stringPool.hashes[vm->stringPool.count - 1];
            vm->stringPool.count--;
            i--; // Retry this index with swapped element
        }
    }
    pthread_mutex_unlock(&vm->stringPool.lock);
}

static size_t sweep(VM* vm, Obj** listHead) {
    Obj* previous = NULL;
    Obj* object = *listHead;
    size_t freedCount = 0;
    size_t freedBytes = 0;
    
    while (object != NULL) {
        // Permanent objects are always considered marked and their mark is never reset
        if (object->isPermanent || object->isMarked) {
            if (!object->isPermanent) {
                object->isMarked = false;
            }
            previous = object;
            object = object->next;
        } else {
            Obj* unreached = object;
            object = object->next;
            if (previous != NULL) {
                previous->next = object;
            } else {
                vm->objects = object;
            }
            
            // Estimate freed bytes (rough estimation based on object type)
            size_t objSize = sizeof(Obj);
            switch (unreached->type) {
                case OBJ_STRING: objSize = sizeof(ObjString) + ((ObjString*)unreached)->length; break;
                case OBJ_ARRAY: objSize = sizeof(Array) + ((Array*)unreached)->capacity * sizeof(Value); break;
                case OBJ_MAP: objSize = sizeof(Map); break;
                case OBJ_FUNCTION: objSize = sizeof(Function); break;
                case OBJ_ENVIRONMENT: objSize = sizeof(Environment); break;
                default: objSize = sizeof(Obj); break;
            }
            freedBytes += objSize;
            freedCount++;
            
            freeObject(vm, unreached);
        }
    }
    
    return freedBytes;
}

// Helper: get current time in microseconds
static uint64_t getCurrentTimeUs(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

void collectGarbage(VM* vm) {
    uint64_t startTime = getCurrentTimeUs();
    size_t beforeBytes = vm->bytesAllocated;
    
    // Track peak memory
    if (vm->bytesAllocated > vm->gcPeakMemory) {
        vm->gcPeakMemory = vm->bytesAllocated;
    }
    
    // Promote nursery to old generation
    if (vm->nursery) {
        Obj* tail = vm->nursery;
        while (tail->next) tail = tail->next;
        tail->next = vm->objects;
        vm->objects = vm->nursery;
        vm->nursery = NULL;
        vm->nurseryCount = 0;
    }

    // Mark phase
    vm->gcPhase = 1;  // GC_MARKING
    markRoots(vm);
    traceReferences(vm);
    
    // Sweep phase
    pruneStringPool(vm);
    vm->gcPhase = 2;  // GC_SWEEPING
    size_t freedBytes = sweep(vm, &vm->objects);
    vm->gcPhase = 0;  // GC_IDLE
    
    // Update statistics
    uint64_t pauseTime = getCurrentTimeUs() - startTime;
    vm->gcCollectCount++;
    vm->gcTotalPauseUs += pauseTime;
    vm->gcLastPauseUs = pauseTime;
    vm->gcTotalFreed += freedBytes;
    
    // Adaptive threshold: adjust based on heap pressure
    // If we freed a lot (>50% of heap), we can be more relaxed
    // If we freed little (<20%), trigger sooner
    double freedRatio = (beforeBytes > 0) ? (double)freedBytes / (double)beforeBytes : 0.0;
    
    if (freedRatio > 0.5) {
        // Freed a lot - can wait longer
        vm->nextGC = vm->bytesAllocated * 3;
    } else if (freedRatio < 0.2) {
        // Freed little - trigger sooner
        vm->nextGC = vm->bytesAllocated + (vm->bytesAllocated / 2);
    } else {
        // Normal doubling
        vm->nextGC = vm->bytesAllocated * 2;
    }
    
    // Minimum threshold
    if (vm->nextGC < 1024 * 32) {
        vm->nextGC = 1024 * 32;  // 32KB minimum
    }
    // Maximum threshold: cap growth to prevent unbounded heap
    if (vm->nextGC > 1024 * 1024 * 4) {
        vm->nextGC = 1024 * 1024 * 4;  // 4MB maximum
    }
}

// Incremental GC for long-running processes
// workUnits: number of objects to process per call (default: 100)
// Returns: true if collection complete, false if more work needed
bool collectGarbageIncremental(VM* vm, int workUnits) {
    // Phase 0: Not started - begin marking
    if (vm->gcPhase == 0) {
        // Track peak memory
        if (vm->bytesAllocated > vm->gcPeakMemory) {
            vm->gcPeakMemory = vm->bytesAllocated;
        }
        
        vm->gcPhase = 1;  // GC_MARKING
        markRoots(vm);
        // Don't trace yet, will do incrementally
        return false;
    }
    
    // Phase 1: Marking in progress
    if (vm->gcPhase == 1) {
        int remaining = traceReferencesIncremental(vm, workUnits);
        if (remaining == 0) {
            // Marking complete, start sweep
            vm->gcPhase = 2;
        }
        return false;
    }
    
    // Phase 2: Sweeping
    if (vm->gcPhase == 2) {
        pruneStringPool(vm);
        size_t freedBytes = sweep(vm, &vm->objects);
        vm->gcPhase = 0;  // GC_IDLE
        
        // Update statistics
        vm->gcCollectCount++;
        vm->gcTotalFreed += freedBytes;
        
        // Adaptive threshold
        vm->nextGC = vm->bytesAllocated * 2;
        if (vm->nextGC < 1024 * 32) vm->nextGC = 1024 * 32;
        if (vm->nextGC > 1024 * 1024 * 4) vm->nextGC = 1024 * 1024 * 4;
        
        return true;  // Collection complete
    }
    
    return true;
}

// Concurrent GC thread worker
typedef struct {
    VM* vm;
    int workUnits;
} ConcurrentGCArgs;

static void* concurrentMarkWorker(void* arg) {
    ConcurrentGCArgs* args = (ConcurrentGCArgs*)arg;
    VM* vm = args->vm;
    int workUnits = args->workUnits;
    
    pthread_mutex_lock(&gcMutex);
    gcConcurrentActive = 1;
    
    // Roots already marked by main thread (STW) before spawning
    // vm->gcPhase = 1;  // Already set
    
    // Trace incrementally
    while (vm->grayCount > 0) {
        int batch = (vm->grayCount < workUnits) ? vm->grayCount : workUnits;
        for (int i = 0; i < batch && vm->grayCount > 0; i++) {
            Obj* object = vm->grayStack[--vm->grayCount];
            // blackenObject is called inline here
            switch (object->type) {
                case OBJ_FUNCTION: {
                    Function* func = (Function*)object;
                    if (func->closure) markObject(vm, (Obj*)func->closure);
                    if (func->bytecodeChunk) {
                        for (int j = 0; j < func->bytecodeChunk->constantCount; j++) {
                            markValue(vm, func->bytecodeChunk->constants[j]);
                        }
                    }
                    break;
                }
                case OBJ_ARRAY: {
                    Array* arr = (Array*)object;
                    for (int j = 0; j < arr->count; j++) {
                        markValue(vm, arr->items[j]);
                    }
                    break;
                }
                case OBJ_MAP: {
                    Map* m = (Map*)object;
                    for (int j = 0; j < TABLE_SIZE; j++) {
                        MapEntry* e = m->buckets[j];
                        while (e) {
                            markValue(vm, e->value);
                            e = e->next;
                        }
                    }
                    break;
                }
                case OBJ_ENVIRONMENT: {
                    Environment* env = (Environment*)object;
                    if (env->enclosing) markObject(vm, (Obj*)env->enclosing);
                    for (int j = 0; j < TABLE_SIZE; j++) {
                        VarEntry* ve = env->buckets[j];
                        while (ve) { markValue(vm, ve->value); ve = ve->next; }
                    }
                    break;
                }
                default: break;
            }
        }
        // Brief yield to reduce contention
        pthread_mutex_unlock(&gcMutex);
        struct timespec ts = {0, 100000}; // 100 microseconds
        nanosleep(&ts, NULL);
        pthread_mutex_lock(&gcMutex);
    }
    
    // Sweep (must be exclusive)
    pruneStringPool(vm);
    vm->gcPhase = 2;
    size_t freedBytes = sweep(vm, &vm->objects);
    vm->gcPhase = 0;
    
    vm->gcCollectCount++;
    vm->gcTotalFreed += freedBytes;
    vm->gcLastCollectTime = getCurrentTimeUs();
    
    // Adaptive threshold
    vm->nextGC = vm->bytesAllocated * 2;
    if (vm->nextGC < 1024 * 32) vm->nextGC = 1024 * 32;
    if (vm->nextGC > 1024 * 1024 * 4) vm->nextGC = 1024 * 1024 * 4;
    
    gcConcurrentActive = 0;
    pthread_mutex_unlock(&gcMutex);
    
    free(args);
    return NULL;
}

// Start concurrent GC in background thread
void collectGarbageConcurrent(VM* vm, int workUnits) {
    // Don't start if already running
    if (gcConcurrentActive) return;
    
    pthread_mutex_lock(&gcMutex);
    
    // Snapshot: Promote current nursery to Old Generation (vm->objects)
    if (vm->nursery) {
         Obj* tail = vm->nursery;
         while (tail->next) tail = tail->next;
         tail->next = vm->objects;
         vm->objects = vm->nursery;
         vm->nursery = NULL;
         vm->nurseryCount = 0;
    }
    
    pthread_mutex_unlock(&gcMutex);
    
    // Phase 1: Mark Roots (STW - Main Thread)
    // Must be done HERE, before spawning thread, to ensure stack safety.
    vm->gcPhase = 1; // GC_MARKING
    markRoots(vm);
    
    ConcurrentGCArgs* args = malloc(sizeof(ConcurrentGCArgs));
    args->vm = vm;
    args->workUnits = workUnits > 0 ? workUnits : 50;
    
    pthread_t thread;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    
    pthread_create(&thread, &attr, concurrentMarkWorker, args);
    pthread_attr_destroy(&attr);
}

// Check if concurrent GC is active
bool isGCActive(void) {
    return gcConcurrentActive != 0;
}
