#include "ucore_system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// args() -> ["arg1", "arg2"]
// args() -> ["arg1", "arg2"]
static Value sys_args(VM* vm, Value* args, int argCount) {
    (void)args; (void)argCount;
    Array* arr = newArray(vm);
    for (int i = 0; i < vm->argc; i++) {
        ObjString* s = internString(vm, vm->argv[i], strlen(vm->argv[i]));
        Value v; v.type = VAL_OBJ; v.obj = (Obj*)s;
        arrayPush(vm, arr, v);
    }
    Value vArr; vArr.type = VAL_OBJ; vArr.obj = (Obj*)arr;
    return vArr;
}

// input(prompt) -> string
static Value sys_input(VM* vm, Value* args, int argCount) {
    if (argCount > 0 && IS_STRING(args[0])) {
        printf("%s", AS_CSTRING(args[0]));
        fflush(stdout); // Ensure prompt is visible
    }
    
    char* line = NULL;
    size_t len = 0;
    ssize_t read;
    
    read = getline(&line, &len, stdin);
    if (read != -1) {
        // Remove newline
        int realLen = read;
        if (realLen > 0 && line[realLen-1] == '\n') {
            line[realLen-1] = '\0';
            realLen--;
        }
        ObjString* s = internString(vm, line, realLen);
        free(line); 
        Value v; v.type = VAL_OBJ; v.obj = (Obj*)s;
        return v;
    }
    
    // EOF or error
    free(line);
    // Return empty string
    ObjString* empty = internString(vm, "", 0);
    Value v; v.type = VAL_OBJ; v.obj = (Obj*)empty;
    return v;
}

// getenv(name) -> string or ""
static Value sys_getenv(VM* vm, Value* args, int argCount) {
    if (argCount != 1 || !IS_STRING(args[0])) {
        ObjString* empty = internString(vm, "", 0);
        return (Value){VAL_OBJ, .obj=(Obj*)empty};
    }
    
    char* val = getenv(AS_CSTRING(args[0]));
    if (val) {
        ObjString* s = internString(vm, val, strlen(val));
        return (Value){VAL_OBJ, .obj=(Obj*)s};
    }
    ObjString* empty = internString(vm, "", 0);
    return (Value){VAL_OBJ, .obj=(Obj*)empty};
}

// fileExists(path) -> bool
static Value sys_fileExists(VM* vm, Value* args, int argCount) {
    if (argCount != 1 || !IS_STRING(args[0])) return (Value){VAL_BOOL, .boolVal=false};
    
    struct stat st;
    if (stat(AS_CSTRING(args[0]), &st) == 0) {
        return (Value){VAL_BOOL, .boolVal=true};
    }
    return (Value){VAL_BOOL, .boolVal=false};
}


void registerUCoreSystem(VM* vm) {
    ObjString* modNameObj = internString(vm, "ucoreSystem", 11);
    char* modName = modNameObj->chars;
    
    Module* mod = ALLOCATE_OBJ(vm, Module, OBJ_MODULE);
    mod->name = strdup(modName);
    mod->source = NULL;
    mod->env = malloc(sizeof(Environment));
    if (!mod->env) error("Memory allocation failed env.", 0);
    memset(mod->env->buckets, 0, sizeof(mod->env->buckets));
    memset(mod->env->funcBuckets, 0, sizeof(mod->env->funcBuckets));
    mod->env->enclosing = NULL;

    defineNative(vm, mod->env, "args", sys_args, 0);
    defineNative(vm, mod->env, "input", sys_input, 1);
    defineNative(vm, mod->env, "getenv", sys_getenv, 1);
    defineNative(vm, mod->env, "fileExists", sys_fileExists, 1);

    Value vMod; vMod.type = VAL_OBJ; vMod.obj = (Obj*)mod;
    defineGlobal(vm, "ucoreSystem", vMod);
}
