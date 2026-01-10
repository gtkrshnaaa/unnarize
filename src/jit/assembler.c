#include "jit/assembler.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/**
 * x86-64 Assembler Implementation
 * 
 * Manual instruction encoding following Intel/AMD manuals.
 * Supports subset of x86-64 needed for baseline JIT.
 */

#define INITIAL_CODE_SIZE 4096

void initAssembler(Assembler* as, size_t initialCapacity) {
    if (initialCapacity == 0) {
        initialCapacity = INITIAL_CODE_SIZE;
    }
    as->code = (uint8_t*)malloc(initialCapacity);
    as->pos = 0;
    as->capacity = initialCapacity;
}

void freeAssembler(Assembler* as) {
    if (as->code) {
        free(as->code);
        as->code = NULL;
    }
    as->pos = 0;
    as->capacity = 0;
}

void* finalizeAssembler(Assembler* as, size_t* outSize) {
    if (outSize) {
        *outSize = as->pos;
    }
    return as->code;
}

// Ensure buffer has enough space
static void ensureCapacity(Assembler* as, size_t needed) {
    if (as->pos + needed > as->capacity) {
        size_t newCapacity = as->capacity * 2;
        while (newCapacity < as->pos + needed) {
            newCapacity *= 2;
        }
        as->code = (uint8_t*)realloc(as->code, newCapacity);
        as->capacity = newCapacity;
    }
}

void emitByte(Assembler* as, uint8_t byte) {
    ensureCapacity(as, 1);
    as->code[as->pos++] = byte;
}

void emitWord(Assembler* as, uint16_t word) {
    ensureCapacity(as, 2);
    as->code[as->pos++] = word & 0xFF;
    as->code[as->pos++] = (word >> 8) & 0xFF;
}

void emitDword(Assembler* as, uint32_t dword) {
    ensureCapacity(as, 4);
    as->code[as->pos++] = dword & 0xFF;
    as->code[as->pos++] = (dword >> 8) & 0xFF;
    as->code[as->pos++] = (dword >> 16) & 0xFF;
    as->code[as->pos++] = (dword >> 24) & 0xFF;
}

void emitQword(Assembler* as, uint64_t qword) {
    ensureCapacity(as, 8);
    for (int i = 0; i < 8; i++) {
        as->code[as->pos++] = (qword >> (i * 8)) & 0xFF;
    }
}

void emitRex(Assembler* as, bool w, bool r, bool x, bool b) {
    uint8_t rex = 0x40;
    if (w) rex |= 0x08;  // REX.W (64-bit operand)
    if (r) rex |= 0x04;  // REX.R (extension of ModR/M reg field)
    if (x) rex |= 0x02;  // REX.X (extension of SIB index field)
    if (b) rex |= 0x01;  // REX.B (extension of ModR/M r/m field)
    emitByte(as, rex);
}

void emitModRM(Assembler* as, uint8_t mod, uint8_t reg, uint8_t rm) {
    uint8_t byte = ((mod & 0x03) << 6) | ((reg & 0x07) << 3) | (rm & 0x07);
    emitByte(as, byte);
}

// === Stack Operations ===

void emit_push_reg(Assembler* as, X64Register reg) {
    if (reg >= REG_R8) {
        emitRex(as, false, false, false, true);
    }
    emitByte(as, 0x50 + (reg & 0x07));
}

void emit_pop_reg(Assembler* as, X64Register reg) {
    if (reg >= REG_R8) {
        emitRex(as, false, false, false, true);
    }
    emitByte(as, 0x58 + (reg & 0x07));
}

// === Data Movement ===

void emit_mov_reg_reg(Assembler* as, X64Register dst, X64Register src) {
    // MOV dst, src (64-bit)
    emitRex(as, true, src >= REG_R8, false, dst >= REG_R8);
    emitByte(as, 0x89);  // MOV r/m64, r64
    emitModRM(as, 0x03, src & 0x07, dst & 0x07);
}

void emit_mov_reg_imm32(Assembler* as, X64Register reg, int32_t imm) {
    // MOV reg, imm32 (sign-extended to 64-bit)
    emitRex(as, true, false, false, reg >= REG_R8);
    emitByte(as, 0xC7);
    emitModRM(as, 0x03, 0, reg & 0x07);
    emitDword(as, (uint32_t)imm);
}

void emit_mov_reg_imm64(Assembler* as, X64Register reg, int64_t imm) {
    // MOVABS reg, imm64
    emitRex(as, true, false, false, reg >= REG_R8);
    emitByte(as, 0xB8 + (reg & 0x07));
    emitQword(as, (uint64_t)imm);
}

