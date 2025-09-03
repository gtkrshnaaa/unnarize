// Example Unnarize external plugin: mathLib (self-contained)
// Provides a native function: mathPower(base, exp)
// Build: gcc -shared -fPIC -o ../build/mathLib.so mathLib.c -lm -ldl

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>

// Minimal mirror of interpreter public types (opaque or layout-compatible)
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
        int bool_storage; // keep footprint consistent; not used here
        void* moduleVal;
        void* arrayVal;
        void* mapVal;
    };
} Value;

typedef Value (*NativeFn)(VM*, Value* args, int argCount);
typedef void (*RegisterFn)(VM*, const char* name, NativeFn function);

static Value mathPower(VM* vm, Value* args, int argCount) {
    (void)vm; // unused
    Value out; out.type = VAL_FLOAT; out.floatVal = 0.0;
    if (argCount != 2) {
        return out;
    }
    double a = 0.0, b = 0.0;
    if (args[0].type == VAL_INT) a = (double)args[0].intVal;
    else if (args[0].type == VAL_FLOAT) a = args[0].floatVal;
    else return out;

    if (args[1].type == VAL_INT) b = (double)args[1].intVal;
    else if (args[1].type == VAL_FLOAT) b = args[1].floatVal;
    else return out;

    out.floatVal = pow(a, b);
    return out;
}

// Mandatory entry point called by the interpreter
void registerFunctions(VM* vm) {
    // Resolve interpreter's registration API dynamically
    RegisterFn reg_fn = (RegisterFn)dlsym(RTLD_DEFAULT, "registerNativeFunction");
    if (!reg_fn) {
        const char* err = dlerror();
        fprintf(stderr, "[plugin mathLib] dlsym(registerNativeFunction) failed: %s\n", err ? err : "unknown error");
        return;
    }
    reg_fn(vm, "mathPower", &mathPower);
}
