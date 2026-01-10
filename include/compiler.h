#ifndef COMPILER_H
#define COMPILER_H

#include "common.h"
#include "bytecode.h"
#include "parser.h"

// Compiler state for AST → Bytecode transformation
typedef struct Compiler {
    Chunk* chunk;               // Current chunk being compiled
    
    // Local variable tracking
    struct {
        Token name;
        int depth;
    } locals[256];
    int localCount;
    int scopeDepth;
    
    // For error reporting
    bool hadError;
    bool panicMode;
} Compiler;

// Initialize compiler
void initCompiler(Compiler* compiler, Chunk* chunk);

// Compile AST to bytecode
bool compile(Node* ast, Chunk* chunk);

#endif // COMPILER_H
