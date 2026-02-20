#ifndef BYTECODE_OPCODES_H
#define BYTECODE_OPCODES_H

#include <stdbool.h>
#include <stdint.h>

/**
 * Unnarize Register-Based Bytecode Instruction Set
 *
 * 32-bit fixed-width instructions.
 *
 * Encoding formats:
 *   ABC:  [opcode:8][A:8][B:8][C:8]   — 3 register operands
 *   ABx:  [opcode:8][A:8][Bx:16]      — register + 16-bit unsigned index
 *   AsBx: [opcode:8][A:8][sBx:16]     — register + 16-bit signed offset
 *   sBx:  [opcode:8][sBx:24]          — 24-bit signed offset (jumps)
 */

typedef enum {
    // === Data Movement ===
    OP_MOVE,            // ABC:  R(A) = R(B)
    OP_LOADK,           // ABx:  R(A) = K(Bx)
    OP_LOADI,           // ABx:  R(A) = (int16_t)Bx  (signed immediate)
    OP_LOADNIL,         // A:    R(A) = nil
    OP_LOADTRUE,        // A:    R(A) = true
    OP_LOADFALSE,       // A:    R(A) = false

    // === Global Variables ===
    OP_GETGLOBAL,       // ABx:  R(A) = globals[K(Bx)]
    OP_SETGLOBAL,       // ABx:  globals[K(Bx)] = R(A)
    OP_DEFGLOBAL,       // ABx:  define globals[K(Bx)] = R(A)

    // === Arithmetic ===
    OP_ADD,             // ABC:  R(A) = R(B) + R(C)
    OP_SUB,             // ABC:  R(A) = R(B) - R(C)
    OP_MUL,             // ABC:  R(A) = R(B) * R(C)
    OP_DIV,             // ABC:  R(A) = R(B) / R(C)
    OP_MOD,             // ABC:  R(A) = R(B) % R(C)
    OP_NEG,             // ABC:  R(A) = -R(B)

    // === Comparisons ===
    OP_LT,              // ABC:  R(A) = R(B) < R(C)
    OP_LE,              // ABC:  R(A) = R(B) <= R(C)
    OP_GT,              // ABC:  R(A) = R(B) > R(C)
    OP_GE,              // ABC:  R(A) = R(B) >= R(C)
    OP_EQ,              // ABC:  R(A) = R(B) == R(C)
    OP_NE,              // ABC:  R(A) = R(B) != R(C)

    // === Logical ===
    OP_NOT,             // ABC:  R(A) = !R(B)

    // === Control Flow ===
    OP_JMP,             // sBx:  pc += sBx  (24-bit signed)
    OP_JMPF,            // AsBx: if !R(A): pc += sBx
    OP_JMPT,            // AsBx: if  R(A): pc += sBx
    OP_LOOP,            // sBx:  pc -= sBx  (backward jump, 24-bit)

    // === Function Calls ===
    OP_CALL,            // ABC:  call R(A) with B args at R(A+1..A+B), C result regs
    OP_RETURN,          // A:    return R(A)
    OP_RETURNNIL,       // -:    return nil

    // === Object / Property Access ===
    OP_GETPROP,         // ABC:  R(A) = R(B).K(C)   (property name from constant pool)
    OP_SETPROP,         // ABC:  R(A).K(B) = R(C)

    // === Index Access ===
    OP_GETIDX,          // ABC:  R(A) = R(B)[R(C)]
    OP_SETIDX,          // ABC:  R(A)[R(B)] = R(C)

    // === Object Creation ===
    OP_NEWARRAY,        // ABx:  R(A) = new array with Bx initial elements from R(A+1..A+Bx)
    OP_NEWMAP,          // A:    R(A) = {}
    OP_NEWSTRUCT,       // ABC:  R(A) = new instance of StructDef R(B) with C field values

    // === Struct Definition ===
    OP_STRUCTDEF,       // ABx:  define struct K(Bx) with A field names from K pool

    // === Array Builtins ===
    OP_PUSH,            // ABC:  push(R(A), R(B))
    OP_POP,             // ABC:  R(A) = pop(R(B))
    OP_LEN,             // ABC:  R(A) = len(R(B))

    // === String Concat (polymorphic add handles this, but explicit for clarity) ===

    // === Import ===
    OP_IMPORT,          // ABx:  R(A) = import(K(Bx))

    // === Async ===
    OP_ASYNC,           // ABC:  R(A) = async call R(B) with C args
    OP_AWAIT,           // ABC:  R(A) = await R(B)

    // === Special ===
    OP_PRINT,           // A:    print R(A)
    OP_HALT,            // -:    stop execution
    OP_NOP,             // -:    no operation

    // === Foreach ===
    OP_FOREACH_PREP,    // ABC:  prepare foreach: R(A)=iterator, R(B)=collection
    OP_FOREACH_NEXT,    // AsBx: R(A) = next element, jump sBx if exhausted

    // === Concatenation (fast path) ===
    OP_CONCAT,          // ABC:  R(A) = R(B) .. R(C)  (string concat)

    OPCODE_COUNT
} OpCode;

// ===== 32-bit Instruction Encoding Helpers =====

// Build instructions
#define ENCODE_ABC(op, a, b, c)   ((uint32_t)((op) << 24 | ((a) & 0xFF) << 16 | ((b) & 0xFF) << 8 | ((c) & 0xFF)))
#define ENCODE_ABx(op, a, bx)     ((uint32_t)((op) << 24 | ((a) & 0xFF) << 16 | ((bx) & 0xFFFF)))
#define ENCODE_AsBx(op, a, sbx)   ENCODE_ABx(op, a, (uint16_t)((sbx) + 0x7FFF))
#define ENCODE_sBx(op, sbx)       ((uint32_t)((op) << 24 | ((uint32_t)((sbx) + 0x7FFFFF) & 0xFFFFFF)))
#define ENCODE_A(op, a)           ((uint32_t)((op) << 24 | ((a) & 0xFF) << 16))

// Decode instructions
#define DECODE_OP(inst)   (((inst) >> 24) & 0xFF)
#define DECODE_A(inst)    (((inst) >> 16) & 0xFF)
#define DECODE_B(inst)    (((inst) >>  8) & 0xFF)
#define DECODE_C(inst)    (((inst)      ) & 0xFF)
#define DECODE_Bx(inst)   (((inst)      ) & 0xFFFF)
#define DECODE_sBx(inst)  ((int)(DECODE_Bx(inst)) - 0x7FFF)
#define DECODE_sBx24(inst) ((int)((inst) & 0xFFFFFF) - 0x7FFFFF)

// Opcode metadata
typedef struct {
    const char* name;
    int format;         // 0=ABC, 1=ABx, 2=AsBx, 3=sBx, 4=A-only
    bool hasSideEffect;
} OpcodeInfo;

const OpcodeInfo* getOpcodeInfo(OpCode op);

#endif // BYTECODE_OPCODES_H
