#ifndef BYTECODE_INTERPRETER_H
#define BYTECODE_INTERPRETER_H

#include "bytecode/chunk.h"
#include "vm.h"

/**
 * Fast Bytecode Interpreter
 * Direct-threaded dispatch with computed goto
 * Target: 200M ops/sec
 */

// Execute bytecode chunk
uint64_t executeBytecode(VM* vm, BytecodeChunk* chunk, int entryStackDepth);

#endif // BYTECODE_INTERPRETER_H
