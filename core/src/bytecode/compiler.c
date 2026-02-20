#include "bytecode/compiler.h"
#include "bytecode/opcodes.h"
#include "lexer.h"

// #define DEBUG_PRINT_CODE

#ifdef DEBUG_PRINT_CODE
#include "bytecode/chunk.h"
#endif

/**
 * AST → Register-Based Bytecode Compiler
 *
 * Each expression compiles into a destination register.
 * Local variables occupy dedicated registers.
 * No push/pop stack operations needed.
 */

// Helper to process string escapes
static char* parseStringLiteral(const char* start, int length) {
    char* buffer = malloc(length + 1);
    char* dest = buffer;
    const char* src = start;
    const char* end = start + length;

    while (src < end) {
        if (*src == '\\' && src + 1 < end) {
            src++;
            switch (*src) {
                case 'n': *dest++ = '\n'; break;
                case 't': *dest++ = '\t'; break;
                case 'r': *dest++ = '\r'; break;
                case '"': *dest++ = '"'; break;
                case '\'': *dest++ = '\''; break;
                case '\\': *dest++ = '\\'; break;
                default:
                    *dest++ = '\\';
                    *dest++ = *src;
            }
            src++;
        } else {
            *dest++ = *src++;
        }
    }
    *dest = '\0';
    return buffer;
}

typedef struct {
    VM* vm;
    BytecodeChunk* chunk;

    // Register-based local variable tracking
    struct {
        const char* name;
        int depth;
        int reg;        // Register index for this variable
    } locals[256];
    int localCount;

    int nextReg;        // Next free register index
    int scopeDepth;
    const char* modulePath;

    bool hadError;
} Compiler;

static void initCompiler(Compiler* c, VM* vm, BytecodeChunk* chunk, const char* modulePath) {
    c->vm = vm;
    c->chunk = chunk;
    c->modulePath = modulePath;
    c->hadError = false;
    c->scopeDepth = 0;

    // Reserve register 0 for the function/script itself
    c->localCount = 1;
    c->locals[0].depth = 0;
    c->locals[0].name = "";
    c->locals[0].reg = 0;
    c->nextReg = 1;
}

// Allocate a temporary register
static int allocReg(Compiler* c) {
    if (c->nextReg >= FRAME_REG_MAX) {
        fprintf(stderr, "Register overflow (%d)\n", c->nextReg);
        c->hadError = true;
        return 0;
    }
    int r = c->nextReg++;
    if (r > c->chunk->maxRegs) c->chunk->maxRegs = r;
    return r;
}

// Free temporary registers back to 'target' (used after sub-expressions)
static void freeRegsTo(Compiler* c, int target) {
    if (target < c->nextReg) c->nextReg = target;
}

// Emit a 32-bit instruction
static void emit(Compiler* c, uint32_t inst, int line) {
    writeChunk(c->chunk, inst, line);
}

// Emit jump placeholder, return instruction index for patching
static int emitJumpPlaceholder(Compiler* c, uint8_t op, uint8_t a, int line) {
    int idx = c->chunk->codeSize;
    if (op == OP_JMP || op == OP_LOOP) {
        emit(c, ENCODE_sBx(op, 0), line);
    } else {
        emit(c, ENCODE_AsBx(op, a, 0), line);
    }
    return idx;
}

// Resolve local variable -> register
static int resolveLocal(Compiler* c, const char* name, int length) {
    for (int i = c->localCount - 1; i >= 0; i--) {
        if (strlen(c->locals[i].name) == (size_t)length &&
            memcmp(c->locals[i].name, name, length) == 0) {
            return c->locals[i].reg;
        }
    }
    return -1; // Not found, assume global
}

