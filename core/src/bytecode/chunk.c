#include "bytecode/chunk.h"
#include <stdlib.h>
#include <stdio.h>

/**
 * Register-based Bytecode Chunk Management
 * Uses 32-bit instruction words
 */

#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

void initChunk(BytecodeChunk* chunk) {
    chunk->code = NULL;
    chunk->codeSize = 0;
    chunk->codeCapacity = 0;

    chunk->constants = NULL;
    chunk->constantCount = 0;
    chunk->constantCapacity = 0;

    chunk->lineNumbers = NULL;
    chunk->lineCapacity = 0;

    chunk->maxRegs = 0;
}

void freeChunk(BytecodeChunk* chunk) {
    if (chunk->code) free(chunk->code);
    if (chunk->constants) free(chunk->constants);
    if (chunk->lineNumbers) free(chunk->lineNumbers);
    initChunk(chunk);
}

void writeChunk(BytecodeChunk* chunk, uint32_t instruction, int line) {
    if (chunk->codeCapacity < chunk->codeSize + 1) {
        int oldCapacity = chunk->codeCapacity;
        chunk->codeCapacity = GROW_CAPACITY(oldCapacity);
        chunk->code = realloc(chunk->code, chunk->codeCapacity * sizeof(uint32_t));
    }

    chunk->code[chunk->codeSize] = instruction;

    if (chunk->lineCapacity < chunk->codeSize + 1) {
        int oldCapacity = chunk->lineCapacity;
        chunk->lineCapacity = GROW_CAPACITY(oldCapacity);
        chunk->lineNumbers = realloc(chunk->lineNumbers, chunk->lineCapacity * sizeof(int));
    }
    chunk->lineNumbers[chunk->codeSize] = line;

    chunk->codeSize++;
}

int addConstant(BytecodeChunk* chunk, Value value) {
    if (chunk->constantCapacity < chunk->constantCount + 1) {
        int oldCapacity = chunk->constantCapacity;
        chunk->constantCapacity = GROW_CAPACITY(oldCapacity);
        chunk->constants = realloc(chunk->constants, chunk->constantCapacity * sizeof(Value));
    }
    chunk->constants[chunk->constantCount] = value;
    return chunk->constantCount++;
}

void patchJump(BytecodeChunk* chunk, int instrIndex) {
    // Patch the jump offset in instruction at instrIndex
    // Jump distance = current position - instrIndex - 1
    int jump = chunk->codeSize - instrIndex - 1;
    uint32_t inst = chunk->code[instrIndex];
    uint8_t op = DECODE_OP(inst);

    if (op == OP_JMP || op == OP_LOOP) {
        // sBx24 format: 24-bit signed offset
        chunk->code[instrIndex] = ENCODE_sBx(op, jump);
    } else {
        // AsBx format: keep A, patch sBx (16-bit)
        uint8_t a = DECODE_A(inst);
        chunk->code[instrIndex] = ENCODE_AsBx(op, a, jump);
    }
}

// Disassembler

void disassembleChunk(BytecodeChunk* chunk, const char* name) {
    printf("== %s (regs: %d) ==\n", name, chunk->maxRegs);
    for (int offset = 0; offset < chunk->codeSize; offset++) {
        disassembleInstruction(chunk, offset);
    }
}

int disassembleInstruction(BytecodeChunk* chunk, int offset) {
    printf("%04d ", offset);

    if (offset > 0 && chunk->lineNumbers[offset] == chunk->lineNumbers[offset - 1]) {
        printf("   | ");
    } else {
        printf("%4d ", chunk->lineNumbers[offset]);
    }

    uint32_t inst = chunk->code[offset];
    uint8_t op = DECODE_OP(inst);
    const OpcodeInfo* info = getOpcodeInfo(op);

    if (!info || !info->name) {
        printf("Unknown opcode %d\n", op);
        return offset + 1;
    }

    uint8_t a = DECODE_A(inst);
    uint8_t b = DECODE_B(inst);
    uint8_t c = DECODE_C(inst);
    uint16_t bx = DECODE_Bx(inst);

    switch (info->format) {
        case 0: // ABC
            printf("%-16s R%-3d R%-3d R%-3d\n", info->name, a, b, c);
            break;
        case 1: // ABx
            printf("%-16s R%-3d %5d", info->name, a, bx);
            // Show constant value if it's a LOADK or global access
            if ((op == OP_LOADK || op == OP_GETGLOBAL || op == OP_SETGLOBAL ||
                 op == OP_DEFGLOBAL) && bx < (uint16_t)chunk->constantCount) {
                printf("  ; K(");
                printValue(chunk->constants[bx]);
                printf(")");
            }
            printf("\n");
            break;
        case 2: // AsBx
            printf("%-16s R%-3d -> %d\n", info->name, a, offset + 1 + DECODE_sBx(inst));
            break;
        case 3: // sBx24
            printf("%-16s -> %d\n", info->name, offset + 1 + DECODE_sBx24(inst));
            break;
        case 4: // A-only
            printf("%-16s R%-3d\n", info->name, a);
            break;
        default:
            printf("%-16s ???\n", info->name);
            break;
    }

    return offset + 1;
}
