#ifndef BYTECODE_CHUNK_H
#define BYTECODE_CHUNK_H

#include <stdint.h>
#include "vm.h"
#include "bytecode/opcodes.h"

/**
 * Bytecode Chunk - Container for compiled bytecode
 * 
 * Self-contained, no external dependencies
 * Optimized for cache-friendly sequential execution
 */

typedef struct BytecodeChunk {
    // === Code Stream ===
    uint8_t* code;              // Bytecode instructions
    int codeSize;               // Number of bytes
    int codeCapacity;           // Allocated capacity
    
    // === Constant Pool ===
    Value* constants;           // Runtime constants (numbers, strings, etc)
    int constantCount;
    int constantCapacity;
    
    // === Line Number Mapping (Debug Info) ===
    int* lineNumbers;           // Line number for each instruction
    int lineCapacity;
    

    
} BytecodeChunk;

// Chunk lifecycle
void initChunk(BytecodeChunk* chunk);
void freeChunk(BytecodeChunk* chunk);

// Code emission
void writeChunk(BytecodeChunk* chunk, uint8_t byte, int line);
void patchJump(BytecodeChunk* chunk, int offset);

// Constant pool
int addConstant(BytecodeChunk* chunk, Value value);

// Debug
void disassembleChunk(BytecodeChunk* chunk, const char* name);
int disassembleInstruction(BytecodeChunk* chunk, int offset);

#endif // BYTECODE_CHUNK_H
