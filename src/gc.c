#include "vm.h"
#include <stdlib.h>
#include <stdio.h>
#include "bytecode/chunk.h"

// GC Helpers
void markObject(VM* vm, Obj* object);
void markValue(VM* vm, Value value);
static void markRoots(VM* vm);
static void traceReferences(VM* vm);
static void sweep(VM* vm);
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
    
    object->isMarked = true;
    
    if (vm->grayCapacity < vm->grayCount + 1) {
        vm->grayCapacity = GROW_CAPACITY(vm->grayCapacity);
        vm->grayStack = (Obj**)realloc(vm->grayStack, sizeof(Obj*) * vm->grayCapacity);
        if (vm->grayStack == NULL) exit(1);
    }
    
    vm->grayStack[vm->grayCount++] = object;
}

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
            
            // Mark values in buckets
            for (int i = 0; i < TABLE_SIZE; i++) {
                VarEntry* entry = env->buckets[i];
                while (entry) {
                    markValue(vm, entry->value);
                    entry = entry->next;
                }
                
                // Functions in env?
                FuncEntry* funcEntry = env->funcBuckets[i];
                while (funcEntry) {
                    if (funcEntry->function) markObject(vm, (Obj*)funcEntry->function);
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

static void sweep(VM* vm) {
    Obj* previous = NULL;
    Obj* object = vm->objects;
    while (object != NULL) {
        if (object->isMarked) {
            object->isMarked = false;
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
            freeObject(vm, unreached);
        }
    }
}

void collectGarbage(VM* vm) {
    markRoots(vm);
    traceReferences(vm);
    sweep(vm);
    vm->nextGC = vm->bytesAllocated * 2;
}
