#ifndef UNNARIZE_EXTERN_H
#define UNNARIZE_EXTERN_H

// Minimal public API for Unnarize external libraries (.so)
// This header is intentionally small and stable so plugin authors
// don't need to include internal interpreter headers.

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque VM forward declaration
typedef struct VM VM;

// Public Value types (must mirror interpreter's)
typedef enum {
    VAL_INT,
    VAL_FLOAT,
    VAL_STRING,
    VAL_BOOL,
    VAL_MODULE,
    VAL_ARRAY,
    VAL_MAP
} ValueType;

// Public Value layout (must mirror interpreter's)
typedef struct Module Module;   // opaque for plugins
typedef struct Array Array;     // opaque for plugins
typedef struct Map Map;         // opaque for plugins

typedef struct Value {
    ValueType type;
    union {
        int intVal;
        double floatVal;
        char* stringVal; // plugins may allocate strings with malloc/strdup
        bool boolVal;
        Module* moduleVal;
        Array* arrayVal;
        Map* mapVal;
    };
} Value;

// Native function signature for functions exposed to Unnarize
typedef Value (*NativeFn)(VM*, Value* args, int argCount);

// Registration API exported by the interpreter binary
void registerNativeFunction(VM* vm, const char* name, NativeFn function);

// Entry point that each external library must export
void register_functions(VM* vm);

#ifdef __cplusplus
}
#endif

#endif // UNNARIZE_EXTERN_H
