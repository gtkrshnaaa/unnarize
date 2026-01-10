#include "bytecode/chunk.h"
#include <stdlib.h>
#include <stdio.h>

/**
 * Bytecode Chunk Management
 * Zero external dependencies - pure C stdlib
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
    
    chunk->hotspots = NULL;
    chunk->hotspotCapacity = 0;
    
    chunk->icSlots = NULL;
    chunk->icSlotCount = 0;
    chunk->icSlotCapacity = 0;
    
    chunk->jitCode = NULL;
    chunk->jitCodeCount = 0;
    chunk->jitCodeCapacity = 0;
    
    chunk->isCompiled = false;
    chunk->tierLevel = 0;
}

void freeChunk(BytecodeChunk* chunk) {
    if (chunk->code) free(chunk->code);
    if (chunk->constants) free(chunk->constants);
    if (chunk->lineNumbers) free(chunk->lineNumbers);
    if (chunk->hotspots) free(chunk->hotspots);
    if (chunk->icSlots) free(chunk->icSlots);
    if (chunk->jitCode) free(chunk->jitCode);
    
    initChunk(chunk);  // Reset to initial state
}

void writeChunk(BytecodeChunk* chunk, uint8_t byte, int line) {
    // Grow code array if needed
    if (chunk->codeCapacity < chunk->codeSize + 1) {
        int oldCapacity = chunk->codeCapacity;
        chunk->codeCapacity = GROW_CAPACITY(oldCapacity);
        chunk->code = realloc(chunk->code, chunk->codeCapacity);
    }
    
    // Write bytecode
    chunk->code[chunk->codeSize] = byte;
    
    // Grow line number array
    if (chunk->lineCapacity < chunk->codeSize + 1) {
        int oldCapacity = chunk->lineCapacity;
        chunk->lineCapacity = GROW_CAPACITY(oldCapacity);
        chunk->lineNumbers = realloc(chunk->lineNumbers, 
            chunk->lineCapacity * sizeof(int));
    }
    chunk->lineNumbers[chunk->codeSize] = line;
    
    // Grow hotspot array
    if (chunk->hotspotCapacity < chunk->codeSize + 1) {
        int oldCapacity = chunk->hotspotCapacity;
        chunk->hotspotCapacity = GROW_CAPACITY(oldCapacity);
        chunk->hotspots = realloc(chunk->hotspots,
            chunk->hotspotCapacity * sizeof(uint32_t));
        // Zero-initialize new hotspots
        for (int i = oldCapacity; i < chunk->hotspotCapacity; i++) {
            chunk->hotspots[i] = 0;
        }
    }
    
    chunk->codeSize++;
}

int addConstant(BytecodeChunk* chunk, Value value) {
    // Grow constant pool if needed
    if (chunk->constantCapacity < chunk->constantCount + 1) {
        int oldCapacity = chunk->constantCapacity;
        chunk->constantCapacity = GROW_CAPACITY(oldCapacity);
        chunk->constants = realloc(chunk->constants,
            chunk->constantCapacity * sizeof(Value));
    }
    
    chunk->constants[chunk->constantCount] = value;
    return chunk->constantCount++;
}

void patchJump(BytecodeChunk* chunk, int offset) {
    // Calculate jump distance
    // -2 to account for the jump instruction operand itself
    int jump = chunk->codeSize - offset - 2;
    
    if (jump > UINT16_MAX) {
        fprintf(stderr, "Too much code to jump over.\n");
        return;
    }
    
    // Patch the 2-byte jump offset
    chunk->code[offset] = (jump >> 8) & 0xff;
    chunk->code[offset + 1] = jump & 0xff;
}

// Disassembler implementation
void disassembleChunk(BytecodeChunk* chunk, const char* name) {
    printf("== %s ==\n", name);
    
    for (int offset = 0; offset < chunk->codeSize;) {
        offset = disassembleInstruction(chunk, offset);
    }
}

static int simpleInstruction(const char* name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

static int byteInstruction(const char* name, BytecodeChunk* chunk, int offset) {
    uint8_t slot = chunk->code[offset + 1];
    printf("%-16s %4d\n", name, slot);
    return offset + 2;
}

static int constantInstruction(const char* name, BytecodeChunk* chunk, int offset) {
    uint8_t constantIndex = chunk->code[offset + 1];
    printf("%-16s %4d '", name, constantIndex);
    printValue(chunk->constants[constantIndex]);
    printf("'\n");
    return offset + 2;
}

static int jumpInstruction(const char* name, int sign, BytecodeChunk* chunk, int offset) {
    uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
    jump |= chunk->code[offset + 2];
    printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
    return offset + 3;
}

int disassembleInstruction(BytecodeChunk* chunk, int offset) {
    printf("%04d ", offset);
    
    // Show line number
    if (offset > 0 && chunk->lineNumbers[offset] == chunk->lineNumbers[offset - 1]) {
        printf("   | ");
    } else {
        printf("%4d ", chunk->lineNumbers[offset]);
    }
    
    // Show hotspot counter
    if (chunk->hotspots && offset < chunk->hotspotCapacity) {
        printf("[%6u] ", chunk->hotspots[offset]);
    } else {
        printf("[     -] ");
    }
    
    uint8_t instruction = chunk->code[offset];
    const OpcodeInfo* info = getOpcodeInfo(instruction);
    
    if (!info) {
        printf("Unknown opcode %d\n", instruction);
        return offset + 1;
    }
    
    switch (info->operandBytes) {
        case 0:
            return simpleInstruction(info->name, offset);
        case 1:
            if (instruction == OP_LOAD_CONST) {
                return constantInstruction(info->name, chunk, offset);
            }
            return byteInstruction(info->name, chunk, offset);
        case 2:
            if (instruction == OP_JUMP || instruction == OP_JUMP_IF_FALSE ||
                instruction == OP_JUMP_IF_TRUE) {
                return jumpInstruction(info->name, 1, chunk, offset);
            }
            if (instruction == OP_LOOP) {
                return jumpInstruction(info->name, -1, chunk, offset);
            }
            return offset + 3;  // Generic 2-byte operand
        case 4:
            return offset + 5;  // 4-byte immediate
        default:
            return offset + 1;
    }
}
