// File: mycalcLib.c
// Self-contained native plugin for Unnarize (exclusive to docssource)
// Builds into a shared library: mycalcLib.so
// Functions exported to Unnarize:
//  - add3(a, b, c) -> int
//  - hypot2(a, b) -> float (sqrt(a^2 + b^2))

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>

// Minimal mirrors of interpreter public types (do NOT include project headers)
typedef struct VM VM; // opaque

typedef enum {
    VAL_INT,
    VAL_FLOAT,
    VAL_STRING,
    VAL_BOOL,
    VAL_MODULE,
    VAL_ARRAY,
    VAL_MAP,
    VAL_FUTURE
} ValueType;

typedef struct Value {
    ValueType type;
    union {
        int intVal;
        double floatVal;
        char* stringVal;
        int boolVal;
        void* moduleVal;
        void* arrayVal;
        void* mapVal;
        void* futureVal;
    };
} Value;

typedef Value (*NativeFn)(VM*, Value* args, int argCount);
typedef void (*RegisterFn)(VM*, const char* name, NativeFn function);

static Value makeInt(int v) {
    Value out; out.type = VAL_INT; out.intVal = v; return out;
}
static Value makeFloat(double v) {
    Value out; out.type = VAL_FLOAT; out.floatVal = v; return out;
}

// add3(a, b, c) -> int
static Value add3Fn(VM* vm, Value* args, int argCount) {
    (void)vm;
    if (argCount != 3) return makeInt(0);
    int a = 0, b = 0, c = 0;
    if (args[0].type == VAL_INT) a = args[0].intVal; else if (args[0].type == VAL_FLOAT) a = (int)args[0].floatVal; else return makeInt(0);
    if (args[1].type == VAL_INT) b = args[1].intVal; else if (args[1].type == VAL_FLOAT) b = (int)args[1].floatVal; else return makeInt(0);
    if (args[2].type == VAL_INT) c = args[2].intVal; else if (args[2].type == VAL_FLOAT) c = (int)args[2].floatVal; else return makeInt(0);
    return makeInt(a + b + c);
}

// hypot2(a, b) -> float
static Value hypot2Fn(VM* vm, Value* args, int argCount) {
    (void)vm;
    if (argCount != 2) return makeFloat(0.0);
    double a = 0.0, b = 0.0;
    if (args[0].type == VAL_INT) a = (double)args[0].intVal; else if (args[0].type == VAL_FLOAT) a = args[0].floatVal; else return makeFloat(0.0);
    if (args[1].type == VAL_INT) b = (double)args[1].intVal; else if (args[1].type == VAL_FLOAT) b = args[1].floatVal; else return makeFloat(0.0);
    return makeFloat(sqrt(a*a + b*b));
}

// Entry point required by Unnarize
void registerFunctions(VM* vm) {
    // Resolve interpreter registration API dynamically
    RegisterFn reg = (RegisterFn)dlsym(RTLD_DEFAULT, "registerNativeFunction");
    if (!reg) {
        const char* err = dlerror();
        fprintf(stderr, "[plugin mycalcLib] dlsym(registerNativeFunction) failed: %s\n", err ? err : "unknown error");
        return;
    }
    reg(vm, "add3", add3Fn);
    reg(vm, "hypot2", hypot2Fn);
}
