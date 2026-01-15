# NaN Boxing

> Efficient 64-bit value representation without heap allocation.

---

## Overview

NaN Boxing is a technique that encodes multiple value types within a 64-bit IEEE 754 floating-point number by exploiting the structure of NaN (Not a Number) values.

### Benefits

1. **No heap allocation** for primitives (int, bool, nil)
2. **Uniform size** - All values are exactly 8 bytes
3. **Fast type checking** - Bit masking operations
4. **Cache friendly** - Values fit in registers

---

## IEEE 754 Double Structure

```
┌─────┬─────────────┬────────────────────────────────────────────────┐
│Sign │  Exponent   │                 Mantissa                        │
│ 1b  │    11b      │                  52b                            │
└─────┴─────────────┴────────────────────────────────────────────────┘
  63   62       52   51                                              0
```

### Special Values

| Pattern | Meaning |
|---------|---------|
| Exponent = 0, Mantissa = 0 | +0.0 or -0.0 |
| Exponent = 2047, Mantissa = 0 | ±Infinity |
| Exponent = 2047, Mantissa ≠ 0 | NaN |

### Key Insight

There are 2^52 distinct NaN bit patterns! We use these to encode other types.

---

## Unnarize Encoding

### Constants

```c
typedef uint64_t Value;

#define SIGN_BIT ((uint64_t)0x8000000000000000)  // Bit 63
#define QNAN     ((uint64_t)0x7ffc000000000000)  // Quiet NaN pattern
```

### Type Tags

| Type | Tag Pattern | Full Pattern |
|------|-------------|--------------|
| Float | None | IEEE 754 double as-is |
| Object | QNAN \| SIGN_BIT \| ptr | 0xFFFC... + pointer |
| Integer | QNAN \| 0x0001... \| int32 | 0x7FFD... + 32-bit int |
| nil | QNAN \| 0x0002... | Fixed value |
| false | QNAN \| 0x0003... | Fixed value |
| true | QNAN \| 0x0003...01 | Fixed value |

---

## Encoding Details

### Float (Double)

Stored directly as IEEE 754:

```c
#define IS_NUMBER(v)  (((v) & QNAN) != QNAN)
#define IS_FLOAT(v)   IS_NUMBER(v)
#define AS_FLOAT(v)   valueToNum(v)
#define FLOAT_VAL(v)  numToValue(v)

static inline double valueToNum(Value v) {
    double d;
    memcpy(&d, &v, sizeof(Value));
    return d;
}
```

### Object (Pointer)

Uses high bits for tag, low 48 bits for pointer:

```c
#define IS_OBJ(v)     (((v) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))
#define AS_OBJ(v)     ((Obj*)(uintptr_t)((v) & ~(SIGN_BIT | QNAN)))
#define OBJ_VAL(obj)  ((Value)(SIGN_BIT | QNAN | (uint64_t)(uintptr_t)(obj)))
```

### Integer

32-bit integer stored in lower bits:

```c
#define TAG_INT_BIT   ((uint64_t)0x0001000000000000)

#define IS_INT(v)     (((v) & (QNAN | TAG_INT_BIT)) == (QNAN | TAG_INT_BIT))
#define AS_INT(v)     ((int32_t)(v & 0xFFFFFFFF))
#define INT_VAL(num)  ((Value)(QNAN | TAG_INT_BIT | ((uint32_t)(num))))
```

### Boolean

Two distinct patterns for true/false:

```c
#define TAGGED_FALSE  ((Value)(QNAN | 0x0003000000000000))
#define TAGGED_TRUE   ((Value)(QNAN | 0x0003000000000001))

#define IS_BOOL(v)    (((v) & 0xFFFFFFFFFFFFFFFE) == TAGGED_FALSE)
#define AS_BOOL(v)    ((v) == TAGGED_TRUE)
#define BOOL_VAL(b)   ((b) ? TAGGED_TRUE : TAGGED_FALSE)
```

### nil

Single fixed value:

```c
#define TAGGED_NIL    ((Value)(QNAN | 0x0002000000000000))

#define IS_NIL(v)     ((v) == TAGGED_NIL)
#define NIL_VAL       TAGGED_NIL
```

---

## Bit Layout Visualization

