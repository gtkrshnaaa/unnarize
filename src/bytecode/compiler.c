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
    VM* vm;
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

static void initCompiler(Compiler* c, VM* vm, BytecodeChunk* chunk) {
    c->vm = vm;
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
            } else if (tok.type == TOKEN_STRING) {
                // Handle string literal
                char* str = strndup(tok.start + 1, tok.length - 2); // Strip quotes
                
                ObjString* objStr = internString(c->vm, str, strlen(str));
                free(str); // internString makes its own copy (or interns)
                
                emitConstant(c, OBJ_VAL(objStr), line);
            }
            break;
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
                char* varName = strndup(name.start, name.length);
                ObjString* nameStr = internString(c->vm, varName, name.length);
                free(varName);
                
                int constIdx = addConstant(c->chunk, OBJ_VAL(nameStr));
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

        case NODE_EXPR_CALL: {
            // Check for built-ins first (optimization)
            if (node->call.callee->type == NODE_EXPR_VAR) {
                Token name = node->call.callee->var.name;
                char* funcName = strndup(name.start, name.length);
                
                bool isBuiltin = false;
                if (strcmp(funcName, "array") == 0) {
                    emitByte(c, OP_NEW_ARRAY, line);
                    isBuiltin = true;
                } else if (strcmp(funcName, "map") == 0) {
                    emitByte(c, OP_NEW_MAP, line);
                    isBuiltin = true;
                } else if (strcmp(funcName, "push") == 0) {
                    Node* arg = node->call.arguments;
                    if (arg && arg->next) { // Ensure 2 args
                        compileExpression(c, arg);       // Array
                        compileExpression(c, arg->next); // Value
                        emitByte(c, OP_ARRAY_PUSH, line);
                        emitByte(c, OP_LOAD_NIL, line); 
                    }
                    isBuiltin = true;
                } else if (strcmp(funcName, "pop") == 0) {
                    Node* arg = node->call.arguments;
                    if (arg) {
                        compileExpression(c, arg);
                        emitByte(c, OP_ARRAY_POP, line);
                    }
                    isBuiltin = true;
                } else if (strcmp(funcName, "length") == 0) {
                    Node* arg = node->call.arguments;
                    if (arg) {
                        compileExpression(c, arg); 
                        emitByte(c, OP_ARRAY_LEN, line);
                    }
                    isBuiltin = true;
                }
                
                free(funcName);
                if (isBuiltin) break;
            }
            
            // Regular function call
            compileExpression(c, node->call.callee);
            
            int argCount = 0;
            Node* arg = node->call.arguments;
            while (arg) {
                compileExpression(c, arg);
                argCount++;
                arg = arg->next;
            }
            
            if (argCount == 0) emitByte(c, OP_CALL_0, line);
            else if (argCount == 1) emitByte(c, OP_CALL_1, line);
            else if (argCount == 2) emitByte(c, OP_CALL_2, line);
            else {
                emitByte(c, OP_CALL, line);
                emitByte(c, (uint8_t)argCount, line);
            }
            break;
        }

        case NODE_EXPR_INDEX: {
            // target[index]
            compileExpression(c, node->index.target);
            compileExpression(c, node->index.index);
            emitByte(c, OP_LOAD_INDEX, line);
            break;
        }
        
        case NODE_EXPR_ARRAY_LITERAL: {
            // [1, 2, 3] -> new array then push each
            emitByte(c, OP_NEW_ARRAY, line);
            
            Node* element = node->arrayLiteral.elements;
            while (element) {
                emitByte(c, OP_DUP, line); // Stack: [array, array]
                compileExpression(c, element); // Stack: [array, array, val]
                emitByte(c, OP_ARRAY_PUSH, line); 
                emitByte(c, OP_POP, line); // Remove push result (or duplicate array ref depending on implementation)
                // My OP_ARRAY_PUSH peeks array, pops value. Stack remains: [array, array].
                // So we need POP to remove duplicate array ref.
                
                element = element->next;
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
            // Node: varDecl
            Token name = node->varDecl.name;
            
            // Compile initializer
            if (node->varDecl.initializer) {
                compileExpression(c, node->varDecl.initializer);
            } else {
                emitByte(c, OP_LOAD_NIL, line);
            }

            if (c->scopeDepth > 0) {
                // Local variable
                char* varName = strndup(name.start, name.length);
                int slot = addLocal(c, varName);
                // Note: varName is stored in c->locals, do NOT free it here!
                emitBytes(c, OP_STORE_LOCAL, (uint8_t)slot, line);
            } else {
                // Global variable
                char* varName = strndup(name.start, name.length);
                ObjString* nameStr = internString(c->vm, varName, name.length);
                // free(varName); // internString may or may not take ownership, but Unnarize vm.c seems to copy?
                // View vm.c Step 244: memcpy(strObj->chars, str, length);
                // So internString COPIES. Thus, free(varName) IS CORRECT.
                // Wait.
                // Step 240: allocates ObjString.
                // Step 244: copies chars.
                // So free(varName) IS correct.
                free(varName);
                
                int constIdx = addConstant(c->chunk, OBJ_VAL(nameStr));
                emitBytes(c, OP_DEFINE_GLOBAL, (uint8_t)constIdx, line);
            }
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
                char* varName = strndup(name.start, name.length);
                ObjString* nameStr = internString(c->vm, varName, name.length);
                free(varName);

                int constIdx = addConstant(c->chunk, OBJ_VAL(nameStr));
                emitBytes(c, OP_STORE_GLOBAL, (uint8_t)constIdx, line);
            }
            emitByte(c, OP_POP, line);
            break;
        }

        case NODE_STMT_INDEX_ASSIGN: {
            // target[index] = value;
            compileExpression(c, node->indexAssign.target);
            compileExpression(c, node->indexAssign.index);
            compileExpression(c, node->indexAssign.value);
            
            emitByte(c, OP_STORE_INDEX, line);
            emitByte(c, OP_POP, line); // Statement should not leave value on stack
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
            // emitByte(c, OP_HOTSPOT_CHECK, line); // TEMPORARY DISABLE FOR DEBUGGING
            
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
                // emitByte(c, OP_POP, line); // Runtime pop
                // Unnarize bytecode uses manual POP for locals going out of scope?
                // Or does it just reset stack top on return?
                // Block scope locals need explicit pop.
                emitByte(c, OP_POP, line); 
                c->localCount--;
            }
            break;
        }

        case NODE_STMT_FUNCTION: {
            // Manually allocate to avoid private macro dependency
            Function* func = malloc(sizeof(Function));
            func->obj.type = OBJ_FUNCTION;
            func->obj.isMarked = false;
            // Link to GC
            func->obj.next = c->vm->objects;
            c->vm->objects = (Obj*)func;
            
            func->name = node->function.name;
            func->params = node->function.params;
            func->paramCount = node->function.paramCount;
            func->isNative = false;
            func->isAsync = false;
            
            // Create independent chunk for function
            func->bytecodeChunk = malloc(sizeof(BytecodeChunk));
            initChunk(func->bytecodeChunk);
            
            // Compile function body in new context
            Compiler funcCompiler;
            initCompiler(&funcCompiler, c->vm, func->bytecodeChunk);
            
            // Add parameters as locals (slot 0..N)
            for (int i = 0; i < func->paramCount; i++) {
                Token p = func->params[i];
                char* pname = strndup(p.start, p.length);
                addLocal(&funcCompiler, pname);
            }
            
            // Compile body
            compileNode(&funcCompiler, node->function.body);
            
            // Ensure return at end
            emitByte(&funcCompiler, OP_LOAD_NIL, line);
            emitByte(&funcCompiler, OP_RETURN, line);
            
            // Store function object in parent
            int constIdx = addConstant(c->chunk, OBJ_VAL(func));
            
            if (c->scopeDepth == 0) {
                // Global function
                char* funcNameStr = strndup(node->function.name.start, node->function.name.length);
                ObjString* nameObj = internString(c->vm, funcNameStr, node->function.name.length);
                free(funcNameStr);
                int nameIdx = addConstant(c->chunk, OBJ_VAL(nameObj));
                
                emitBytes(c, OP_LOAD_CONST, (uint8_t)constIdx, line);
                emitBytes(c, OP_DEFINE_GLOBAL, (uint8_t)nameIdx, line);
            } else {
                // Local function
                char* funcNameStr = strndup(node->function.name.start, node->function.name.length);
                int slot = addLocal(c, funcNameStr);
                emitBytes(c, OP_LOAD_CONST, (uint8_t)constIdx, line);
                emitBytes(c, OP_STORE_LOCAL, (uint8_t)slot, line);
            }
            break;
        }
        
        case NODE_STMT_RETURN: {
            if (node->returnStmt.value) {
                compileExpression(c, node->returnStmt.value);
                emitByte(c, OP_RETURN, line);
            } else {
                emitByte(c, OP_RETURN_NIL, line);
            }
            break;
        }
        
        default:
            break;
    }
}

static void compileNode(Compiler* c, Node* node) {
    if (!node) return;
    
    // Fix dispatch: check for ANY statement type
    // In parser.h, VAR_DECL is before PRINT.
    // We should check if it's a statement vs expression.
    // Simplest is to check if it's NOT an expression.
    if (node->type >= NODE_STMT_VAR_DECL && node->type <= NODE_STMT_PROP_ASSIGN) {
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

bool compileToBytecode(VM* vm, Node* ast, BytecodeChunk* chunk) {
    Compiler compiler;
    initCompiler(&compiler, vm, chunk);
    
    // Parser wraps program in a BLOCK. We must compile contents as global scope (0).
    // Do not call compileNode(BLOCK) because it increments scope depth.
    if (ast && ast->type == NODE_STMT_BLOCK) {
        for (int i = 0; i < ast->block.count; i++) {
            compileNode(&compiler, ast->block.statements[i]);
        }
    } else {
        compileNode(&compiler, ast);
    }
    
    // Emit halt
    emitByte(&compiler, OP_HALT, 1);
    
    return !compiler.hadError;
}
