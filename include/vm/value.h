#ifndef VM_VALUE_H
#define VM_VALUE_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Unnarize Value Representation
 * 
 * NaN-boxing for efficient 64-bit value encoding
 * Zero external dependencies
 */

// Forward declarations
typedef struct Obj Obj;
typedef struct ObjString ObjString;

// Value types
typedef enum {
    VAL_NIL,
    VAL_BOOL,
    VAL_INT,
    VAL_FLOAT,
    VAL_OBJ
} ValueType;

// Unified value representation
typedef struct {
    ValueType type;
    union {
        bool boolVal;
        int64_t intVal;
        double floatVal;
        Obj* obj;
    };
} Value;

// Value type checking macros
#define IS_NIL(value)    ((value).type == VAL_NIL)
#define IS_BOOL(value)   ((value).type == VAL_BOOL)
#define IS_INT(value)    ((value).type == VAL_INT)
#define IS_FLOAT(value)  ((value).type == VAL_FLOAT)
#define IS_OBJ(value)    ((value).type == VAL_OBJ)

// Value extraction macros
#define AS_BOOL(value)   ((value).boolVal)
#define AS_INT(value)    ((value).intVal)
#define AS_FLOAT(value)  ((value).floatVal)
#define AS_OBJ(value)    ((value).obj)

// Value construction macros
#define NIL_VAL          ((Value){VAL_NIL, {.intVal = 0}})
#define BOOL_VAL(b)      ((Value){VAL_BOOL, {.boolVal = (b)}})
#define INT_VAL(i)       ((Value){VAL_INT, {.intVal = (i)}})
#define FLOAT_VAL(f)     ((Value){VAL_FLOAT, {.floatVal = (f)}})
#define OBJ_VAL(o)       ((Value){VAL_OBJ, {.obj = (Obj*)(o)}})

// Value utilities
bool valuesEqual(Value a, Value b);
void printValue(Value value);

#endif // VM_VALUE_H
