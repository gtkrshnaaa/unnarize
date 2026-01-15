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
        Value v = OBJ_VAL(s);
        arrayPush(vm, arr, v);
    }
    Value vArr = OBJ_VAL(arr);
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
        Value v = OBJ_VAL(s);
        return v;
    }
    
    // EOF or error
    free(line);
    // Return empty string
    ObjString* empty = internString(vm, "", 0);
    Value v = OBJ_VAL(empty);
    return v;
}

// getenv(name) -> string or ""
static Value sys_getenv(VM* vm, Value* args, int argCount) {
    if (argCount != 1 || !IS_STRING(args[0])) {
        ObjString* empty = internString(vm, "", 0);
        return OBJ_VAL(empty);
    }
    
    char* val = getenv(AS_CSTRING(args[0]));
    if (val) {
        ObjString* s = internString(vm, val, strlen(val));
        return OBJ_VAL(s);
    }
    ObjString* empty = internString(vm, "", 0);
    return OBJ_VAL(empty);
}

// fileExists(path) -> bool
static Value sys_fileExists(VM* vm, Value* args, int argCount) {
    if (argCount != 1 || !IS_STRING(args[0])) return BOOL_VAL(false);
    (void)vm;
    
    struct stat st;
    if (stat(AS_CSTRING(args[0]), &st) == 0) {
        return BOOL_VAL(true);
    }
    return BOOL_VAL(false);
}

// writeFile(path, content) -> bool
static Value sys_writeFile(VM* vm, Value* args, int argCount) {
    (void)vm;
    if (argCount != 2 || !IS_STRING(args[0]) || !IS_STRING(args[1])) {
        printf("Error: ucoreSystem.writeFile(path, content) expects 2 string arguments.\n");
        return BOOL_VAL(false);
    }
    
    char* path = AS_CSTRING(args[0]);
    char* content = AS_CSTRING(args[1]);
    
    FILE* f = fopen(path, "w");
    if (!f) {
        printf("Error: Could not open file for writing: %s\n", path);
        return BOOL_VAL(false);
    }
    
    fprintf(f, "%s", content);
    fclose(f);
    
    return BOOL_VAL(true);
}

// readFile(path) -> string or ""
static Value sys_readFile(VM* vm, Value* args, int argCount) {
    if (argCount != 1 || !IS_STRING(args[0])) {
        ObjString* empty = internString(vm, "", 0);
        return OBJ_VAL(empty);
    }
    
    char* path = AS_CSTRING(args[0]);
    FILE* f = fopen(path, "r");
    if (!f) {
        ObjString* empty = internString(vm, "", 0);
        return OBJ_VAL(empty);
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* buf = malloc(size + 1);
    if (!buf) {
        fclose(f);
        ObjString* empty = internString(vm, "", 0);
        return OBJ_VAL(empty);
    }
    
    size_t bytesRead = fread(buf, 1, size, f);
    buf[bytesRead] = '\0';
    fclose(f);
    
    ObjString* s = internString(vm, buf, (int)size);
    free(buf);
    return OBJ_VAL(s);
}


// exit(code)
static Value sys_exit(VM* vm, Value* args, int argCount) {
    int code = 0;
    if (argCount > 0) {
        if (IS_INT(args[0])) code = AS_INT(args[0]);
        else if (IS_FLOAT(args[0])) code = (int)AS_FLOAT(args[0]);
    }
    exit(code);
    return NIL_VAL; 
}

// exec(command) -> exitCode (int)
static Value sys_exec(VM* vm, Value* args, int argCount) {
    if (argCount != 1 || !IS_STRING(args[0])) {
        return INT_VAL(-1);
    }
    
    ObjString* cmd = AS_STRING(args[0]);
    int result = system(cmd->chars);
    return INT_VAL(result); // Usually 0 is success
}

void registerUCoreSystem(VM* vm) {
    ObjString* modNameObj = internString(vm, "ucoreSystem", 11);
    char* modName = modNameObj->chars;
    
    Module* mod = ALLOCATE_OBJ(vm, Module, OBJ_MODULE);
    mod->name = strdup(modName);
    mod->source = NULL;
    mod->obj.isMarked = true; // PERMANENT ROOT
    
    // CRITICAL: Use ALLOCATE_OBJ for Environment to enable GC tracking
    Environment* modEnv = ALLOCATE_OBJ(vm, Environment, OBJ_ENVIRONMENT);
    memset(modEnv->buckets, 0, sizeof(modEnv->buckets));
    memset(modEnv->funcBuckets, 0, sizeof(modEnv->funcBuckets));
    modEnv->enclosing = NULL;
    modEnv->obj.isMarked = true; 
    modEnv->obj.isPermanent = true; // PERMANENT ROOT
    mod->env = modEnv;

    defineNative(vm, mod->env, "args", sys_args, 0);
    defineNative(vm, mod->env, "input", sys_input, 1);
    defineNative(vm, mod->env, "getenv", sys_getenv, 1);
    defineNative(vm, mod->env, "exec", sys_exec, 1);
    defineNative(vm, mod->env, "exit", sys_exit, 1);
    defineNative(vm, mod->env, "fileExists", sys_fileExists, 1);
    defineNative(vm, mod->env, "writeFile", sys_writeFile, 2);
    defineNative(vm, mod->env, "readFile", sys_readFile, 1);

    Value vMod = OBJ_VAL(mod);
    defineGlobal(vm, "ucoreSystem", vMod);
}