void emit_mov_reg_mem(Assembler* as, X64Register dst, X64Register base, int32_t offset) {
    // MOV dst, [base + offset]
    emitRex(as, true, dst >= REG_R8, false, base >= REG_R8);
    emitByte(as, 0x8B);  // MOV r64, r/m64
    
    if (offset == 0 && (base & 0x07) != REG_RBP) {
        // [base] with no displacement
        emitModRM(as, 0x00, dst & 0x07, base & 0x07);
        if ((base & 0x07) == REG_RSP) {
            emitByte(as, 0x24);  // SIB byte for RSP
        }
    } else if (offset >= -128 && offset <= 127) {
        // [base + disp8]
        emitModRM(as, 0x01, dst & 0x07, base & 0x07);
        if ((base & 0x07) == REG_RSP) {
            emitByte(as, 0x24);  // SIB byte for RSP
        }
        emitByte(as, (uint8_t)offset);
    } else {
        // [base + disp32]
        emitModRM(as, 0x02, dst & 0x07, base & 0x07);
        if ((base & 0x07) == REG_RSP) {
            emitByte(as, 0x24);  // SIB byte for RSP
        }
        emitDword(as, (uint32_t)offset);
    }
}

void emit_mov_mem_reg(Assembler* as, X64Register base, int32_t offset, X64Register src) {
    // MOV [base + offset], src
    emitRex(as, true, src >= REG_R8, false, base >= REG_R8);
    emitByte(as, 0x89);  // MOV r/m64, r64
    
    if (offset == 0 && (base & 0x07) != REG_RBP) {
        emitModRM(as, 0x00, src & 0x07, base & 0x07);
        if ((base & 0x07) == REG_RSP) {
            emitByte(as, 0x24);
        }
    } else if (offset >= -128 && offset <= 127) {
        emitModRM(as, 0x01, src & 0x07, base & 0x07);
        if ((base & 0x07) == REG_RSP) {
            emitByte(as, 0x24);
        }
        emitByte(as, (uint8_t)offset);
    } else {
        emitModRM(as, 0x02, src & 0x07, base & 0x07);
        if ((base & 0x07) == REG_RSP) {
            emitByte(as, 0x24);
        }
        emitDword(as, (uint32_t)offset);
    }
}

// === Arithmetic Operations ===

void emit_add_reg_reg(Assembler* as, X64Register dst, X64Register src) {
    // ADD dst, src
    emitRex(as, true, src >= REG_R8, false, dst >= REG_R8);
    emitByte(as, 0x01);  // ADD r/m64, r64
    emitModRM(as, 0x03, src & 0x07, dst & 0x07);
}

void emit_add_reg_imm(Assembler* as, X64Register reg, int32_t imm) {
    // ADD reg, imm32
    emitRex(as, true, false, false, reg >= REG_R8);
    if (imm >= -128 && imm <= 127) {
        emitByte(as, 0x83);  // ADD r/m64, imm8
        emitModRM(as, 0x03, 0, reg & 0x07);
        emitByte(as, (uint8_t)imm);
    } else {
        emitByte(as, 0x81);  // ADD r/m64, imm32
        emitModRM(as, 0x03, 0, reg & 0x07);
        emitDword(as, (uint32_t)imm);
    }
}

void emit_sub_reg_reg(Assembler* as, X64Register dst, X64Register src) {
    // SUB dst, src
    emitRex(as, true, src >= REG_R8, false, dst >= REG_R8);
    emitByte(as, 0x29);  // SUB r/m64, r64
    emitModRM(as, 0x03, src & 0x07, dst & 0x07);
}

void emit_sub_reg_imm(Assembler* as, X64Register reg, int32_t imm) {
    // SUB reg, imm32
    emitRex(as, true, false, false, reg >= REG_R8);
    if (imm >= -128 && imm <= 127) {
        emitByte(as, 0x83);  // SUB r/m64, imm8
        emitModRM(as, 0x03, 5, reg & 0x07);
        emitByte(as, (uint8_t)imm);
    } else {
        emitByte(as, 0x81);  // SUB r/m64, imm32
        emitModRM(as, 0x03, 5, reg & 0x07);
        emitDword(as, (uint32_t)imm);
    }
}

void emit_imul_reg_reg(Assembler* as, X64Register dst, X64Register src) {
    // IMUL dst, src
    emitRex(as, true, dst >= REG_R8, false, src >= REG_R8);
    emitByte(as, 0x0F);
    emitByte(as, 0xAF);  // IMUL r64, r/m64
    emitModRM(as, 0x03, dst & 0x07, src & 0x07);
}

void emit_idiv_reg(Assembler* as, X64Register divisor) {
    // IDIV divisor (RAX:RDX / divisor -> RAX=quotient, RDX=remainder)
    emitRex(as, true, false, false, divisor >= REG_R8);
    emitByte(as, 0xF7);
    emitModRM(as, 0x03, 7, divisor & 0x07);
}

