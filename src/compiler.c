#include "compiler.h"
#include <stdio.h>
#include <string.h>

// Forward declarations
static void compileNode(Compiler* c, Node* node);
static void compileExpression(Compiler* c, Node* node);
static void compileStatement(Compiler* c, Node* node);

// Error handling
static void errorAt(Compiler* c, const char* message) {
    if (c->panicMode) return;
    c->panicMode = true;
    c->hadError = true;
    fprintf(stderr, "[Compiler Error] %s\n", message);
}

// Emit a single byte
static void emitByte(Compiler* c, uint8_t byte) {
    writeChunk(c->chunk, byte, 0); // Line number tracking TODO
}

// Emit two bytes
static void emitBytes(Compiler* c, uint8_t byte1, uint8_t byte2) {
    emitByte(c, byte1);
    emitByte(c, byte2);
}

// Emit a return instruction
static void emitReturn(Compiler* c) {
    emitByte(c, OP_RETURN);
}

// Emit a constant instruction
static void emitConstant(Compiler* c, Value value) {
    int constant = addConstant(c->chunk, value);
    if (constant > 255) {
        errorAt(c, "Too many constants in one chunk.");
        return;
    }
    emitBytes(c, OP_CONSTANT, (uint8_t)constant);
}

// Initialize compiler
void initCompiler(Compiler* compiler, Chunk* chunk) {
    compiler->chunk = chunk;
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->hadError = false;
    compiler->panicMode = false;
}

// === Local variable handling ===

static void beginScope(Compiler* c) {
    c->scopeDepth++;
}

static void endScope(Compiler* c) {
    c->scopeDepth--;
    
    // Pop all locals in this scope
    while (c->localCount > 0 && 
           c->locals[c->localCount - 1].depth > c->scopeDepth) {
        emitByte(c, OP_POP);
        c->localCount--;
    }
}

static int resolveLocal(Compiler* c, Token* name) {
    // Search locals from top to bottom (innermost to outermost)
    for (int i = c->localCount - 1; i >= 0; i--) {
        if (c->locals[i].name.length == name->length &&
            memcmp(c->locals[i].name.start, name->start, name->length) == 0) {
            return i;
        }
    }
    return -1; // Not found, assume global
}

static void addLocal(Compiler* c, Token name) {
    if (c->localCount == 256) {
        errorAt(c, "Too many local variables in scope.");
        return;
    }
    c->locals[c->localCount].name = name;
    c->locals[c->localCount].depth = c->scopeDepth;
    c->localCount++;
}

// === Expression compilation ===

static void compileLiteral(Compiler* c, Node* node) {
    Token token = node->literal.token;
    
    switch (token.type) {
        case TOKEN_NUMBER: {
            // Parse number
            char* str = strndup(token.start, token.length);
            
            // Check if it's a float
            if (strchr(str, '.') != NULL) {
                double value = atof(str);
                Value val = {.type = VAL_FLOAT, .as.floatNum = value};
                emitConstant(c, val);
            } else {
                long value = atol(str);
                Value val = {.type = VAL_INT, .as.integer = value};
                emitConstant(c, val);
            }
            free(str);
            break;
        }
        
        case TOKEN_STRING: {
            // String literal (skip quotes)
            char* str = strndup(token.start + 1, token.length - 2);
            
            // Create string object (simplified - needs proper VM integration)
            Value val = {.type = VAL_STRING};
            // TODO: Proper string object creation
            // For now, just store as placeholder
            emitConstant(c, val);
            free(str);
            break;
        }
        
        case TOKEN_TRUE:
            emitByte(c, OP_TRUE);
            break;
            
        case TOKEN_FALSE:
            emitByte(c, OP_FALSE);
            break;
            
        case TOKEN_NIL:
            emitByte(c, OP_NIL);
            break;
            
        default:
            errorAt(c, "Unknown literal type");
            break;
    }
}

static void compileVariable(Compiler* c, Node* node) {
    Token name = node->var.name;
    
    // Check if it's a local variable
    int slot = node->var.slot;
    
    if (slot != -1) {
        // Local variable
        emitBytes(c, OP_GET_LOCAL, (uint8_t)slot);
    } else {
        // Global variable - add name to constant pool
        char* varName = strndup(name.start, name.length);
        Value nameVal = {.type = VAL_STRING}; // TODO: Proper string object
        int nameConstant = addConstant(c->chunk, nameVal);
        free(varName);
        
        emitBytes(c, OP_GET_GLOBAL, (uint8_t)nameConstant);
    }
}

