#ifndef JIT_ASSEMBLER_H
#define JIT_ASSEMBLER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * x86-64 Machine Code Assembler
 * 
 * Manual instruction encoding for JIT compilation.
 * Zero external dependencies - pure C implementation.
 * 
 * Supports System V AMD64 ABI calling convention.
 */

// x86-64 Register encoding
typedef enum {
    REG_RAX = 0,
    REG_RCX = 1,
    REG_RDX = 2,
    REG_RBX = 3,
    REG_RSP = 4,
    REG_RBP = 5,
    REG_RSI = 6,
    REG_RDI = 7,
    REG_R8  = 8,
    REG_R9  = 9,
    REG_R10 = 10,
    REG_R11 = 11,
    REG_R12 = 12,
    REG_R13 = 13,
    REG_R14 = 14,
    REG_R15 = 15
} X64Register;

// Assembler state
typedef struct {
    uint8_t* code;      // Code buffer
    size_t pos;         // Current position
    size_t capacity;    // Buffer capacity
} Assembler;

// Assembler lifecycle
void initAssembler(Assembler* as, size_t initialCapacity);
void freeAssembler(Assembler* as);
void* finalizeAssembler(Assembler* as, size_t* outSize);

// Low-level emission
void emitByte(Assembler* as, uint8_t byte);
void emitWord(Assembler* as, uint16_t word);
void emitDword(Assembler* as, uint32_t dword);
void emitQword(Assembler* as, uint64_t qword);

// REX prefix (for 64-bit operands and extended registers)
void emitRex(Assembler* as, bool w, bool r, bool x, bool b);

// ModR/M byte encoding
void emitModRM(Assembler* as, uint8_t mod, uint8_t reg, uint8_t rm);

// === Stack Operations ===
void emit_push_reg(Assembler* as, X64Register reg);
void emit_pop_reg(Assembler* as, X64Register reg);

// === Data Movement ===
void emit_mov_reg_reg(Assembler* as, X64Register dst, X64Register src);
void emit_mov_reg_imm32(Assembler* as, X64Register reg, int32_t imm);
void emit_mov_reg_imm64(Assembler* as, X64Register reg, int64_t imm);
void emit_mov_reg_mem(Assembler* as, X64Register dst, X64Register base, int32_t offset);
void emit_mov_mem_reg(Assembler* as, X64Register base, int32_t offset, X64Register src);

// === Arithmetic Operations ===
void emit_add_reg_reg(Assembler* as, X64Register dst, X64Register src);
void emit_add_reg_imm(Assembler* as, X64Register reg, int32_t imm);
void emit_sub_reg_reg(Assembler* as, X64Register dst, X64Register src);
void emit_sub_reg_imm(Assembler* as, X64Register reg, int32_t imm);
void emit_imul_reg_reg(Assembler* as, X64Register dst, X64Register src);
void emit_idiv_reg(Assembler* as, X64Register divisor);
void emit_neg_reg(Assembler* as, X64Register reg);

// === Logical Operations ===
void emit_and_reg_reg(Assembler* as, X64Register dst, X64Register src);
void emit_or_reg_reg(Assembler* as, X64Register dst, X64Register src);
void emit_xor_reg_reg(Assembler* as, X64Register dst, X64Register src);
void emit_xor_reg_imm(Assembler* as, X64Register reg, int32_t imm);

// === Comparison ===
void emit_cmp_reg_reg(Assembler* as, X64Register r1, X64Register r2);
void emit_cmp_reg_imm(Assembler* as, X64Register reg, int32_t imm);
void emit_test_reg_reg(Assembler* as, X64Register r1, X64Register r2);

// === Control Flow ===
void emit_jmp(Assembler* as, int32_t offset);
void emit_je(Assembler* as, int32_t offset);
void emit_jne(Assembler* as, int32_t offset);
void emit_jl(Assembler* as, int32_t offset);
void emit_jle(Assembler* as, int32_t offset);
void emit_jg(Assembler* as, int32_t offset);
void emit_jge(Assembler* as, int32_t offset);
void emit_call_abs(Assembler* as, void* target);
void emit_call_reg(Assembler* as, X64Register reg);
void emit_ret(Assembler* as);

// === Special ===
void emit_nop(Assembler* as);
void emit_int3(Assembler* as);  // Breakpoint for debugging

// === Jump patching ===
size_t getCurrentPosition(Assembler* as);
void patchJumpOffset(Assembler* as, size_t jumpPos, size_t targetPos);

#endif // JIT_ASSEMBLER_H
