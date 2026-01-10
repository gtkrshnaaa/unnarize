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
    
    // === Profiling Data (for JIT tier decisions) ===
    uint32_t* hotspots;         // Execution counter per instruction
    int hotspotCapacity;
    
    // === Inline Cache Slots ===
    // For property access optimization
    struct {
        void* cachedObject;     // Cached object shape
        int cachedOffset;       // Cached property offset
        uint32_t hits;          // Cache hit count
        uint32_t misses;        // Cache miss count
    }* icSlots;
    int icSlotCount;
    int icSlotCapacity;
    
    // === JIT Metadata ===
    void** jitCode;             // Pointers to compiled native code
    int jitCodeCount;
    int jitCodeCapacity;
    
    // Compilation state
    bool isCompiled;            // Has been JIT compiled?
    int tierLevel;              // 0=bytecode, 1=baseline, 2=optimized
    
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
