#include "ucore_timer.h"
#include <time.h>
#include <unistd.h>
#include <stdio.h>

// Native ucoreTimer.now()
// Returns current monotonic time in milliseconds (double)
static Value utimer_now(VM* vm, Value* args, int argCount) {
    (void)vm; (void)args; (void)argCount;
    
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    
    // Convert to milliseconds
    double ms = (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
    
    Value v;
    v.type = VAL_FLOAT;
    v.floatVal = ms;
    return v;
}

// Native ucoreTimer.sleep(ms)
static Value utimer_sleep(VM* vm, Value* args, int argCount) {
    (void)vm;
    if (argCount != 1 || args[0].type != VAL_INT) {
        printf("Error: ucoreTimer.sleep expects 1 int argument (ms).\n");
        return (Value){VAL_INT, .intVal = 0};
    }
    
    int ms = args[0].intVal;
    struct timespec req;
    req.tv_sec = ms / 1000;
    req.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&req, NULL);
    
    return (Value){VAL_INT, .intVal = 0};
}

void registerUCoreTimer(VM* vm) {
    ObjString* modNameObj = internString(vm, "ucoreTimer", 10);
    // Use raw chars
    char* modName = modNameObj->chars;
    
    Module* mod = ALLOCATE_OBJ(vm, Module, OBJ_MODULE);
    mod->name = strdup(modName);
    mod->env = malloc(sizeof(Environment));
    if (!mod->env) return; 
    memset(mod->env->buckets, 0, sizeof(mod->env->buckets));
    memset(mod->env->funcBuckets, 0, sizeof(mod->env->funcBuckets));
    mod->env->enclosing = NULL;

    defineNative(vm, mod->env, "now", utimer_now, 0);
    defineNative(vm, mod->env, "sleep", utimer_sleep, 1);

    Value vMod; vMod.type = VAL_OBJ; vMod.obj = (Obj*)mod;
    defineGlobal(vm, "ucoreTimer", vMod);
}
