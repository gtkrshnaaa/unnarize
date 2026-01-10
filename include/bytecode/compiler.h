#ifndef BYTECODE_COMPILER_H
#define BYTECODE_COMPILER_H

#include "parser.h"
#include "bytecode/chunk.h"

/**
 * Bytecode Compiler - AST to Bytecode
 * Zero external dependencies
 */

// Compile AST to bytecode chunk
bool compileToBytecode(VM* vm, Node* ast, BytecodeChunk* chunk);

#endif // BYTECODE_COMPILER_H