```
Float:   0xxx xxxx xxxx xxxx ... (not a NaN)
Object:  1111 1111 1111 11xx [48-bit pointer]
Integer: 0111 1111 1111 1101 [32-bit integer]
nil:     0111 1111 1111 1100 0000 0010 0...0
false:   0111 1111 1111 1100 0000 0011 0...0
true:    0111 1111 1111 1100 0000 0011 0...1
```

---

## Type Checking

Fast O(1) type checks with bit operations:

```c
static inline ValueType getValueType(Value v) {
    if (IS_INT(v)) return VAL_INT;
    if (IS_OBJ(v)) return VAL_OBJ;
    if (IS_BOOL(v)) return VAL_BOOL;
    if (IS_NIL(v)) return VAL_NIL;
    return VAL_FLOAT;
}
```

### Check Order Optimization

Checks are ordered by frequency:
1. Integer (most common in loops)
2. Object (strings, arrays, etc.)
3. Boolean
4. nil
5. Float

---

## Advantages Over Tagged Pointers

| NaN Boxing | Tagged Pointers |
|------------|-----------------|
| Full 32-bit int range | Limited int range |
| Full 64-bit float | Float requires heap |
| Pointer in lower bits | Pointer alignment needed |
| Works on all platforms | Platform-specific |

---

## Advantages Over Value Struct

Traditional approach:
```c
struct Value {
    ValueType type;  // 4 bytes (padded to 8)
    union {
        double number;
        bool boolean;
        Obj* obj;
    };               // 8 bytes
};                   // Total: 16 bytes
```

NaN Boxing:
```c
typedef uint64_t Value;  // 8 bytes total
```

**50% memory reduction** for value storage!

---

## Object Types

Object pointer encoding supports all heap types:

```c
typedef enum {
    OBJ_STRING,
    OBJ_ARRAY,
    OBJ_MAP,
    OBJ_STRUCT_DEF,
    OBJ_STRUCT_INSTANCE,
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_FUTURE,
    OBJ_ENVIRONMENT
} ObjType;

// Type checking
#define IS_STRING(v)  (IS_OBJ(v) && AS_OBJ(v)->type == OBJ_STRING)
#define IS_ARRAY(v)   (IS_OBJ(v) && AS_OBJ(v)->type == OBJ_ARRAY)
#define IS_MAP(v)     (IS_OBJ(v) && AS_OBJ(v)->type == OBJ_MAP)
```

---

## Common Operations

### Printing Values

```c
void printValue(Value val) {
    if (IS_INT(val)) {
        printf("%d", AS_INT(val));
    } else if (IS_FLOAT(val)) {
        printf("%g", AS_FLOAT(val));
    } else if (IS_BOOL(val)) {
        printf(AS_BOOL(val) ? "true" : "false");
    } else if (IS_NIL(val)) {
        printf("nil");
    } else if (IS_OBJ(val)) {
        printObject(AS_OBJ(val));
    }
}
```

### Arithmetic

```c
Value add(Value a, Value b) {
    if (IS_INT(a) && IS_INT(b)) {
        return INT_VAL(AS_INT(a) + AS_INT(b));
    }
    if (IS_NUMBER(a) && IS_NUMBER(b)) {
        return FLOAT_VAL(AS_FLOAT(a) + AS_FLOAT(b));
    }
    // Handle string concatenation...
}
```

### Equality

```c
bool valuesEqual(Value a, Value b) {
    // Same bits = same value (usually)
    if (a == b) return true;
    
    // Special case: NaN != NaN
    if (IS_FLOAT(a) && IS_FLOAT(b)) {
        return AS_FLOAT(a) == AS_FLOAT(b);
    }
    
    // String comparison
    if (IS_STRING(a) && IS_STRING(b)) {
        // With interning, pointer comparison suffices
        return AS_OBJ(a) == AS_OBJ(b);
    }
    
    return false;
}
```

---

## Performance Impact

| Operation | Tagged Union | NaN Boxing |
|-----------|--------------|------------|
| Type check | Branch + load | Bit mask |
| Stack push | 16 bytes | 8 bytes |
| Cache usage | 2x memory | 1x memory |
| Arithmetic | Unbox → op → box | Minimal overhead |

---

## Next Steps

- [Architecture](architecture.md) - Where values are used
- [Bytecode](bytecode.md) - Value-manipulating opcodes
- [Garbage Collection](garbage-collection.md) - Object pointer handling