// Add local variable in the current register
static int addLocal(Compiler* c, const char* name) {
    if (c->localCount >= 256) {
        fprintf(stderr, "Too many local variables\n");
        c->hadError = true;
        return 0;
    }
    int reg = allocReg(c);
    c->locals[c->localCount].name = name;
    c->locals[c->localCount].depth = c->scopeDepth;
    c->locals[c->localCount].reg = reg;
    c->localCount++;
    return reg;
}

// Add constant and return its index
static int emitConstant(Compiler* c, Value value) {
    return addConstant(c->chunk, value);
}

// Intern a name token and add to constant pool
static int internNameConst(Compiler* c, Token name) {
    char* str = strndup(name.start, name.length);
    ObjString* obj = internString(c->vm, str, name.length);
    free(str);
    return addConstant(c->chunk, OBJ_VAL(obj));
}

// Forward declarations
static void compileNode(Compiler* c, Node* node);
static void compileExpr(Compiler* c, Node* node, int dest);
static void compileStmt(Compiler* c, Node* node);

/**
 * Compile expression, result goes into register 'dest'
 */
static void compileExpr(Compiler* c, Node* node, int dest) {
    if (!node) return;
    int line = node->literal.token.line > 0 ? node->literal.token.line : 1;

    switch (node->type) {
        case NODE_EXPR_LITERAL: {
            Token tok = node->literal.token;
            line = tok.line > 0 ? tok.line : 1;
            if (tok.type == TOKEN_NUMBER) {
                char* str = strndup(tok.start, tok.length);
                if (strchr(str, '.')) {
                    double val = atof(str);
                    int ki = emitConstant(c, FLOAT_VAL(val));
                    emit(c, ENCODE_ABx(OP_LOADK, dest, ki), line);
                } else {
                    int64_t val = atoll(str);
                    // Use LOADI for small integers that fit in signed 16-bit
                    if (val >= -32767 && val <= 32767) {
                        emit(c, ENCODE_ABx(OP_LOADI, dest, (uint16_t)(val + 0x7FFF)), line);
                    } else {
                        int ki = emitConstant(c, INT_VAL(val));
                        emit(c, ENCODE_ABx(OP_LOADK, dest, ki), line);
                    }
                }
                free(str);
            } else if (tok.type == TOKEN_TRUE) {
                emit(c, ENCODE_A(OP_LOADTRUE, dest), line);
            } else if (tok.type == TOKEN_FALSE) {
                emit(c, ENCODE_A(OP_LOADFALSE, dest), line);
            } else if (tok.type == TOKEN_NIL) {
                emit(c, ENCODE_A(OP_LOADNIL, dest), line);
            } else if (tok.type == TOKEN_STRING) {
                char* str = parseStringLiteral(tok.start + 1, tok.length - 2);
                ObjString* objStr = internString(c->vm, str, strlen(str));
                free(str);
                int ki = emitConstant(c, OBJ_VAL(objStr));
                emit(c, ENCODE_ABx(OP_LOADK, dest, ki), line);
            }
            break;
        }

        case NODE_EXPR_VAR: {
            Token name = node->var.name;
            line = name.line > 0 ? name.line : 1;
            int local = resolveLocal(c, name.start, name.length);
            if (local != -1) {
                if (local != dest) {
                    emit(c, ENCODE_ABC(OP_MOVE, dest, local, 0), line);
                }
            } else {
                int ki = internNameConst(c, name);
                emit(c, ENCODE_ABx(OP_GETGLOBAL, dest, ki), line);
            }
            break;
        }

        case NODE_EXPR_BINARY: {
            int regB = allocReg(c);
            int regC = allocReg(c);
            compileExpr(c, node->binary.left, regB);
            compileExpr(c, node->binary.right, regC);

            switch (node->binary.op.type) {
                case TOKEN_PLUS:          emit(c, ENCODE_ABC(OP_ADD, dest, regB, regC), line); break;
                case TOKEN_MINUS:         emit(c, ENCODE_ABC(OP_SUB, dest, regB, regC), line); break;
                case TOKEN_STAR:          emit(c, ENCODE_ABC(OP_MUL, dest, regB, regC), line); break;
                case TOKEN_SLASH:         emit(c, ENCODE_ABC(OP_DIV, dest, regB, regC), line); break;
                case TOKEN_PERCENT:       emit(c, ENCODE_ABC(OP_MOD, dest, regB, regC), line); break;
                case TOKEN_LESS:          emit(c, ENCODE_ABC(OP_LT,  dest, regB, regC), line); break;
                case TOKEN_LESS_EQUAL:    emit(c, ENCODE_ABC(OP_LE,  dest, regB, regC), line); break;
                case TOKEN_GREATER:       emit(c, ENCODE_ABC(OP_GT,  dest, regB, regC), line); break;
                case TOKEN_GREATER_EQUAL: emit(c, ENCODE_ABC(OP_GE,  dest, regB, regC), line); break;
                case TOKEN_EQUAL_EQUAL:   emit(c, ENCODE_ABC(OP_EQ,  dest, regB, regC), line); break;
                case TOKEN_BANG_EQUAL:    emit(c, ENCODE_ABC(OP_NE,  dest, regB, regC), line); break;
                default: break;
            }
            freeRegsTo(c, regB);
            break;
        }

        case NODE_EXPR_UNARY: {
            int regB = allocReg(c);
            compileExpr(c, node->unary.expr, regB);
            if (node->unary.op.type == TOKEN_MINUS) {
                emit(c, ENCODE_ABC(OP_NEG, dest, regB, 0), line);
            } else if (node->unary.op.type == TOKEN_BANG) {
                emit(c, ENCODE_ABC(OP_NOT, dest, regB, 0), line);
            }
            freeRegsTo(c, regB);
            break;
        }

        case NODE_EXPR_AWAIT: {
            int regB = allocReg(c);
            compileExpr(c, node->unary.expr, regB);
            emit(c, ENCODE_ABC(OP_AWAIT, dest, regB, 0), line);
            freeRegsTo(c, regB);
            break;
        }

        case NODE_EXPR_CALL: {
            // Check builtins
            if (node->call.callee->type == NODE_EXPR_VAR) {
                Token name = node->call.callee->var.name;
                char* funcName = strndup(name.start, name.length);

                if (strcmp(funcName, "push") == 0) {
                    Node* arg = node->call.arguments;
                    if (arg && arg->next) {
                        int regArr = allocReg(c);
                        int regVal = allocReg(c);
                        compileExpr(c, arg, regArr);
                        compileExpr(c, arg->next, regVal);
                        emit(c, ENCODE_ABC(OP_PUSH, regArr, regVal, 0), line);
                        emit(c, ENCODE_A(OP_LOADNIL, dest), line);
                        freeRegsTo(c, regArr);
                    }
                    free(funcName);
                    break;
                } else if (strcmp(funcName, "pop") == 0) {
                    Node* arg = node->call.arguments;
                    if (arg) {
                        int regArr = allocReg(c);
                        compileExpr(c, arg, regArr);
                        emit(c, ENCODE_ABC(OP_POP, dest, regArr, 0), line);
                        freeRegsTo(c, regArr);
                    }
                    free(funcName);
                    break;
                } else if (strcmp(funcName, "length") == 0) {
                    Node* arg = node->call.arguments;
                    if (arg) {
                        int regArr = allocReg(c);
                        compileExpr(c, arg, regArr);
                        emit(c, ENCODE_ABC(OP_LEN, dest, regArr, 0), line);
                        freeRegsTo(c, regArr);
                    }
                    free(funcName);
                    break;
                } else if (strcmp(funcName, "array") == 0) {
                    emit(c, ENCODE_ABx(OP_NEWARRAY, dest, 0), line);
                    free(funcName);
                    break;
                } else if (strcmp(funcName, "map") == 0) {
                    emit(c, ENCODE_A(OP_NEWMAP, dest), line);
                    free(funcName);
                    break;
                }
                free(funcName);
            }

            // Regular function call: R(dest) = call R(funcReg) with args
            // Layout: funcReg, arg0, arg1, ..., argN (contiguous)
            int funcReg = allocReg(c);
            compileExpr(c, node->call.callee, funcReg);

            int argCount = 0;
            Node* arg = node->call.arguments;
            while (arg) {
                int argReg = allocReg(c);
                compileExpr(c, arg, argReg);
                argCount++;
                arg = arg->next;
            }

            // OP_CALL A=funcReg B=argCount C=resultCount
            // Result goes into funcReg, then MOVE to dest
            emit(c, ENCODE_ABC(OP_CALL, funcReg, argCount, 1), line);
            if (funcReg != dest) {
                emit(c, ENCODE_ABC(OP_MOVE, dest, funcReg, 0), line);
            }
            freeRegsTo(c, funcReg);
            break;
        }

        case NODE_EXPR_INDEX: {
            int regB = allocReg(c);
            int regC = allocReg(c);
            compileExpr(c, node->index.target, regB);
            compileExpr(c, node->index.index, regC);
            emit(c, ENCODE_ABC(OP_GETIDX, dest, regB, regC), line);
            freeRegsTo(c, regB);
            break;
        }

        case NODE_EXPR_ARRAY_LITERAL: {
            // Count elements
            int count = 0;
            Node* el = node->arrayLiteral.elements;
            while (el) { count++; el = el->next; }

            // Allocate contiguous regs for elements
            int baseReg = c->nextReg;
            el = node->arrayLiteral.elements;
            while (el) {
                int r = allocReg(c);
                compileExpr(c, el, r);
                el = el->next;
            }

            // Put array into dest, init with baseReg..baseReg+count-1
            // OP_NEWARRAY A=dest Bx=count; elements in R(dest+1)..R(dest+count)
            // Actually, simpler: create empty array then push
            emit(c, ENCODE_ABx(OP_NEWARRAY, dest, 0), line);
            for (int i = 0; i < count; i++) {
                emit(c, ENCODE_ABC(OP_PUSH, dest, baseReg + i, 0), line);
            }
            freeRegsTo(c, baseReg);
            break;
        }

        case NODE_EXPR_GET: {
            int regB = allocReg(c);
            compileExpr(c, node->get.object, regB);
            int ki = internNameConst(c, node->get.name);
            emit(c, ENCODE_ABC(OP_GETPROP, dest, regB, ki), line);
            freeRegsTo(c, regB);
            break;
        }

        case NODE_STMT_ASSIGN: {
            // Assignment used as expression
            Token name = node->assign.name;
            int local = resolveLocal(c, name.start, name.length);
            if (local != -1) {
                compileExpr(c, node->assign.value, local);
                if (local != dest) {
                    emit(c, ENCODE_ABC(OP_MOVE, dest, local, 0), line);
                }
            } else {
                compileExpr(c, node->assign.value, dest);
                int ki = internNameConst(c, name);
                emit(c, ENCODE_ABx(OP_SETGLOBAL, dest, ki), line);
            }
            break;
        }

        default:
            break;
    }
}

