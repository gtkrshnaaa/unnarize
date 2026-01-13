#include "bytecode/compiler.h"
#include "bytecode/opcodes.h"
#include "lexer.h"

// #define DEBUG_PRINT_CODE

#ifdef DEBUG_PRINT_CODE
#include "bytecode/chunk.h" // disassemble header is in chunk.h
#endif

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
    const char* modulePath;
    
    // Error tracking
    bool hadError;
} Compiler;

static void initCompiler(Compiler* c, VM* vm, BytecodeChunk* chunk, const char* modulePath) {
    c->vm = vm;
    c->chunk = chunk;
    c->modulePath = modulePath;
    c->hadError = false;
    c->scopeDepth = 0;
    
    // Reserve slot 0 for the function/script itself
    c->localCount = 1;
    c->locals[0].depth = 0;
    c->locals[0].name = ""; 
    c->locals[0].slot = 0;
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
                if (strchr(str, '.')) {
                    double val = atof(str);
                    emitConstant(c, FLOAT_VAL(val), line);
                } else {
                    int64_t val = atoll(str);
                    emitConstant(c, INT_VAL(val), line);
                }
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
                        emitByte(c, OP_ARRAY_PUSH_CLEAN, line);
                        emitByte(c, OP_LOAD_NIL, line); 
                        emitByte(c, OP_LOAD_NIL, line); // Stack adjustment for push operation
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
                // OP_ARRAY_PUSH peeks array, pops value. Stack remains: [array, array].
                // We need POP to remove duplicate array ref.
                
                element = element->next;
            }
            break;
        }

        case NODE_EXPR_GET: {
            compileExpression(c, node->get.object);
            
            Token name = node->get.name;
            char* propName = strndup(name.start, name.length);
            ObjString* nameStr = internString(c->vm, propName, name.length);
            free(propName);
            
            int constIdx = addConstant(c->chunk, OBJ_VAL(nameStr));
            emitBytes(c, OP_LOAD_PROPERTY, (uint8_t)constIdx, line);
            break;
        }
        
        case NODE_STMT_ASSIGN: {
            compileExpression(c, node->assign.value);
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

            // emitByte(c, OP_POP, line); // Pop assignment value (FIX: Expression should return value)
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
                c->locals[slot].depth = c->scopeDepth; // Initialize depth
                emitBytes(c, OP_STORE_LOCAL, (uint8_t)slot, line);
            } else {
                // Global variable
                char* varName = strndup(name.start, name.length);
                ObjString* nameStr = internString(c->vm, varName, name.length);
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

            
            // Condition
            compileExpression(c, node->whileStmt.condition);
            
            // Exit if false
            int exitJump = emitJump(c, OP_JUMP_IF_FALSE, line);
            emitByte(c, OP_POP, line);
            
            // Body
            compileStatement(c, node->whileStmt.body);
            
            // Loop back
            int offset = c->chunk->codeSize - loopStart + 3;
            emitByte(c, OP_LOOP, line);
            emitByte(c, (offset >> 8) & 0xff, line);
            emitByte(c, offset & 0xff, line);
            
            // Patch exit
            patchJump(c->chunk, exitJump);
            emitByte(c, OP_POP, line);
            break;
        }
        



        case NODE_STMT_FOREACH: {
            // foreach (var item : collection) body
            c->scopeDepth++;
            
            // 1. Evaluate collection -> local ".col"
            compileExpression(c, node->foreachStmt.collection);
            addLocal(c, strdup(".col"));
            emitBytes(c, OP_STORE_LOCAL, (uint8_t)(c->localCount - 1), line);
            emitBytes(c, OP_STORE_LOCAL, (uint8_t)(c->localCount - 1), line);
            // Do NOT pop! Reserve slot. 

            // 2. Index -> local ".idx" = 0
            int zeroIdx = addConstant(c->chunk, INT_VAL(0));
            emitBytes(c, OP_LOAD_CONST, (uint8_t)zeroIdx, line);
            addLocal(c, strdup(".idx"));
            emitBytes(c, OP_STORE_LOCAL, (uint8_t)(c->localCount - 1), line);
            emitBytes(c, OP_STORE_LOCAL, (uint8_t)(c->localCount - 1), line);
            // Do NOT pop! Reserve slot.

            // 3. Loop Start
            int loopStart = c->chunk->codeSize;
            emitByte(c, OP_LOOP_HEADER, line);

            // 4. Condition: .idx < len(.col)
            // Load .idx
            emitBytes(c, OP_LOAD_LOCAL, (uint8_t)(c->localCount -1), line); 
            
            // Load .col
            emitBytes(c, OP_LOAD_LOCAL, (uint8_t)(c->localCount -2), line);
            emitByte(c, OP_ARRAY_LEN, line);
            emitByte(c, OP_LT, line);
            
            // 5. Exit if false
            int exitJump = emitJump(c, OP_JUMP_IF_FALSE, line);
            emitByte(c, OP_POP, line); // Pop condition

            // 6. Load item: .col[.idx] -> userVar
            // Load .col
            emitBytes(c, OP_LOAD_LOCAL, (uint8_t)(c->localCount -2), line);
            // Load .idx
            emitBytes(c, OP_LOAD_LOCAL, (uint8_t)(c->localCount -1), line);
            emitByte(c, OP_LOAD_INDEX, line);
            
            // Assign to user variable
            // Create inner scope for loop variable
            c->scopeDepth++;
            Token iterator = node->foreachStmt.iterator;
            char* iterName = strndup(iterator.start, iterator.length);
            addLocal(c, iterName); // New slot
            c->locals[c->localCount - 1].depth = c->scopeDepth; // Mark initialized!
            
            emitBytes(c, OP_STORE_LOCAL, (uint8_t)(c->localCount - 1), line);
            // NO POP! Keep it on stack to protect slot.

            // 7. Body
            compileStatement(c, node->foreachStmt.body);

            // 8. Increment .idx
            int idxLocal = resolveLocal(c, ".idx", 4);
            if (idxLocal == -1) { printf("Compiler Error: .idx missing\n"); exit(1); }
            
            emitBytes(c, OP_LOAD_LOCAL, (uint8_t)idxLocal, line);
            int oneIdx = addConstant(c->chunk, INT_VAL(1));
            emitBytes(c, OP_LOAD_CONST, (uint8_t)oneIdx, line);
            emitByte(c, OP_ADD, line);
            emitBytes(c, OP_STORE_LOCAL, (uint8_t)idxLocal, line);
            emitByte(c, OP_POP, line);
            
            // Loop back
            int offset = c->chunk->codeSize - loopStart + 3;
            emitByte(c, OP_LOOP, line);
            emitByte(c, (offset >> 8) & 0xff, line);
            emitByte(c, offset & 0xff, line);
            
            // Patch exit
            patchJump(c->chunk, exitJump);
            emitByte(c, OP_POP, line);

            // Pop .col and .idx
            c->scopeDepth--; // Exit foreach scope
            emitByte(c, OP_POP, line); // .idx
            emitByte(c, OP_POP, line); // .col
            c->localCount -= 2;

            break; 
        }

        case NODE_STMT_FOR: {
            // New scope for loop variable
            c->scopeDepth++;
            
            // Initializer
            if (node->forStmt.initializer) {
                if (node->forStmt.initializer->type == NODE_STMT_VAR_DECL) {
                    compileStatement(c, node->forStmt.initializer);
                } else {
                    compileExpression(c, node->forStmt.initializer);
                    emitByte(c, OP_POP, line); // Expression stmt pops
                }
            }
            
            int loopStart = c->chunk->codeSize;
            emitByte(c, OP_LOOP_HEADER, line);
            
            // Condition
            int exitJump = -1;
            if (node->forStmt.condition) {
                compileExpression(c, node->forStmt.condition);
                exitJump = emitJump(c, OP_JUMP_IF_FALSE, line);
                emitByte(c, OP_POP, line); // Pop condition result
            }
            
            // Body
            compileStatement(c, node->forStmt.body);
            
            // Increment
            if (node->forStmt.increment) {
                compileExpression(c, node->forStmt.increment);
                emitByte(c, OP_POP, line);
            }
            
            // Loop back
            int offset = c->chunk->codeSize - loopStart + 3;
            emitByte(c, OP_LOOP, line);
            emitByte(c, (offset >> 8) & 0xff, line);
            emitByte(c, offset & 0xff, line);
            
            // Patch exit
            if (exitJump != -1) {
                patchJump(c->chunk, exitJump);
                emitByte(c, OP_POP, line); // Pop condition result (if jumped)
            }
            
            // End scope
            c->scopeDepth--;
            while (c->localCount > 0 &&
                   c->locals[c->localCount - 1].depth > c->scopeDepth) {
                emitByte(c, OP_POP, line);
                c->localCount--;
            }
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
            func->modulePath = c->modulePath ? strdup(c->modulePath) : NULL;
            
            // Create independent chunk for function
            func->bytecodeChunk = malloc(sizeof(BytecodeChunk));
            initChunk(func->bytecodeChunk);
            
            // Critical GC Fix: Root the function object on VM stack
            // so it doesn't get collected during compilation of its body.
            // (GC traces stack -> func -> bytecodeChunk -> constants)
            if (c->vm->stackTop < STACK_MAX) {
                c->vm->stack[c->vm->stackTop++] = OBJ_VAL(func);
            } else {
                fprintf(stderr, "Stack overflow during compilation\n");
                exit(1);
            }
            
            // Compile function body in new context
            Compiler funcCompiler;
            initCompiler(&funcCompiler, c->vm, func->bytecodeChunk, c->modulePath);
            
            // Add parameters as locals (slot 0..N)
            for (int i = 0; i < func->paramCount; i++) {
                Token p = func->params[i];
                char* pname = strndup(p.start, p.length);
                addLocal(&funcCompiler, pname);
            }
            
            // Compile body
            compileNode(&funcCompiler, node->function.body);

#ifdef DEBUG_PRINT_CODE
    if (!funcCompiler.hadError) {
        disassembleChunk(func->bytecodeChunk, func->name.start ? func->name.start : "script");
    }
#endif
            
            // Ensure return at end
            emitByte(&funcCompiler, OP_LOAD_NIL, line);
            emitByte(&funcCompiler, OP_RETURN, line);
            
            // Pop function from stack
            c->vm->stackTop--;
            
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

        case NODE_STMT_STRUCT_DECL: {
            // struct Name { f1; f2; ... }
            Token name = node->structDecl.name;
            
            // Push name
            char* nameStr = strndup(name.start, name.length);
            ObjString* nameObj = internString(c->vm, nameStr, name.length);
            free(nameStr);
            int nameIdx = addConstant(c->chunk, OBJ_VAL(nameObj));
            emitBytes(c, OP_LOAD_CONST, (uint8_t)nameIdx, line);
            
            // Push fields
            int fieldCount = node->structDecl.fieldCount;
            for (int i = 0; i < fieldCount; i++) {
                Token ft = node->structDecl.fields[i];
                char* fStr = strndup(ft.start, ft.length);
                ObjString* fObj = internString(c->vm, fStr, ft.length);
                free(fStr);
                int fIdx = addConstant(c->chunk, OBJ_VAL(fObj));
                emitBytes(c, OP_LOAD_CONST, (uint8_t)fIdx, line);
            }
            
            // Emit struct definition opcode
            emitByte(c, OP_STRUCT_DEF, line);
            emitByte(c, (uint8_t)fieldCount, line);
            
            // Define variable
            if (c->scopeDepth == 0) {
                emitBytes(c, OP_DEFINE_GLOBAL, (uint8_t)nameIdx, line);
            } else {
                int slot = addLocal(c, strndup(name.start, name.length));
                emitBytes(c, OP_STORE_LOCAL, (uint8_t)slot, line);
            }
            break;
        }
        
        case NODE_STMT_PROP_ASSIGN: {
             compileExpression(c, node->propAssign.object);
             compileExpression(c, node->propAssign.value);
             
             Token name = node->propAssign.name;
             char* propName = strndup(name.start, name.length);
             ObjString* nameStr = internString(c->vm, propName, name.length);
             free(propName);
             int constIdx = addConstant(c->chunk, OBJ_VAL(nameStr));
             
             emitBytes(c, OP_STORE_PROPERTY, (uint8_t)constIdx, line);
             emitByte(c, OP_POP, line); 
             break;
        }

        case NODE_STMT_IMPORT: {
             // import name as alias;
             Token name = node->importStmt.module; // FIXED: .module not .name
             Token alias = node->importStmt.alias; // "math"
             
             // 1. Emit OP_IMPORT <name_string_index>
             char* modName = NULL;
             if (name.type == TOKEN_STRING) {
                 modName = strndup(name.start + 1, name.length - 2); // Strip quotes
             } else {
                 modName = strndup(name.start, name.length);
             }
             ObjString* modStr = internString(c->vm, modName, strlen(modName));
             free(modName);
             int constIdx = addConstant(c->chunk, OBJ_VAL(modStr));
             emitBytes(c, OP_IMPORT, (uint8_t)constIdx, line);
             
             // 2. Store result in variable 'alias'
             if (c->scopeDepth == 0) {
                 char* aliasName = strndup(alias.start, alias.length);
                 ObjString* aliasStr = internString(c->vm, aliasName, alias.length);
                 free(aliasName);
                 int aliasIdx = addConstant(c->chunk, OBJ_VAL(aliasStr));
                 emitBytes(c, OP_DEFINE_GLOBAL, (uint8_t)aliasIdx, line);
             } else {
                 int slot = addLocal(c, strndup(alias.start, alias.length));
                 emitBytes(c, OP_STORE_LOCAL, (uint8_t)slot, line);
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

bool compileToBytecode(VM* vm, Node* ast, BytecodeChunk* chunk, const char* modulePath) {
    Compiler compiler;
    initCompiler(&compiler, vm, chunk, modulePath);
    
    // Compile AST
    if (ast) {} // printf("DEBUG: compileToBytecode AST Type: %d\n", ast->type);
    
    if (ast && ast->type == NODE_STMT_BLOCK) {
        // printf("DEBUG: compileToBytecode BLOCK count=%d\n", ast->block.count);
        for (int i = 0; i < ast->block.count; i++) {
            Node* s = ast->block.statements[i];
            // printf("DEBUG: Compiling statement %d type=%d\n", i, s->type);
            compileNode(&compiler, s);
        }
    } else {
        // printf("DEBUG: Compiling single node type=%d\n", ast ? ast->type : -1);
        compileNode(&compiler, ast);
    }
    
    emitByte(&compiler, OP_RETURN_NIL, 0); // Implicit return
    
    #ifdef DEBUG_PRINT_CODE
    if (!compiler.hadError) {
        printf("== Bytecode ==\n");
        disassembleChunk(chunk, "script");
    }
    #endif
    
    return !compiler.hadError;
}
