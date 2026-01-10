#ifndef JIT_COMPILER_H
#define JIT_COMPILER_H

#include "vm.h"
#include "bytecode/chunk.h"
#include <stddef.h>
#include <stdbool.h>

/**
 * Baseline JIT Compiler
 * 
 * Template-based compilation: 1:1 mapping from bytecode to native x86-64.
 * No advanced optimizations - those come in Phase 3 (Optimizing JIT).
 * 
 * Design:
 * - Simple register allocation (RAX, RBX, RCX, RDX for stack top 4 values)
 * - Spill to memory when stack depth > 4
 * - Direct translation of bytecode operations
 * - Fallback to interpreter for complex operations
 */

// Compiled native function
typedef struct {
    void* code;         // Executable machine code
    size_t codeSize;    // Size of code in bytes
    int stackDepth;     // Maximum stack depth
    bool isValid;       // Compilation succeeded?
} JITFunction;

/**
 * Compile bytecode chunk to native x86-64 code
 * 
 * @param vm Virtual machine context
 * @param chunk Bytecode chunk to compile
 * @return Compiled function, or NULL on failure
 * 
 * The compiled function follows System V AMD64 ABI:
 * - RDI: VM pointer (first argument)
 * - RAX: Return value
 * - Callee-saved: RBX, R12-R15, RBP
 */
JITFunction* compileFunction(VM* vm, BytecodeChunk* chunk);

/**
 * Execute JIT-compiled function
 * 
 * @param vm Virtual machine context
 * @param jitFunc Compiled function
 * @return Return value from function
 */
Value executeJIT(VM* vm, JITFunction* jitFunc);

/**
 * Free compiled function
 * 
 * @param jitFunc Function to free
 */
void freeJITFunction(JITFunction* jitFunc);

#endif // JIT_COMPILER_H
