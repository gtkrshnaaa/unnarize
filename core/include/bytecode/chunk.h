#ifndef BYTECODE_CHUNK_H
#define BYTECODE_CHUNK_H

#include <stdint.h>
#include "vm.h"
#include "bytecode/opcodes.h"

/**
 * Bytecode Chunk - Container for compiled register-based bytecode
 *
 * Uses 32-bit instruction words for fixed-width decoding.
 * Optimized for cache-friendly sequential execution.
 */

typedef struct BytecodeChunk {
    // === Code Stream (32-bit instructions) ===
    uint32_t* code;             // Register-based instruction words
    int codeSize;               // Number of instructions
    int codeCapacity;           // Allocated capacity (in instruction count)

    // === Constant Pool ===
    Value* constants;           // Runtime constants (numbers, strings, etc)
    int constantCount;
    int constantCapacity;

    // === Debug Info ===
    int* lineNumbers;           // Line number per instruction
    int lineCapacity;

    // === Register Info ===
    int maxRegs;                // Maximum register index used by this chunk
} BytecodeChunk;

// Chunk lifecycle
void initChunk(BytecodeChunk* chunk);
void freeChunk(BytecodeChunk* chunk);

// Code emission (32-bit instruction words)
void writeChunk(BytecodeChunk* chunk, uint32_t instruction, int line);

// Patch jump target at given instruction index
void patchJump(BytecodeChunk* chunk, int offset);

// Constant pool
int addConstant(BytecodeChunk* chunk, Value value);

// Debug
void disassembleChunk(BytecodeChunk* chunk, const char* name);
int disassembleInstruction(BytecodeChunk* chunk, int offset);

#endif // BYTECODE_CHUNK_H