static void compileBinary(Compiler* c, Node* node) {
    // Compile left operand
    compileExpression(c, node->binary.left);
    
    // Compile right operand
    compileExpression(c, node->binary.right);
    
    // Emit operation
    switch (node->binary.op.type) {
        case TOKEN_PLUS:
            emitByte(c, OP_ADD);
            break;
        case TOKEN_MINUS:
            emitByte(c, OP_SUBTRACT);
            break;
        case TOKEN_STAR:
            emitByte(c, OP_MULTIPLY);
            break;
        case TOKEN_SLASH:
            emitByte(c, OP_DIVIDE);
            break;
        case TOKEN_PERCENT:
            emitByte(c, OP_MODULO);
            break;
        case TOKEN_EQUAL_EQUAL:
            emitByte(c, OP_EQUAL);
            break;
        case TOKEN_BANG_EQUAL:
            emitByte(c, OP_NOT_EQUAL);
            break;
        case TOKEN_GREATER:
            emitByte(c, OP_GREATER);
            break;
        case TOKEN_LESS:
            emitByte(c, OP_LESS);
            break;
        case TOKEN_GREATER_EQUAL:
            emitByte(c, OP_GREATER_EQUAL);
            break;
        case TOKEN_LESS_EQUAL:
            emitByte(c, OP_LESS_EQUAL);
            break;
        default:
            errorAt(c, "Unknown binary operator");
            break;
    }
}

static void compileUnary(Compiler* c, Node* node) {
    // Compile operand
    compileExpression(c, node->unary.expr);
    
    // Emit operation
    switch (node->unary.op.type) {
        case TOKEN_MINUS:
            emitByte(c, OP_NEGATE);
            break;
        case TOKEN_BANG:
            emitByte(c, OP_NOT);
            break;
        default:
            errorAt(c, "Unknown unary operator");
            break;
    }
}

static void compileExpression(Compiler* c, Node* node) {
    if (!node) return;
    
    switch (node->type) {
        case NODE_EXPR_LITERAL:
            compileLiteral(c, node);
            break;
        case NODE_EXPR_VAR:
            compileVariable(c, node);
            break;
        case NODE_EXPR_BINARY:
            compileBinary(c, node);
            break;
        case NODE_EXPR_UNARY:
            compileUnary(c, node);
            break;
        case NODE_EXPR_CALL:
            // TODO: Function calls
            errorAt(c, "Function calls not yet implemented in bytecode compiler");
            break;
        case NODE_EXPR_ARRAY_LITERAL:
            // TODO: Arrays
            errorAt(c, "Arrays not yet implemented in bytecode compiler");
            break;
        default:
            errorAt(c, "Unknown expression type");
            break;
    }
}

// === Statement compilation ===

static void compilePrint(Compiler* c, Node* node) {
    compileExpression(c, node->print.expr);
    emitByte(c, OP_PRINT);
}

static void compileVarDecl(Compiler* c, Node* node) {
    // Compile initializer
    if (node->varDecl.initializer) {
        compileExpression(c, node->varDecl.initializer);
    } else {
        emitByte(c, OP_NIL);
    }
    
    // Check if local or global
    int slot = node->varDecl.slot;
    
    if (slot != -1) {
        // Local variable - already on stack, just record in locals
        // (Resolver already assigned slot)
    } else {
        // Global variable
        char* varName = strndup(node->varDecl.name.start, node->varDecl.name.length);
        Value nameVal = {.type = VAL_STRING}; // TODO: Proper string
        int nameConstant = addConstant(c->chunk, nameVal);
        free(varName);
        
        emitBytes(c, OP_DEFINE_GLOBAL, (uint8_t)nameConstant);
    }
}

static void compileAssignment(Compiler* c, Node* node) {
    // Compile value
    compileExpression(c, node->assign.value);
    
    // Store in variable
    int slot = node->assign.slot;
    
    if (slot != -1) {
        // Local
        emitBytes(c, OP_SET_LOCAL, (uint8_t)slot);
    } else {
        // Global
        char* varName = strndup(node->assign.name.start, node->assign.name.length);
        Value nameVal = {.type = VAL_STRING}; // TODO
        int nameConstant = addConstant(c->chunk, nameVal);
        free(varName);
        
        emitBytes(c, OP_SET_GLOBAL, (uint8_t)nameConstant);
    }
}

