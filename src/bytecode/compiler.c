#include "bytecode/compiler.h"
#include "bytecode/opcodes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * AST → Bytecode Compiler
 * Production-grade, zero external dependencies
 */

typedef struct {
    BytecodeChunk* chunk;
    
    // Local variable tracking
    struct {
        const char* name;
        int depth;
        int slot;
    } locals[256];
    int localCount;
    int scopeDepth;
    
    // Error tracking
    bool hadError;
} Compiler;

static void initCompiler(Compiler* c, BytecodeChunk* chunk) {
    c->chunk = chunk;
    c->localCount = 0;
    c->scopeDepth = 0;
    c->hadError = false;
}

// Emit single byte
static void emitByte(Compiler* c, uint8_t byte, int line) {
    writeChunk(c->chunk, byte, line);
}

// Emit two bytes
static void emitBytes(Compiler* c, uint8_t b1, uint8_t b2, int line) {
    emitByte(c, b1, line);
    emitByte(c, b2, line);
}

// Emit constant
static void emitConstant(Compiler* c, Value value, int line) {
    int index = addConstant(c->chunk, value);
    emitBytes(c, OP_LOAD_CONST, (uint8_t)index, line);
}

// Emit jump
static int emitJump(Compiler* c, uint8_t instruction, int line) {
    emitByte(c, instruction, line);
    emitByte(c, 0xff, line);  // Placeholder
    emitByte(c, 0xff, line);
    return c->chunk->codeSize - 2;
}

// Add local variable
static int addLocal(Compiler* c, const char* name) {
    if (c->localCount >= 256) {
        fprintf(stderr, "Too many local variables\n");
        c->hadError = true;
        return -1;
    }
    
    c->locals[c->localCount].name = name;
    c->locals[c->localCount].depth = c->scopeDepth;
    c->locals[c->localCount].slot = c->localCount;
    return c->localCount++;
}

// Resolve local variable
static int resolveLocal(Compiler* c, const char* name, int length) {
    for (int i = c->localCount - 1; i >= 0; i--) {
        if (strlen(c->locals[i].name) == (size_t)length &&
            memcmp(c->locals[i].name, name, length) == 0) {
            return c->locals[i].slot;
        }
    }
    return -1;  // Not found, assume global
}

// Forward declarations
static void compileNode(Compiler* c, Node* node);
static void compileExpression(Compiler* c, Node* node);
static void compileStatement(Compiler* c, Node* node);

// Compile expression
static void compileExpression(Compiler* c, Node* node) {
    if (!node) return;
    
    int line = 1;  // TODO: Get from node
    
    switch (node->type) {
        case NODE_EXPR_LITERAL: {
            Token tok = node->literal.token;
            if (tok.type == TOKEN_NUMBER) {
                // Parse number
                char* str = strndup(tok.start, tok.length);
                int64_t val = atoll(str);
                free(str);
                emitConstant(c, INT_VAL(val), line);
            } else if (tok.type == TOKEN_TRUE) {
                emitByte(c, OP_LOAD_TRUE, line);
            } else if (tok.type == TOKEN_FALSE) {
                emitByte(c, OP_LOAD_FALSE, line);
            } else if (tok.type == TOKEN_NIL) {
                emitByte(c, OP_LOAD_NIL, line);
            }
            break;
        }
        
        case NODE_EXPR_VAR: {
            // Variable load
            Token name = node->var.name;
            int local = resolveLocal(c, name.start, name.length);
            if (local != -1) {
                emitBytes(c, OP_LOAD_LOCAL, (uint8_t)local, line);
            } else {
                // Global variable
                int constIdx = addConstant(c->chunk, OBJ_VAL(NULL));  // TODO: proper string
                emitBytes(c, OP_LOAD_GLOBAL, (uint8_t)constIdx, line);
            }
            break;
        }
        
        case NODE_EXPR_BINARY: {
            // Compile operands
            compileExpression(c, node->binary.left);
            compileExpression(c, node->binary.right);
            
            // Emit operation
            switch (node->binary.op.type) {
                case TOKEN_PLUS:   emitByte(c, OP_ADD, line); break;
                case TOKEN_MINUS:  emitByte(c, OP_SUB, line); break;
                case TOKEN_STAR:   emitByte(c, OP_MUL, line); break;
                case TOKEN_SLASH:  emitByte(c, OP_DIV, line); break;
                case TOKEN_PERCENT: emitByte(c, OP_MOD, line); break;
                case TOKEN_LESS:   emitByte(c, OP_LT, line); break;
                case TOKEN_LESS_EQUAL: emitByte(c, OP_LE, line); break;
                case TOKEN_GREATER: emitByte(c, OP_GT, line); break;
                case TOKEN_GREATER_EQUAL: emitByte(c, OP_GE, line); break;
                case TOKEN_EQUAL_EQUAL: emitByte(c, OP_EQ, line); break;
                default: break;
            }
            break;
        }
        
        case NODE_EXPR_UNARY: {
            compileExpression(c, node->unary.expr);
            if (node->unary.op.type == TOKEN_MINUS) {
                emitByte(c, OP_NEG, line);
            } else if (node->unary.op.type == TOKEN_BANG) {
                emitByte(c, OP_NOT, line);
            }
            break;
        }
        
        default:
            break;
    }
}