void emit_neg_reg(Assembler* as, X64Register reg) {
    // NEG reg
    emitRex(as, true, false, false, reg >= REG_R8);
    emitByte(as, 0xF7);
    emitModRM(as, 0x03, 3, reg & 0x07);
}

// === Comparison ===

void emit_cmp_reg_reg(Assembler* as, X64Register r1, X64Register r2) {
    // CMP r1, r2
    emitRex(as, true, r2 >= REG_R8, false, r1 >= REG_R8);
    emitByte(as, 0x39);  // CMP r/m64, r64
    emitModRM(as, 0x03, r2 & 0x07, r1 & 0x07);
}

void emit_cmp_reg_imm(Assembler* as, X64Register reg, int32_t imm) {
    // CMP reg, imm32
    emitRex(as, true, false, false, reg >= REG_R8);
    if (imm >= -128 && imm <= 127) {
        emitByte(as, 0x83);  // CMP r/m64, imm8
        emitModRM(as, 0x03, 7, reg & 0x07);
        emitByte(as, (uint8_t)imm);
    } else {
        emitByte(as, 0x81);  // CMP r/m64, imm32
        emitModRM(as, 0x03, 7, reg & 0x07);
        emitDword(as, (uint32_t)imm);
    }
}

void emit_test_reg_reg(Assembler* as, X64Register r1, X64Register r2) {
    // TEST r1, r2
    emitRex(as, true, r2 >= REG_R8, false, r1 >= REG_R8);
    emitByte(as, 0x85);  // TEST r/m64, r64
    emitModRM(as, 0x03, r2 & 0x07, r1 & 0x07);
}

// === Control Flow ===

void emit_jmp(Assembler* as, int32_t offset) {
    // JMP rel32
    emitByte(as, 0xE9);
    emitDword(as, (uint32_t)offset);
}

void emit_je(Assembler* as, int32_t offset) {
    // JE rel32 (jump if equal/zero)
    emitByte(as, 0x0F);
    emitByte(as, 0x84);
    emitDword(as, (uint32_t)offset);
}

void emit_jne(Assembler* as, int32_t offset) {
    // JNE rel32 (jump if not equal/not zero)
    emitByte(as, 0x0F);
    emitByte(as, 0x85);
    emitDword(as, (uint32_t)offset);
}

void emit_jl(Assembler* as, int32_t offset) {
    // JL rel32 (jump if less)
    emitByte(as, 0x0F);
    emitByte(as, 0x8C);
    emitDword(as, (uint32_t)offset);
}

void emit_jle(Assembler* as, int32_t offset) {
    // JLE rel32 (jump if less or equal)
    emitByte(as, 0x0F);
    emitByte(as, 0x8E);
    emitDword(as, (uint32_t)offset);
}

void emit_jg(Assembler* as, int32_t offset) {
    // JG rel32 (jump if greater)
    emitByte(as, 0x0F);
    emitByte(as, 0x8F);
    emitDword(as, (uint32_t)offset);
}

void emit_jge(Assembler* as, int32_t offset) {
    // JGE rel32 (jump if greater or equal)
    emitByte(as, 0x0F);
    emitByte(as, 0x8D);
    emitDword(as, (uint32_t)offset);
}

void emit_call_abs(Assembler* as, void* target) {
    // CALL absolute address (using MOV + CALL reg)
    // MOV RAX, target
    emit_mov_reg_imm64(as, REG_RAX, (int64_t)target);
    // CALL RAX
    emit_call_reg(as, REG_RAX);
}

void emit_call_reg(Assembler* as, X64Register reg) {
    // CALL reg
    if (reg >= REG_R8) {
        emitRex(as, false, false, false, true);
    }
    emitByte(as, 0xFF);
    emitModRM(as, 0x03, 2, reg & 0x07);
}

void emit_ret(Assembler* as) {
    // RET
    emitByte(as, 0xC3);
}

// === Special ===

void emit_nop(Assembler* as) {
    // NOP
    emitByte(as, 0x90);
}

void emit_int3(Assembler* as) {
    // INT3 (breakpoint)
    emitByte(as, 0xCC);
}

// === Jump patching ===

size_t getCurrentPosition(Assembler* as) {
    return as->pos;
}

void patchJumpOffset(Assembler* as, size_t jumpPos, size_t targetPos) {
    // Calculate relative offset (from end of jump instruction)
    int32_t offset = (int32_t)(targetPos - (jumpPos + 4));
    
    // Patch the 4-byte offset at jumpPos
    as->code[jumpPos + 0] = offset & 0xFF;
    as->code[jumpPos + 1] = (offset >> 8) & 0xFF;
    as->code[jumpPos + 2] = (offset >> 16) & 0xFF;
    as->code[jumpPos + 3] = (offset >> 24) & 0xFF;
}