static void compileBlock(Compiler* c, Node* node) {
    beginScope(c);
    
    for (int i = 0; i < node->block.count; i++) {
        compileStatement(c, node->block.statements[i]);
    }
    
    endScope(c);
}

static void compileIf(Compiler* c, Node* node) {
    // Compile condition
    compileExpression(c, node->ifStmt.condition);
    
    // Jump if false
    int thenJump = c->chunk->count;
    emitBytes(c, OP_JUMP_IF_FALSE, 0xFF); // Placeholder
    emitByte(c, 0xFF);
    
    emitByte(c, OP_POP); // Pop condition
    
    // Compile then branch
    compileStatement(c, node->ifStmt.thenBranch);
    
    // Jump over else
    int elseJump = c->chunk->count;
    emitBytes(c, OP_JUMP, 0xFF);
    emitByte(c, 0xFF);
    
    // Patch then jump
    int thenOffset = c->chunk->count - thenJump - 3;
    c->chunk->code[thenJump + 1] = (thenOffset >> 8) & 0xFF;
    c->chunk->code[thenJump + 2] = thenOffset & 0xFF;
    
    emitByte(c, OP_POP); // Pop condition for else branch
    
    // Compile else branch
    if (node->ifStmt.elseBranch) {
        compileStatement(c, node->ifStmt.elseBranch);
    }
    
    // Patch else jump
    int elseOffset = c->chunk->count - elseJump - 3;
    c->chunk->code[elseJump + 1] = (elseOffset >> 8) & 0xFF;
    c->chunk->code[elseJump + 2] = elseOffset & 0xFF;
}

static void compileWhile(Compiler* c, Node* node) {
    int loopStart = c->chunk->count;
    
    // Compile condition
    compileExpression(c, node->whileStmt.condition);
    
    // Jump if false (exit loop)
    int exitJump = c->chunk->count;
    emitBytes(c, OP_JUMP_IF_FALSE, 0xFF);
    emitByte(c, 0xFF);
    
    emitByte(c, OP_POP); // Pop condition
    
    // Compile body
    compileStatement(c, node->whileStmt.body);
    
    // Loop back
    int offset = c->chunk->count - loopStart + 3;
    emitBytes(c, OP_LOOP, (offset >> 8) & 0xFF);
    emitByte(c, offset & 0xFF);
    
    // Patch exit jump
    int exitOffset = c->chunk->count - exitJump - 3;
    c->chunk->code[exitJump + 1] = (exitOffset >> 8) & 0xFF;
    c->chunk->code[exitJump + 2] = exitOffset & 0xFF;
    
    emitByte(c, OP_POP); // Pop condition
}

static void compileStatement(Compiler* c, Node* node) {
    if (!node) return;
    
    switch (node->type) {
        case NODE_STMT_PRINT:
            compilePrint(c, node);
            break;
        case NODE_STMT_VAR_DECL:
            compileVarDecl(c, node);
            break;
        case NODE_STMT_ASSIGN:
            compileAssignment(c, node);
            break;
        case NODE_STMT_BLOCK:
            compileBlock(c, node);
            break;
        case NODE_STMT_IF:
            compileIf(c, node);
            break;
        case NODE_STMT_WHILE:
            compileWhile(c, node);
            break;
        case NODE_STMT_FOR:
            // TODO: For loops
            errorAt(c, "For loops not yet implemented in bytecode compiler");
            break;
        case NODE_STMT_RETURN:
            if (node->returnStmt.value) {
                compileExpression(c, node->returnStmt.value);
            } else {
                emitByte(c, OP_NIL);
            }
            emitReturn(c);
            break;
        default:
            // Try as expression statement
            compileExpression(c, node);
            emitByte(c, OP_POP); // Discard expression result
            break;
    }
}

static void compileNode(Compiler* c, Node* node) {
    if (node->type >= NODE_EXPR_LITERAL && node->type <= NODE_EXPR_ARRAY_LITERAL) {
        compileExpression(c, node);
        emitByte(c, OP_POP); // Top-level expressions are discarded
    } else {
        compileStatement(c, node);
    }
}

// Main compile function
bool compile(Node* ast, Chunk* chunk) {
    Compiler compiler;
    initCompiler(&compiler, chunk);
    
    // Compile the AST
    compileNode(&compiler, ast);
    
    // Emit final halt
    emitByte(&compiler, OP_HALT);
    
    return !compiler.hadError;
}
