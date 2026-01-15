# Bytecode Reference

> Complete instruction set for the Unnarize VM.

---

## Overview

Unnarize uses a stack-based bytecode VM with ~100 opcodes. Instructions are designed for:

- **Fast Dispatch** - Computed goto with GCC/Clang
- **Type Specialization** - Separate opcodes for int/float
- **Minimal Overhead** - Compact encoding

---

## Opcode Categories

| Category | Count | Description |
|----------|-------|-------------|
| Stack Operations | 8 | Push, pop, dup |
| Local Variables | 6 | Load/store locals |
| Global Variables | 3 | Load/store/define globals |
| Arithmetic (Int) | 6 | Type-specialized int ops |
| Arithmetic (Float) | 5 | Type-specialized float ops |
| Arithmetic (Generic) | 6 | Polymorphic with type checks |
| Comparison (Int) | 6 | Type-specialized comparisons |
| Comparison (Float) | 6 | Type-specialized comparisons |
| Comparison (Generic) | 6 | Polymorphic comparisons |
| Logical | 3 | AND, OR, NOT |
| Control Flow | 5 | Jump, loop |
| Function Calls | 6 | Call, return |
| Object Access | 4 | Property, index |
| Object Creation | 3 | Array, map, struct |
| Array Operations | 4 | Push, pop, length |
| Async | 2 | Async call, await |
| Special | 4 | Print, halt, nop |

---

## Stack Operations

| Opcode | Operands | Stack Effect | Description |
|--------|----------|--------------|-------------|
| `OP_IMPORT` | 1 (const index) | → | Import module |
| `OP_LOAD_IMM` | 4 (int32) | → value | Push 32-bit immediate |
| `OP_LOAD_CONST` | 1 (index) | → value | Push from constant pool |
| `OP_LOAD_NIL` | 0 | → nil | Push nil |
| `OP_LOAD_TRUE` | 0 | → true | Push true |
| `OP_LOAD_FALSE` | 0 | → false | Push false |
| `OP_POP` | 0 | value → | Discard top |
| `OP_DUP` | 0 | a → a a | Duplicate top |

---

## Local Variables

| Opcode | Operands | Stack Effect | Description |
|--------|----------|--------------|-------------|
| `OP_LOAD_LOCAL` | 1 (slot) | → value | Load local by slot |
| `OP_STORE_LOCAL` | 1 (slot) | value → | Store to local |
| `OP_LOAD_LOCAL_0` | 0 | → value | Fast path: slot 0 |
| `OP_LOAD_LOCAL_1` | 0 | → value | Fast path: slot 1 |
| `OP_LOAD_LOCAL_2` | 0 | → value | Fast path: slot 2 |
| `OP_INC_LOCAL` | 1 (slot) | → | Increment local by 1 |
| `OP_DEC_LOCAL` | 1 (slot) | → | Decrement local by 1 |

### Fast Path Example

```
// Code: var x = 0; x = x + 1;
OP_LOAD_IMM 0      // Push 0
OP_STORE_LOCAL 0   // Store to slot 0

// Optimized to:
OP_INC_LOCAL 0     // Increment slot 0 directly
```

---

## Global Variables

| Opcode | Operands | Stack Effect | Description |
|--------|----------|--------------|-------------|
| `OP_LOAD_GLOBAL` | 1 (name index) | → value | Load global by name |
| `OP_STORE_GLOBAL` | 1 (name index) | value → | Store to global |
| `OP_DEFINE_GLOBAL` | 1 (name index) | value → | Define new global |

---

## Specialized Arithmetic (Integer)

No type checking - compiler guarantees int operands.

| Opcode | Stack Effect | Operation |
|--------|--------------|-----------|
| `OP_ADD_II` | a b → (a+b) | int + int |
| `OP_SUB_II` | a b → (a-b) | int - int |
| `OP_MUL_II` | a b → (a*b) | int * int |
| `OP_DIV_II` | a b → (a/b) | int / int |
| `OP_MOD_II` | a b → (a%b) | int % int |
| `OP_NEG_I` | a → (-a) | -int |

---

## Specialized Arithmetic (Float)

| Opcode | Stack Effect | Operation |
|--------|--------------|-----------|
| `OP_ADD_FF` | a b → (a+b) | float + float |
| `OP_SUB_FF` | a b → (a-b) | float - float |
| `OP_MUL_FF` | a b → (a*b) | float * float |
| `OP_DIV_FF` | a b → (a/b) | float / float |
| `OP_NEG_F` | a → (-a) | -float |

---

## Generic Arithmetic

Type checks at runtime, handles string concatenation.

| Opcode | Stack Effect | Operation |
|--------|--------------|-----------|
| `OP_ADD` | a b → (a+b) | Generic add/concat |
| `OP_SUB` | a b → (a-b) | Generic subtract |
| `OP_MUL` | a b → (a*b) | Generic multiply |
| `OP_DIV` | a b → (a/b) | Generic divide |
| `OP_MOD` | a b → (a%b) | Generic modulo |
| `OP_NEG` | a → (-a) | Generic negate |

---

## Specialized Comparisons (Integer)

| Opcode | Stack Effect | Operation |
|--------|--------------|-----------|
| `OP_LT_II` | a b → bool | int < int |
| `OP_LE_II` | a b → bool | int <= int |
| `OP_GT_II` | a b → bool | int > int |
| `OP_GE_II` | a b → bool | int >= int |
| `OP_EQ_II` | a b → bool | int == int |
| `OP_NE_II` | a b → bool | int != int |

