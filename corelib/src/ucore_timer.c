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
    // Create Module
    Module* mod = malloc(sizeof(Module));
    if (!mod) return;
    mod->name = strdup("ucoreTimer");
    mod->env = malloc(sizeof(Environment));
    if (!mod->env) return; // Should free mod
    memset(mod->env->buckets, 0, sizeof(mod->env->buckets));
    memset(mod->env->funcBuckets, 0, sizeof(mod->env->funcBuckets));

    // Register now()
    unsigned int hNow = hash("now", 3);
    FuncEntry* feNow = malloc(sizeof(FuncEntry)); feNow->key = strdup("now");
    feNow->next = mod->env->funcBuckets[hNow]; mod->env->funcBuckets[hNow] = feNow;
    Function* fnNow = malloc(sizeof(Function)); fnNow->isNative = true; fnNow->native = utimer_now; fnNow->paramCount = 0;
    fnNow->name = (Token){TOKEN_IDENTIFIER, feNow->key, 3, 0}; feNow->function = fnNow;

    // Register sleep(ms)
    unsigned int hSleep = hash("sleep", 5);
    FuncEntry* feSleep = malloc(sizeof(FuncEntry)); feSleep->key = strdup("sleep");
    feSleep->next = mod->env->funcBuckets[hSleep]; mod->env->funcBuckets[hSleep] = feSleep;
    Function* fnSleep = malloc(sizeof(Function)); fnSleep->isNative = true; fnSleep->native = utimer_sleep; fnSleep->paramCount = 1;
    fnSleep->name = (Token){TOKEN_IDENTIFIER, feSleep->key, 5, 0}; feSleep->function = fnSleep;

    // Add ucoreTimer to global environment
    VarEntry* ve = malloc(sizeof(VarEntry));
    ve->key = strdup("ucoreTimer");
    ve->keyLength = 10;
    ve->ownsKey = true;
    ve->value.type = VAL_MODULE; ve->value.moduleVal = mod;
    unsigned int h = hash("ucoreTimer", 10);
    ve->next = vm->globalEnv->buckets[h];
    vm->globalEnv->buckets[h] = ve;
}
