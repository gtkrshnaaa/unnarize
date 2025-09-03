// Timer plugin for Unnarize performance benchmarking
// Provides: timerStart(), timerEnd(), timerElapsed()
// Build: gcc -shared -fPIC -o ../build/timerLib.so timerLib.c -ldl

#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>

// Minimal mirror of interpreter public types
typedef struct VM VM; // opaque

typedef enum {
    VAL_INT,
    VAL_FLOAT,
    VAL_STRING,
    VAL_BOOL,
    VAL_MODULE,
    VAL_ARRAY,
    VAL_MAP
} ValueType;

typedef struct Value {
    ValueType type;
    union {
        int intVal;
        double floatVal;
        char* stringVal;
        int bool_storage;
        void* moduleVal;
        void* arrayVal;
        void* mapVal;
    };
} Value;

typedef Value (*NativeFn)(VM*, Value* args, int argCount);
typedef void (*RegisterFn)(VM*, const char* name, NativeFn function);

// Global timer storage
static struct timespec start_time;
static struct timespec end_time;

// Get current time in nanoseconds
static long long getCurrentTimeNs() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

// Start timer - returns current time in nanoseconds
static Value timerStart(VM* vm, Value* args, int argCount) {
    (void)vm; (void)args; (void)argCount;
    
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    
    Value v;
    v.type = VAL_INT;
    v.intVal = (int)(getCurrentTimeNs() / 1000000); // Return milliseconds
    return v;
}

// End timer - returns current time in nanoseconds
static Value timerEnd(VM* vm, Value* args, int argCount) {
    (void)vm; (void)args; (void)argCount;
    
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    
    Value v;
    v.type = VAL_INT;
    v.intVal = (int)(getCurrentTimeNs() / 1000000); // Return milliseconds
    return v;
}

// Get elapsed time in milliseconds since timerStart()
static Value timerElapsed(VM* vm, Value* args, int argCount) {
    (void)vm; (void)args; (void)argCount;
    
    struct timespec current;
    clock_gettime(CLOCK_MONOTONIC, &current);
    
    long long elapsed_ns = (current.tv_sec - start_time.tv_sec) * 1000000000LL + 
                          (current.tv_nsec - start_time.tv_nsec);
    
    Value v;
    v.type = VAL_INT;
    v.intVal = (int)(elapsed_ns / 1000000); // Convert to milliseconds
    return v;
}

// Get elapsed time in microseconds for high precision
static Value timerElapsedMicros(VM* vm, Value* args, int argCount) {
    (void)vm; (void)args; (void)argCount;
    
    struct timespec current;
    clock_gettime(CLOCK_MONOTONIC, &current);
    
    long long elapsed_ns = (current.tv_sec - start_time.tv_sec) * 1000000000LL + 
                          (current.tv_nsec - start_time.tv_nsec);
    
    Value v;
    v.type = VAL_INT;
    v.intVal = (int)(elapsed_ns / 1000); // Convert to microseconds
    return v;
}

// Get current timestamp in milliseconds
static Value timerNow(VM* vm, Value* args, int argCount) {
    (void)vm; (void)args; (void)argCount;
    
    Value v;
    v.type = VAL_INT;
    v.intVal = (int)(getCurrentTimeNs() / 1000000);
    return v;
}

void registerFunctions(VM* vm) {
    RegisterFn reg_fn = (RegisterFn)dlsym(RTLD_DEFAULT, "registerNativeFunction");
    if (!reg_fn) {
        const char* err = dlerror();
        fprintf(stderr, "[plugin timerLib] dlsym(registerNativeFunction) failed: %s\n", err ? err : "unknown error");
        return;
    }
    
    reg_fn(vm, "timerStart", &timerStart);
    reg_fn(vm, "timerEnd", &timerEnd);
    reg_fn(vm, "timerElapsed", &timerElapsed);
    reg_fn(vm, "timerElapsedMicros", &timerElapsedMicros);
    reg_fn(vm, "timerNow", &timerNow);
}
