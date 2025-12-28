#include "ucore_system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// args() -> ["arg1", "arg2"]
static Value sys_args(VM* vm, Value* args, int argCount) {
    (void)args; (void)argCount;
    Array* arr = newArray();
    for (int i = 0; i < vm->argc; i++) {
        Value v;
        v.type = VAL_STRING;
        v.stringVal = strdup(vm->argv[i]);
        arrayPush(arr, v);
    }
    Value vArr; vArr.type = VAL_ARRAY; vArr.arrayVal = arr;
    return vArr;
}

// input(prompt) -> string
static Value sys_input(VM* vm, Value* args, int argCount) {
    (void)vm;
    if (argCount > 0 && args[0].type == VAL_STRING) {
        printf("%s", args[0].stringVal);
        fflush(stdout); // Ensure prompt is visible
    }
    
    char* line = NULL;
    size_t len = 0;
    ssize_t read;
    
    read = getline(&line, &len, stdin);
    if (read != -1) {
        // Remove newline
        if (read > 0 && line[read-1] == '\n') {
            line[read-1] = '\0';
        }
        Value v; v.type = VAL_STRING; v.stringVal = line; // line is malloc'd by getline
        return v;
    }
    
    // EOF or error
    free(line); 
    return (Value){VAL_STRING, .stringVal = strdup("")};
}

// getenv(name) -> string or ""
static Value sys_getenv(VM* vm, Value* args, int argCount) {
    (void)vm;
    if (argCount != 1 || args[0].type != VAL_STRING) return (Value){VAL_STRING, .stringVal=strdup("")};
    
    char* val = getenv(args[0].stringVal);
    if (val) {
        return (Value){VAL_STRING, .stringVal=strdup(val)};
    }
    return (Value){VAL_STRING, .stringVal=strdup("")};
}

// fileExists(path) -> bool
static Value sys_fileExists(VM* vm, Value* args, int argCount) {
    (void)vm;
    if (argCount != 1 || args[0].type != VAL_STRING) return (Value){VAL_BOOL, .boolVal=false};
    
    struct stat st;
    if (stat(args[0].stringVal, &st) == 0) {
        return (Value){VAL_BOOL, .boolVal=true};
    }
    return (Value){VAL_BOOL, .boolVal=false};
}


void registerUCoreSystem(VM* vm) {
    char* modName = internString(vm, "ucoreSystem", 11);
    Module* mod = malloc(sizeof(Module));
    mod->name = modName;
    mod->source = NULL;
    mod->env = malloc(sizeof(Environment));
    memset(mod->env->buckets, 0, sizeof(mod->env->buckets));
    memset(mod->env->funcBuckets, 0, sizeof(mod->env->funcBuckets));

    defineNative(vm, mod->env, "args", sys_args, 0);
    defineNative(vm, mod->env, "input", sys_input, 1);
    defineNative(vm, mod->env, "getenv", sys_getenv, 1);
    defineNative(vm, mod->env, "fileExists", sys_fileExists, 1);

    Value vMod; vMod.type = VAL_MODULE; vMod.moduleVal = mod;
    defineGlobal(vm, "ucoreSystem", vMod);
}
