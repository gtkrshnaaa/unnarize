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
    Module* mod = malloc(sizeof(Module));
    mod->name = strdup("ucoreSystem");
    mod->source = NULL;
    mod->env = malloc(sizeof(Environment));
    memset(mod->env->buckets, 0, sizeof(mod->env->buckets));
    memset(mod->env->funcBuckets, 0, sizeof(mod->env->funcBuckets));

    // Register natives
    {
        FuncEntry* fe = malloc(sizeof(FuncEntry)); fe->key = strdup("args");
        unsigned int h = hash("args", 4);
        fe->next = mod->env->funcBuckets[h]; mod->env->funcBuckets[h] = fe;
        Function* fn = malloc(sizeof(Function)); fn->isNative = true; fn->native = sys_args; fn->paramCount=0;
        fn->name = (Token){TOKEN_IDENTIFIER, fe->key, 4, 0}; fe->function = fn;
    }
    {
        FuncEntry* fe = malloc(sizeof(FuncEntry)); fe->key = strdup("input");
        unsigned int h = hash("input", 5);
        fe->next = mod->env->funcBuckets[h]; mod->env->funcBuckets[h] = fe;
        Function* fn = malloc(sizeof(Function)); fn->isNative = true; fn->native = sys_input; fn->paramCount=1;
        fn->name = (Token){TOKEN_IDENTIFIER, fe->key, 5, 0}; fe->function = fn;
    }
    {
        FuncEntry* fe = malloc(sizeof(FuncEntry)); fe->key = strdup("getenv");
        unsigned int h = hash("getenv", 6);
        fe->next = mod->env->funcBuckets[h]; mod->env->funcBuckets[h] = fe;
        Function* fn = malloc(sizeof(Function)); fn->isNative = true; fn->native = sys_getenv; fn->paramCount=1;
        fn->name = (Token){TOKEN_IDENTIFIER, fe->key, 6, 0}; fe->function = fn;
    }
    {
        FuncEntry* fe = malloc(sizeof(FuncEntry)); fe->key = strdup("fileExists");
        unsigned int h = hash("fileExists", 10);
        fe->next = mod->env->funcBuckets[h]; mod->env->funcBuckets[h] = fe;
        Function* fn = malloc(sizeof(Function)); fn->isNative = true; fn->native = sys_fileExists; fn->paramCount=1;
        fn->name = (Token){TOKEN_IDENTIFIER, fe->key, 10, 0}; fe->function = fn;
    }

    // Add to global
    VarEntry* ve = malloc(sizeof(VarEntry));
    ve->key = strdup("ucoreSystem");
    ve->keyLength = 11;
    ve->ownsKey = true;
    ve->value.type = VAL_MODULE;
    ve->value.moduleVal = mod;
    unsigned int h = hash("ucoreSystem", 11);
    ve->next = vm->globalEnv->buckets[h];
    vm->globalEnv->buckets[h] = ve;
}