---

## Specialized Comparisons (Float)

| Opcode | Stack Effect | Operation |
|--------|--------------|-----------|
| `OP_LT_FF` | a b → bool | float < float |
| `OP_LE_FF` | a b → bool | float <= float |
| `OP_GT_FF` | a b → bool | float > float |
| `OP_GE_FF` | a b → bool | float >= float |
| `OP_EQ_FF` | a b → bool | float == float |
| `OP_NE_FF` | a b → bool | float != float |

---

## Generic Comparisons

| Opcode | Stack Effect | Operation |
|--------|--------------|-----------|
| `OP_LT` | a b → bool | Generic < |
| `OP_LE` | a b → bool | Generic <= |
| `OP_GT` | a b → bool | Generic > |
| `OP_GE` | a b → bool | Generic >= |
| `OP_EQ` | a b → bool | Generic == |
| `OP_NE` | a b → bool | Generic != |

---

## Logical Operations

| Opcode | Stack Effect | Description |
|--------|--------------|-------------|
| `OP_NOT` | a → !a | Logical NOT |
| `OP_AND` | a b → (a&&b) | Logical AND |
| `OP_OR` | a b → (a\|\|b) | Logical OR |

---

## Control Flow

| Opcode | Operands | Stack Effect | Description |
|--------|----------|--------------|-------------|
| `OP_JUMP` | 2 (offset) | → | Unconditional jump |
| `OP_JUMP_IF_FALSE` | 2 (offset) | cond → | Jump if false |
| `OP_JUMP_IF_TRUE` | 2 (offset) | cond → | Jump if true |
| `OP_LOOP` | 2 (offset) | → | Backward jump |
| `OP_LOOP_HEADER` | 0 | → | Loop marker (for OSR) |

### Jump Encoding

```
Offset = (byte1 << 8) | byte2  // 16-bit signed
```

---

## Function Calls

| Opcode | Operands | Stack Effect | Description |
|--------|----------|--------------|-------------|
| `OP_CALL` | 1 (argc) | fn args... → result | Call function |
| `OP_CALL_0` | 0 | fn → result | Optimized: 0 args |
| `OP_CALL_1` | 0 | fn arg → result | Optimized: 1 arg |
| `OP_CALL_2` | 0 | fn arg1 arg2 → result | Optimized: 2 args |
| `OP_RETURN` | 0 | result → | Return value |
| `OP_RETURN_NIL` | 0 | → | Return nil |

---

## Object/Property Access

| Opcode | Operands | Stack Effect | Description |
|--------|----------|--------------|-------------|
| `OP_LOAD_PROPERTY` | 1 (name) | obj → value | Get property |
| `OP_STORE_PROPERTY` | 1 (name) | obj value → | Set property |
| `OP_LOAD_INDEX` | 0 | obj idx → value | Array/map index |
| `OP_STORE_INDEX` | 0 | obj idx value → | Array/map store |

---

## Object Creation

| Opcode | Operands | Stack Effect | Description |
|--------|----------|--------------|-------------|
| `OP_NEW_ARRAY` | 1 (count) | items... → array | Create array |
| `OP_NEW_MAP` | 0 | → map | Create empty map |
| `OP_NEW_OBJECT` | 1 (fields) | values... → obj | Create struct |
| `OP_STRUCT_DEF` | 1 (count) | → | Define struct type |

---

## Array Operations

| Opcode | Stack Effect | Description |
|--------|--------------|-------------|
| `OP_ARRAY_PUSH` | arr val → arr | Push to array |
| `OP_ARRAY_PUSH_CLEAN` | arr val → | Push and clean |
| `OP_ARRAY_POP` | arr → val | Pop from array |
| `OP_ARRAY_LEN` | arr → len | Get length |

---

## Async Operations

| Opcode | Operands | Stack Effect | Description |
|--------|----------|--------------|-------------|
| `OP_ASYNC_CALL` | 1 (argc) | fn args... → future | Start async |
| `OP_AWAIT` | 0 | future → result | Wait for result |

---

## Special

| Opcode | Description |
|--------|-------------|
| `OP_PRINT` | Debug print top of stack |
| `OP_HALT` | Stop execution |
| `OP_NOP` | No operation |

---

## Bytecode Chunk Structure

```c
typedef struct BytecodeChunk {
    uint8_t* code;       // Instruction bytes
    int codeCount;
    int codeCapacity;
    
    Value* constants;    // Constant pool
    int constCount;
    int constCapacity;
    
    int* lines;          // Line number info
} BytecodeChunk;
```

---

## Example Bytecode

Source:
```javascript
var x = 10;
var y = 20;
print(x + y);
```

Bytecode:
```
OP_LOAD_IMM 10        // Push 10
OP_DEFINE_GLOBAL "x"  // Define x = 10
OP_LOAD_IMM 20        // Push 20
OP_DEFINE_GLOBAL "y"  // Define y = 20
OP_LOAD_GLOBAL "x"    // Push x
OP_LOAD_GLOBAL "y"    // Push y
OP_ADD                // Add
OP_PRINT              // Print result
```

---

## Next Steps

- [Architecture](architecture.md) - Overall VM structure
- [Garbage Collection](garbage-collection.md) - Memory management
- [NaN Boxing](nan-boxing.md) - Value encoding