/**
 * Compile statement (no result register needed)
 */
static void compileStmt(Compiler* c, Node* node) {
    if (!node) return;
    int line = 1;

    switch (node->type) {
        case NODE_STMT_PRINT: {
            int reg = allocReg(c);
            compileExpr(c, node->print.expr, reg);
            emit(c, ENCODE_A(OP_PRINT, reg), line);
            freeRegsTo(c, reg);
            break;
        }

        case NODE_STMT_VAR_DECL: {
            Token name = node->varDecl.name;
            line = name.line > 0 ? name.line : 1;

            if (c->scopeDepth > 0) {
                // Local variable -> allocate register
                int reg = addLocal(c, strndup(name.start, name.length));
                if (node->varDecl.initializer) {
                    compileExpr(c, node->varDecl.initializer, reg);
                } else {
                    emit(c, ENCODE_A(OP_LOADNIL, reg), line);
                }
            } else {
                // Global variable
                int reg = allocReg(c);
                if (node->varDecl.initializer) {
                    compileExpr(c, node->varDecl.initializer, reg);
                } else {
                    emit(c, ENCODE_A(OP_LOADNIL, reg), line);
                }
                int ki = internNameConst(c, name);
                emit(c, ENCODE_ABx(OP_DEFGLOBAL, reg, ki), line);
                freeRegsTo(c, reg);
            }
            break;
        }

        case NODE_STMT_ASSIGN: {
            Token name = node->assign.name;
            int local = resolveLocal(c, name.start, name.length);

            // Pattern: i = i + 1 / i = i - 1 -> ADD/SUB in place
            if (local != -1 && node->assign.value && node->assign.value->type == NODE_EXPR_BINARY) {
                Node* bin = node->assign.value;
                if (bin->binary.left && bin->binary.left->type == NODE_EXPR_VAR) {
                    Token leftName = bin->binary.left->var.name;
                    if (leftName.length == name.length &&
                        memcmp(leftName.start, name.start, name.length) == 0) {
                        // i = i OP expr
                        int regC = allocReg(c);
                        compileExpr(c, bin->binary.right, regC);
                        switch (bin->binary.op.type) {
                            case TOKEN_PLUS:  emit(c, ENCODE_ABC(OP_ADD, local, local, regC), line); break;
                            case TOKEN_MINUS: emit(c, ENCODE_ABC(OP_SUB, local, local, regC), line); break;
                            case TOKEN_STAR:  emit(c, ENCODE_ABC(OP_MUL, local, local, regC), line); break;
                            case TOKEN_SLASH: emit(c, ENCODE_ABC(OP_DIV, local, local, regC), line); break;
                            default: goto fallback_assign;
                        }
                        freeRegsTo(c, regC);
                        break;
                    }
                }
            }

            fallback_assign:
            if (local != -1) {
                compileExpr(c, node->assign.value, local);
            } else {
                int reg = allocReg(c);
                compileExpr(c, node->assign.value, reg);
                int ki = internNameConst(c, name);
                emit(c, ENCODE_ABx(OP_SETGLOBAL, reg, ki), line);
                freeRegsTo(c, reg);
            }
            break;
        }

        case NODE_STMT_INDEX_ASSIGN: {
            int regA = allocReg(c);
            int regB = allocReg(c);
            int regC = allocReg(c);
            compileExpr(c, node->indexAssign.target, regA);
            compileExpr(c, node->indexAssign.index, regB);
            compileExpr(c, node->indexAssign.value, regC);
            emit(c, ENCODE_ABC(OP_SETIDX, regA, regB, regC), line);
            freeRegsTo(c, regA);
            break;
        }

        case NODE_STMT_IF: {
            int condReg = allocReg(c);
            compileExpr(c, node->ifStmt.condition, condReg);

            int elseJmp = emitJumpPlaceholder(c, OP_JMPF, condReg, line);
            freeRegsTo(c, condReg);

            compileStmt(c, node->ifStmt.thenBranch);

            if (node->ifStmt.elseBranch) {
                int endJmp = emitJumpPlaceholder(c, OP_JMP, 0, line);
                patchJump(c->chunk, elseJmp);
                compileStmt(c, node->ifStmt.elseBranch);
                patchJump(c->chunk, endJmp);
            } else {
                patchJump(c->chunk, elseJmp);
            }
            break;
        }

        case NODE_STMT_WHILE: {
            int loopStart = c->chunk->codeSize;

            int condReg = allocReg(c);
            compileExpr(c, node->whileStmt.condition, condReg);
            int exitJmp = emitJumpPlaceholder(c, OP_JMPF, condReg, line);
            freeRegsTo(c, condReg);

            compileStmt(c, node->whileStmt.body);

            // Loop back
            int backOffset = c->chunk->codeSize - loopStart + 1;
            emit(c, ENCODE_sBx(OP_LOOP, backOffset), line);

            patchJump(c->chunk, exitJmp);
            break;
        }

        case NODE_STMT_FOR: {
            c->scopeDepth++;
            int savedLocalCount = c->localCount;
            int savedNextReg = c->nextReg;

            if (node->forStmt.initializer) {
                if (node->forStmt.initializer->type == NODE_STMT_VAR_DECL) {
                    compileStmt(c, node->forStmt.initializer);
                } else {
                    int tmp = allocReg(c);
                    compileExpr(c, node->forStmt.initializer, tmp);
                    freeRegsTo(c, tmp);
                }
            }

            int loopStart = c->chunk->codeSize;
            int exitJmp = -1;

            if (node->forStmt.condition) {
                int condReg = allocReg(c);
                compileExpr(c, node->forStmt.condition, condReg);
                exitJmp = emitJumpPlaceholder(c, OP_JMPF, condReg, line);
                freeRegsTo(c, condReg);
            }

            compileStmt(c, node->forStmt.body);

            if (node->forStmt.increment) {
                if (node->forStmt.increment->type >= NODE_STMT_VAR_DECL &&
                    node->forStmt.increment->type <= NODE_STMT_PROP_ASSIGN) {
                    compileStmt(c, node->forStmt.increment);
                } else {
                    int tmp = allocReg(c);
                    compileExpr(c, node->forStmt.increment, tmp);
                    freeRegsTo(c, tmp);
                }
            }

            int backOffset = c->chunk->codeSize - loopStart + 1;
            emit(c, ENCODE_sBx(OP_LOOP, backOffset), line);

            if (exitJmp != -1) patchJump(c->chunk, exitJmp);

            c->scopeDepth--;
            c->localCount = savedLocalCount;
            c->nextReg = savedNextReg;
            break;
        }

        case NODE_STMT_FOREACH: {
            c->scopeDepth++;
            int savedLocalCount = c->localCount;
            int savedNextReg = c->nextReg;

            // Collection register
            int colReg = addLocal(c, strdup(".col"));
            compileExpr(c, node->foreachStmt.collection, colReg);

            // Index register = 0
            int idxReg = addLocal(c, strdup(".idx"));
            emit(c, ENCODE_ABx(OP_LOADI, idxReg, 0 + 0x7FFF), line); // 0

            // Loop start
            int loopStart = c->chunk->codeSize;

            // Condition: idx < len(col)
            int lenReg = allocReg(c);
            int condReg = allocReg(c);
            emit(c, ENCODE_ABC(OP_LEN, lenReg, colReg, 0), line);
            emit(c, ENCODE_ABC(OP_LT, condReg, idxReg, lenReg), line);
            int exitJmp = emitJumpPlaceholder(c, OP_JMPF, condReg, line);
            freeRegsTo(c, lenReg);

            // Iterator variable = col[idx]
            c->scopeDepth++;
            Token iterator = node->foreachStmt.iterator;
            int iterReg = addLocal(c, strndup(iterator.start, iterator.length));
            emit(c, ENCODE_ABC(OP_GETIDX, iterReg, colReg, idxReg), line);

            // Body
            compileStmt(c, node->foreachStmt.body);

            // Increment idx
            int oneReg = allocReg(c);
            emit(c, ENCODE_ABx(OP_LOADI, oneReg, 1 + 0x7FFF), line); // 1
            emit(c, ENCODE_ABC(OP_ADD, idxReg, idxReg, oneReg), line);
            freeRegsTo(c, oneReg);

            // Loop back
            int backOffset = c->chunk->codeSize - loopStart + 1;
            emit(c, ENCODE_sBx(OP_LOOP, backOffset), line);

            patchJump(c->chunk, exitJmp);

            c->scopeDepth--;
            c->scopeDepth--;
            c->localCount = savedLocalCount;
            c->nextReg = savedNextReg;
            break;
        }

        case NODE_STMT_BLOCK: {
            c->scopeDepth++;
            int savedLocalCount = c->localCount;
            int savedNextReg = c->nextReg;

            for (int i = 0; i < node->block.count; i++) {
                compileNode(c, node->block.statements[i]);
            }

            c->scopeDepth--;
            c->localCount = savedLocalCount;
            c->nextReg = savedNextReg;
            break;
        }

        case NODE_STMT_FUNCTION: {
            // Allocate Function object
            Function* func = malloc(sizeof(Function));
            func->obj.type = OBJ_FUNCTION;
            func->obj.isMarked = false;
            func->obj.isPermanent = false;
            func->obj.generation = 0;
            func->obj.next = c->vm->objects;
            c->vm->objects = (Obj*)func;

            func->name = node->function.name;
            func->params = node->function.params;
            func->paramCount = node->function.paramCount;
            func->isNative = false;
            func->isAsync = false;
            func->modulePath = c->modulePath ? strdup(c->modulePath) : NULL;
            func->moduleEnv = c->vm->globalEnv;
            func->closure = NULL;
            func->native = NULL;

            // Compile function body into new chunk
            func->bytecodeChunk = malloc(sizeof(BytecodeChunk));
            initChunk(func->bytecodeChunk);

            // Root function for GC safety
            if (c->vm->stackTop < 8192) {
                c->vm->stack[c->vm->stackTop++] = OBJ_VAL(func);
            }

            Compiler funcCompiler;
            initCompiler(&funcCompiler, c->vm, func->bytecodeChunk, c->modulePath);

            // Parameters occupy registers 1..paramCount
            for (int i = 0; i < func->paramCount; i++) {
                Token p = func->params[i];
                char* pname = strndup(p.start, p.length);
                addLocal(&funcCompiler, pname);
            }

            // Compile body
            compileNode(&funcCompiler, node->function.body);

            // Implicit return nil
            emit(&funcCompiler, ENCODE_A(OP_RETURNNIL, 0), line);

#ifdef DEBUG_PRINT_CODE
            if (!funcCompiler.hadError) {
                char fname[256];
                snprintf(fname, sizeof(fname), "%.*s", func->name.length, func->name.start);
                disassembleChunk(func->bytecodeChunk, fname);
            }
#endif

            // Unroot
            c->vm->stackTop--;

            // Store function in parent scope
            int funcConstIdx = emitConstant(c, OBJ_VAL(func));

            if (c->scopeDepth == 0) {
                // Global function
                int nameIdx = internNameConst(c, node->function.name);
                int reg = allocReg(c);
                emit(c, ENCODE_ABx(OP_LOADK, reg, funcConstIdx), line);
                emit(c, ENCODE_ABx(OP_DEFGLOBAL, reg, nameIdx), line);
                freeRegsTo(c, reg);
            } else {
                // Local function
                int reg = addLocal(c, strndup(node->function.name.start, node->function.name.length));
                emit(c, ENCODE_ABx(OP_LOADK, reg, funcConstIdx), line);
            }
            break;
        }

        case NODE_STMT_RETURN: {
            if (node->returnStmt.value) {
                int reg = allocReg(c);
                compileExpr(c, node->returnStmt.value, reg);
                emit(c, ENCODE_A(OP_RETURN, reg), line);
                freeRegsTo(c, reg);
            } else {
                emit(c, ENCODE_A(OP_RETURNNIL, 0), line);
            }
            break;
        }

        case NODE_STMT_STRUCT_DECL: {
            Token name = node->structDecl.name;
            int nameIdx = internNameConst(c, name);

            // Push struct name into a register
            int nameReg = allocReg(c);
            emit(c, ENCODE_ABx(OP_LOADK, nameReg, nameIdx), line);

            // Field names into constant pool
            int fieldCount = node->structDecl.fieldCount;
            for (int i = 0; i < fieldCount; i++) {
                Token ft = node->structDecl.fields[i];
                int fIdx = internNameConst(c, ft);
                int fReg = allocReg(c);
                emit(c, ENCODE_ABx(OP_LOADK, fReg, fIdx), line);
            }

            // OP_STRUCTDEF A=fieldCount Bx=nameIdx
            emit(c, ENCODE_ABx(OP_STRUCTDEF, fieldCount, nameIdx), line);

            // Define as global/local
            if (c->scopeDepth == 0) {
                emit(c, ENCODE_ABx(OP_DEFGLOBAL, nameReg, nameIdx), line);
            } else {
                int reg = addLocal(c, strndup(name.start, name.length));
                emit(c, ENCODE_ABC(OP_MOVE, reg, nameReg, 0), line);
            }
            freeRegsTo(c, nameReg);
            break;
        }

        case NODE_STMT_PROP_ASSIGN: {
            int regObj = allocReg(c);
            int regVal = allocReg(c);
            compileExpr(c, node->propAssign.object, regObj);
            compileExpr(c, node->propAssign.value, regVal);
            int ki = internNameConst(c, node->propAssign.name);
            emit(c, ENCODE_ABC(OP_SETPROP, regObj, ki, regVal), line);
            freeRegsTo(c, regObj);
            break;
        }

        case NODE_STMT_IMPORT: {
            Token name = node->importStmt.module;
            Token alias = node->importStmt.alias;

            char* modName = NULL;
            if (name.type == TOKEN_STRING) {
                modName = strndup(name.start + 1, name.length - 2);
            } else {
                modName = strndup(name.start, name.length);
            }
            ObjString* modStr = internString(c->vm, modName, strlen(modName));
            free(modName);
            int modIdx = emitConstant(c, OBJ_VAL(modStr));

            int reg = allocReg(c);
            emit(c, ENCODE_ABx(OP_IMPORT, reg, modIdx), line);

            if (c->scopeDepth == 0) {
                int aliasIdx = internNameConst(c, alias);
                emit(c, ENCODE_ABx(OP_DEFGLOBAL, reg, aliasIdx), line);
            } else {
                int localReg = addLocal(c, strndup(alias.start, alias.length));
                emit(c, ENCODE_ABC(OP_MOVE, localReg, reg, 0), line);
            }
            freeRegsTo(c, reg);
            break;
        }

        default:
            break;
    }
}

static void compileNode(Compiler* c, Node* node) {
    if (!node) return;

    if (node->type >= NODE_STMT_VAR_DECL && node->type <= NODE_STMT_PROP_ASSIGN) {
        compileStmt(c, node);
    } else {
        // Expression statement -> result is discarded
        int reg = allocReg(c);
        compileExpr(c, node, reg);
        freeRegsTo(c, reg);
    }

    if (node->next) {
        compileNode(c, node->next);
    }
}

bool compileToBytecode(VM* vm, Node* ast, BytecodeChunk* chunk, const char* modulePath) {
    Compiler compiler;
    initCompiler(&compiler, vm, chunk, modulePath);

    if (ast && ast->type == NODE_STMT_BLOCK) {
        for (int i = 0; i < ast->block.count; i++) {
            compileNode(&compiler, ast->block.statements[i]);
        }
    } else {
        compileNode(&compiler, ast);
    }

    // Implicit halt/return at end of script
    emit(&compiler, ENCODE_A(OP_RETURNNIL, 0), 0);

#ifdef DEBUG_PRINT_CODE
    if (!compiler.hadError) {
        printf("== Script Bytecode ==\n");
        disassembleChunk(chunk, "script");
    }
#endif

    return !compiler.hadError;
}