// Compile statement
static void compileStatement(Compiler* c, Node* node) {
    if (!node) return;
    
    int line = 1;
    
    switch (node->type) {
        case NODE_STMT_PRINT: {
            compileExpression(c, node->print.expr);
            emitByte(c, OP_PRINT, line);
            break;
        }
        
        case NODE_STMT_VAR_DECL: {
            // Add local
            Token name = node->varDecl.name;
            char* varName = strndup(name.start, name.length);
            int slot = addLocal(c, varName);
            
            // Compile initializer
            if (node->varDecl.initializer) {
                compileExpression(c, node->varDecl.initializer);
            } else {
                emitByte(c, OP_LOAD_NIL, line);
            }
            
            // Store to local
            emitBytes(c, OP_STORE_LOCAL, (uint8_t)slot, line);
            emitByte(c, OP_POP, line);  // Pop the value
            break;
        }
        
        case NODE_STMT_ASSIGN: {
            // Compile value
            compileExpression(c, node->assign.value);
            
            // Store to variable
            Token name = node->assign.name;
            int local = resolveLocal(c, name.start, name.length);
            if (local != -1) {
                emitBytes(c, OP_STORE_LOCAL, (uint8_t)local, line);
            } else {
                int constIdx = addConstant(c->chunk, OBJ_VAL(NULL));
                emitBytes(c, OP_STORE_GLOBAL, (uint8_t)constIdx, line);
            }
            emitByte(c, OP_POP, line);
            break;
        }
        
        case NODE_STMT_IF: {
            // Compile condition
            compileExpression(c, node->ifStmt.condition);
            
            // Jump to else if false
            int thenJump = emitJump(c, OP_JUMP_IF_FALSE, line);
            emitByte(c, OP_POP, line);  // Pop condition
            
            // Then branch
            compileStatement(c, node->ifStmt.thenBranch);
            
            int elseJump = emitJump(c, OP_JUMP, line);
            
            // Patch then jump
            patchJump(c->chunk, thenJump);
            emitByte(c, OP_POP, line);
            
            // Else branch
            if (node->ifStmt.elseBranch) {
                compileStatement(c, node->ifStmt.elseBranch);
            }
            
            patchJump(c->chunk, elseJump);
            break;
        }
        
        case NODE_STMT_WHILE: {
            int loopStart = c->chunk->codeSize;
            
            // Loop header marker for OSR
            emitByte(c, OP_LOOP_HEADER, line);
            
            // Hotspot check
            emitByte(c, OP_HOTSPOT_CHECK, line);
            
            // Condition
            compileExpression(c, node->whileStmt.condition);
            
            // Exit if false
            int exitJump = emitJump(c, OP_JUMP_IF_FALSE, line);
            emitByte(c, OP_POP, line);
            
            // Body
            compileStatement(c, node->whileStmt.body);
            
            // Loop back
            int offset = c->chunk->codeSize - loopStart + 2;
            emitByte(c, OP_LOOP, line);
            emitByte(c, (offset >> 8) & 0xff, line);
            emitByte(c, offset & 0xff, line);
            
            // Patch exit
            patchJump(c->chunk, exitJump);
            emitByte(c, OP_POP, line);
            break;
        }
        
        case NODE_STMT_BLOCK: {
            c->scopeDepth++;
            for (int i = 0; i < node->block.count; i++) {
                compileNode(c, node->block.statements[i]);
            }
            c->scopeDepth--;
            
            // Pop locals
            while (c->localCount > 0 &&
                   c->locals[c->localCount - 1].depth > c->scopeDepth) {
                c->localCount--;
            }
            break;
        }
        
        default:
            break;
    }
}

static void compileNode(Compiler* c, Node* node) {
    if (!node) return;
    
    if (node->type >= NODE_STMT_PRINT && node->type <= NODE_STMT_LOADEXTERN) {
        compileStatement(c, node);
    } else {
        compileExpression(c, node);
        emitByte(c, OP_POP, 1);  // Pop unused expression result
    }
    
    // Compile next node in sequence
    if (node->next) {
        compileNode(c, node->next);
    }
}

bool compileToBytecode(Node* ast, BytecodeChunk* chunk) {
    Compiler compiler;
    initCompiler(&compiler, chunk);
    
    compileNode(&compiler, ast);
    
    // Emit halt
    emitByte(&compiler, OP_HALT, 1);
    
    return !compiler.hadError;
}
