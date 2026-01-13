#include <time.h>
#include <stdio.h>
#include "../../include/vm.h"
#include "../include/ucore_benchmark.h"

// Helper: Get monotonic time in seconds (double)
static double get_time_sec() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

// ucoreBenchmark.start() -> Returns current timestamp (float, seconds)
static Value ubench_start(VM* vm, Value* args, int argCount) {
    (void)vm; (void)args; (void)argCount;
    return FLOAT_VAL(get_time_sec());
}

// ucoreBenchmark.end(startTime) -> Returns elapsed time in seconds (float)
// Or just returns end timestamp?
// User said: "result... didasarkan data yang di dapat dari start dan end"
// Common pattern: start returns T1, end returns T2.
// Let's make end() return current timestamp T2.
static Value ubench_end(VM* vm, Value* args, int argCount) {
    (void)vm; (void)args; (void)argCount;
    return FLOAT_VAL(get_time_sec());
}

// ucoreBenchmark.result(start, end, iterations) -> String "X ops/sec"
static Value ubench_result(VM* vm, Value* args, int argCount) {
    (void)vm;
    if (argCount != 3) {
        printf("Error: ucoreBenchmark.result expects 3 arguments (start, end, iterations).\n");
        return NIL_VAL;
    }
    
    double start = AS_FLOAT(args[0]);
    double end = AS_FLOAT(args[1]);
    double iterations = AS_FLOAT(args[2]); // Auto-coerced or explicit? Unnarize ints are values.
    
    if (IS_INT(args[2])) {
        iterations = (double)AS_INT(args[2]);
    }
    
    double duration = end - start;
    if (duration <= 0) duration = 0.000001; // Prevent div by zero
    
    double ops_sec = iterations / duration;
    
    // Format string
    char buf[128];
    // Formats: 1.2M ops/sec, 500k ops/sec, etc. or just raw?
    // Let's do raw for now, or nice readable format.
    if (ops_sec > 1e9) snprintf(buf, sizeof(buf), "%.2fG ops/sec", ops_sec / 1e9);
    else if (ops_sec > 1e6) snprintf(buf, sizeof(buf), "%.2fM ops/sec", ops_sec / 1e6);
    else if (ops_sec > 1e3) snprintf(buf, sizeof(buf), "%.2fk ops/sec", ops_sec / 1e3);
    else snprintf(buf, sizeof(buf), "%.2f ops/sec", ops_sec);
    
    return OBJ_VAL(internString(vm, buf, strlen(buf)));
}

void registerUCoreBenchmark(VM* vm) {
    ObjString* modNameObj = internString(vm, "ucoreBenchmark", 14);
    char* modName = modNameObj->chars;
    
    Module* mod = ALLOCATE_OBJ(vm, Module, OBJ_MODULE);
    mod->name = strdup(modName);
    mod->env = malloc(sizeof(Environment));
    if (!mod->env) return;
    memset(mod->env->buckets, 0, sizeof(mod->env->buckets));
    memset(mod->env->funcBuckets, 0, sizeof(mod->env->funcBuckets));
    mod->env->enclosing = NULL;

    defineNative(vm, mod->env, "start", ubench_start, 0);
    defineNative(vm, mod->env, "end", ubench_end, 0);
    defineNative(vm, mod->env, "result", ubench_result, 3);
    
    Value vMod = OBJ_VAL(mod);
    defineGlobal(vm, "ucoreBenchmark", vMod);
}
