// Example Unnarize external plugin: timeLib (self-contained)
// Provides: nowEpoch(), randInt(n)
// Build: gcc -shared -fPIC -o ../build/timeLib.so timeLib.c -ldl

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
        int booleanVal;
        void* moduleVal;
        void* arrayVal;
        void* mapVal;
    };
} Value;

typedef Value (*NativeFn)(VM*, Value* args, int argCount);
typedef void (*RegisterFn)(VM*, const char* name, NativeFn function);

static int seeded = 0;

static Value nowEpoch(VM* vm, Value* args, int argCount) {
    (void)vm; (void)args; (void)argCount;
    Value v; v.type = VAL_INT; v.intVal = (int)time(NULL);
    return v;
}

static Value randInt(VM* vm, Value* args, int argCount) {
    (void)vm;
    Value v; v.type = VAL_INT; v.intVal = 0;
    if (!seeded) { srand((unsigned)time(NULL)); seeded = 1; }
    if (argCount != 1 || args[0].type != VAL_INT) return v;
    int n = args[0].intVal;
    if (n <= 0) return v;
    v.intVal = rand() % n;
    return v;
}

void registerFunctions(VM* vm) {
    RegisterFn reg_fn = (RegisterFn)dlsym(RTLD_DEFAULT, "registerNativeFunction");
    if (!reg_fn) {
        const char* err = dlerror();
        fprintf(stderr, "[plugin timeLib] dlsym(registerNativeFunction) failed: %s\n", err ? err : "unknown error");
        return;
    }
    reg_fn(vm, "nowEpoch", &nowEpoch);
    reg_fn(vm, "randInt", &randInt);
}
