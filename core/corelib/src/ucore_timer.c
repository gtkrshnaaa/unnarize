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
    
    Value v = FLOAT_VAL(ms);
    return v;
}

// Native ucoreTimer.sleep(ms)
static Value utimer_sleep(VM* vm, Value* args, int argCount) {
    (void)vm;
    if (argCount != 1 || !IS_INT(args[0])) {
        printf("Error: ucoreTimer.sleep expects 1 int argument (ms).\n");
        return INT_VAL(0);
    }
    
    int ms = AS_INT(args[0]);
    struct timespec req;
    req.tv_sec = ms / 1000;
    req.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&req, NULL);
    
    return INT_VAL(0);
}

void registerUCoreTimer(VM* vm) {
    ObjString* modNameObj = internString(vm, "ucoreTimer", 10);
    // Use raw chars
    char* modName = modNameObj->chars;
    
    Module* mod = ALLOCATE_OBJ(vm, Module, OBJ_MODULE);
    mod->name = strdup(modName);
    mod->obj.isMarked = true; 
    mod->obj.isPermanent = true; // PERMANENT ROOT
    
    // CRITICAL: Use ALLOCATE_OBJ for Environment to enable GC tracking
    Environment* modEnv = ALLOCATE_OBJ(vm, Environment, OBJ_ENVIRONMENT);
    memset(modEnv->buckets, 0, sizeof(modEnv->buckets));
    memset(modEnv->funcBuckets, 0, sizeof(modEnv->funcBuckets));
    modEnv->enclosing = NULL;
    modEnv->obj.isMarked = true; 
    modEnv->obj.isPermanent = true; // PERMANENT ROOT
    mod->env = modEnv;

    defineNative(vm, mod->env, "now", utimer_now, 0);
    defineNative(vm, mod->env, "sleep", utimer_sleep, 1);

    Value vMod = OBJ_VAL(mod);
    defineGlobal(vm, "ucoreTimer", vMod);
}
