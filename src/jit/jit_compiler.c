#include "jit/jit_compiler.h"
#include "jit/assembler.h"
#include "jit/memory.h"
#include "bytecode/opcodes.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/**
 * Baseline JIT Compiler Implementation
 * 
 * Template-based compilation with simple register allocation.
 * Compiles hot bytecode loops to native x86-64 code.
 */

// Register allocation for VM stack
// Stack top 4 values are kept in registers for fast access
#define STACK_REG_0 REG_RAX  // Top of stack
#define STACK_REG_1 REG_RBX  // Second from top
#define STACK_REG_2 REG_RCX  // Third from top
#define STACK_REG_3 REG_RDX  // Fourth from top

// Compilation context
typedef struct {
    Assembler as;
    BytecodeChunk* chunk;
    VM* vm;
    int stackDepth;
    int maxStackDepth;
    size_t* jumpTargets;  // Array of jump target positions
    int jumpTargetCount;
} CompileContext;

static void initCompileContext(CompileContext* ctx, VM* vm, BytecodeChunk* chunk) {
    initAssembler(&ctx->as, 4096);
    ctx->chunk = chunk;
    ctx->vm = vm;
    ctx->stackDepth = 0;
    ctx->maxStackDepth = 0;
    ctx->jumpTargets = NULL;
    ctx->jumpTargetCount = 0;
}

static void freeCompileContext(CompileContext* ctx) {
    freeAssembler(&ctx->as);
    if (ctx->jumpTargets) {
        free(ctx->jumpTargets);
    }
}

// Emit function prolog
static void emitProlog(CompileContext* ctx) {
    // Standard function prolog
    // PUSH RBP
    emit_push_reg(&ctx->as, REG_RBP);
    // MOV RBP, RSP
    emit_mov_reg_reg(&ctx->as, REG_RBP, REG_RSP);
    
    // Allocate stack space for locals (if needed)
    // For now, we'll use a fixed size
    int localSpace = 256;  // 256 bytes for local variables
    if (localSpace > 0) {
        // SUB RSP, localSpace
        emit_sub_reg_imm(&ctx->as, REG_RSP, localSpace);
    }
    
    // Save callee-saved registers
    emit_push_reg(&ctx->as, REG_RBX);
    emit_push_reg(&ctx->as, REG_R12);
    emit_push_reg(&ctx->as, REG_R13);
    emit_push_reg(&ctx->as, REG_R14);
    emit_push_reg(&ctx->as, REG_R15);
    
    // RDI contains VM pointer (first argument)
    // Save it to R15 for later use
    emit_mov_reg_reg(&ctx->as, REG_R15, REG_RDI);
}

// Emit function epilog
static void emitEpilog(CompileContext* ctx) {
    // Restore callee-saved registers
    emit_pop_reg(&ctx->as, REG_R15);
    emit_pop_reg(&ctx->as, REG_R14);
    emit_pop_reg(&ctx->as, REG_R13);
    emit_pop_reg(&ctx->as, REG_R12);
    emit_pop_reg(&ctx->as, REG_RBX);
    
    // Restore stack frame
    // MOV RSP, RBP
    emit_mov_reg_reg(&ctx->as, REG_RSP, REG_RBP);
    // POP RBP
    emit_pop_reg(&ctx->as, REG_RBP);
    
    // RET
    emit_ret(&ctx->as);
}

// Compile a single bytecode instruction
static bool compileInstruction(CompileContext* ctx, int* ip) {
    uint8_t* code = ctx->chunk->code;
    OpCode op = (OpCode)code[*ip];
    (*ip)++;
    
    // Debug: print opcode being compiled
    // static int debugCount = 0;
    // if (debugCount < 20) {
    //     fprintf(stderr, "JIT: Compiling opcode %d at ip=%d\n", op, *ip - 1);
    //     debugCount++;
    // }
    
    switch (op) {
        case OP_LOAD_IMM: {
            // Load 32-bit immediate value
            int32_t value = (int32_t)(
                (code[*ip] << 0) |
                (code[*ip + 1] << 8) |
                (code[*ip + 2] << 16) |
                (code[*ip + 3] << 24)
            );
            *ip += 4;
            
            // MOV RAX, value
            emit_mov_reg_imm32(&ctx->as, STACK_REG_0, value);
            ctx->stackDepth++;
            if (ctx->stackDepth > ctx->maxStackDepth) {
                ctx->maxStackDepth = ctx->stackDepth;
            }
            break;
        }
        
        case OP_LOAD_CONST: {
            // Load from constant pool
            uint8_t constIdx = code[*ip];
            (*ip)++;
            
            // For now, treat constants as immediates (simplified)
            // In full implementation, would load from chunk->constants
            emit_mov_reg_imm32(&ctx->as, STACK_REG_0, 0);
            ctx->stackDepth++;
            if (ctx->stackDepth > ctx->maxStackDepth) {
                ctx->maxStackDepth = ctx->stackDepth;
            }
            break;
        }
        
        case OP_LOAD_NIL: {
            // Load nil (0)
            emit_mov_reg_imm32(&ctx->as, STACK_REG_0, 0);
            ctx->stackDepth++;
            if (ctx->stackDepth > ctx->maxStackDepth) {
                ctx->maxStackDepth = ctx->stackDepth;
            }
            break;
        }
        
        case OP_LOAD_TRUE: {
            // Load true (1)
            emit_mov_reg_imm32(&ctx->as, STACK_REG_0, 1);
            ctx->stackDepth++;
            if (ctx->stackDepth > ctx->maxStackDepth) {
                ctx->maxStackDepth = ctx->stackDepth;
            }
            break;
        }
        
        case OP_LOAD_FALSE: {
            // Load false (0)
            emit_mov_reg_imm32(&ctx->as, STACK_REG_0, 0);
            ctx->stackDepth++;
            if (ctx->stackDepth > ctx->maxStackDepth) {
                ctx->maxStackDepth = ctx->stackDepth;
            }
            break;
        }
        
        case OP_POP: {
            // Pop and discard top of stack
            ctx->stackDepth--;
            break;
        }
        
        case OP_ADD_II: {
            // Integer addition: RAX = RAX + RBX
            emit_add_reg_reg(&ctx->as, STACK_REG_0, STACK_REG_1);
            ctx->stackDepth--;
            break;
        }
        
        case OP_SUB_II: {
            // Integer subtraction: RAX = RAX - RBX
            emit_sub_reg_reg(&ctx->as, STACK_REG_0, STACK_REG_1);
            ctx->stackDepth--;
            break;
        }
        
        case OP_MUL_II: {
            // Integer multiplication: RAX = RAX * RBX
            emit_imul_reg_reg(&ctx->as, STACK_REG_0, STACK_REG_1);
            ctx->stackDepth--;
            break;
        }
        
        case OP_DIV_II: {
            // Integer division: RAX = RAX / RBX
            // x86-64 IDIV uses RDX:RAX, result in RAX
            // For simplicity, assume no overflow
            emit_idiv_reg(&ctx->as, STACK_REG_1);
            ctx->stackDepth--;
            break;
        }
        
        case OP_MOD_II: {
            // Integer modulo: RAX = RAX % RBX
            // IDIV puts remainder in RDX
            emit_idiv_reg(&ctx->as, STACK_REG_1);
            // MOV RAX, RDX (move remainder to RAX)
            emit_mov_reg_reg(&ctx->as, REG_RAX, REG_RDX);
            ctx->stackDepth--;
            break;
        }
        
        case OP_ADD_FF:
        case OP_SUB_FF:
        case OP_MUL_FF:
        case OP_DIV_FF:
        case OP_NEG_F:
            // Float operations - not implemented yet, fallback to interpreter
            fprintf(stderr, "JIT: Float ops not implemented yet\n");
            return false;
        
        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
        case OP_DIV:
        case OP_MOD:
        case OP_NEG:
            // Generic arithmetic - simplified, treat as integer for now
            // In full implementation, would check types and dispatch
            // For now, just use integer ops
            switch (op) {
                case OP_ADD:
                    emit_add_reg_reg(&ctx->as, STACK_REG_0, STACK_REG_1);
                    ctx->stackDepth--;
                    break;
                case OP_SUB:
                    emit_sub_reg_reg(&ctx->as, STACK_REG_0, STACK_REG_1);
                    ctx->stackDepth--;
                    break;
                case OP_MUL:
                    emit_imul_reg_reg(&ctx->as, STACK_REG_0, STACK_REG_1);
                    ctx->stackDepth--;
                    break;
                case OP_DIV:
                    emit_idiv_reg(&ctx->as, STACK_REG_1);
                    ctx->stackDepth--;
                    break;
                case OP_MOD:
                    emit_idiv_reg(&ctx->as, STACK_REG_1);
                    emit_mov_reg_reg(&ctx->as, REG_RAX, REG_RDX);
                    ctx->stackDepth--;
                    break;
                case OP_NEG:
                    emit_neg_reg(&ctx->as, STACK_REG_0);
                    break;
                default:
                    break;
            }
            break;
        
        case OP_LOAD_GLOBAL:
        case OP_STORE_GLOBAL:
        case OP_DEFINE_GLOBAL:
            // Global variables - fallback to interpreter for now
            // Requires accessing VM's global environment which is complex
            fprintf(stderr, "JIT: Global variable ops not implemented - fallback to interpreter\n");
            return false;
        
        case OP_NEG_I: {
            // Negate integer: RAX = -RAX
            emit_neg_reg(&ctx->as, STACK_REG_0);
            break;
        }
        
        case OP_LOAD_LOCAL: {
            // Load local variable
            uint8_t index = code[*ip];
            (*ip)++;
            
            // MOV RAX, [RBP - offset]
            int32_t offset = -((int32_t)index * 8 + 8);
            emit_mov_reg_mem(&ctx->as, STACK_REG_0, REG_RBP, offset);
            ctx->stackDepth++;
            if (ctx->stackDepth > ctx->maxStackDepth) {
                ctx->maxStackDepth = ctx->stackDepth;
            }
            break;
        }
        
        case OP_STORE_LOCAL: {
            // Store local variable
            uint8_t index = code[*ip];
            (*ip)++;
            
            // MOV [RBP - offset], RAX
            int32_t offset = -((int32_t)index * 8 + 8);
            emit_mov_mem_reg(&ctx->as, REG_RBP, offset, STACK_REG_0);
            ctx->stackDepth--;
            break;
        }
        
        case OP_LOAD_LOCAL_0: {
            // Fast path for local 0
            emit_mov_reg_mem(&ctx->as, STACK_REG_0, REG_RBP, -8);
            ctx->stackDepth++;
            if (ctx->stackDepth > ctx->maxStackDepth) {
                ctx->maxStackDepth = ctx->stackDepth;
            }
            break;
        }
        
        case OP_LOAD_LOCAL_1: {
            // Fast path for local 1
            emit_mov_reg_mem(&ctx->as, STACK_REG_0, REG_RBP, -16);
            ctx->stackDepth++;
            if (ctx->stackDepth > ctx->maxStackDepth) {
                ctx->maxStackDepth = ctx->stackDepth;
            }
            break;
        }
        
        case OP_LT_II: {
            // Integer less than comparison
            // CMP RAX, RBX
            emit_cmp_reg_reg(&ctx->as, STACK_REG_0, STACK_REG_1);
            // SETL AL (set if less)
            // For simplicity, we'll use conditional move
            // MOV RAX, 0
            emit_mov_reg_imm32(&ctx->as, STACK_REG_0, 0);
            // JGE skip
            size_t skipPos = getCurrentPosition(&ctx->as);
            emit_jge(&ctx->as, 0);  // Placeholder offset
            // MOV RAX, 1
            emit_mov_reg_imm32(&ctx->as, STACK_REG_0, 1);
            // skip:
            size_t skipTarget = getCurrentPosition(&ctx->as);
            patchJumpOffset(&ctx->as, skipPos + 2, skipTarget);
            ctx->stackDepth--;
            break;
        }
        
        case OP_LE_II: {
            // Integer less than or equal: RAX <= RBX
            emit_cmp_reg_reg(&ctx->as, STACK_REG_0, STACK_REG_1);
            emit_mov_reg_imm32(&ctx->as, STACK_REG_0, 0);
            size_t skipPos = getCurrentPosition(&ctx->as);
            emit_jg(&ctx->as, 0);  // Jump if greater
            emit_mov_reg_imm32(&ctx->as, STACK_REG_0, 1);
            size_t skipTarget = getCurrentPosition(&ctx->as);
            patchJumpOffset(&ctx->as, skipPos + 2, skipTarget);
            ctx->stackDepth--;
            break;
        }
        
        case OP_GT_II: {
            // Integer greater than: RAX > RBX
            emit_cmp_reg_reg(&ctx->as, STACK_REG_0, STACK_REG_1);
            emit_mov_reg_imm32(&ctx->as, STACK_REG_0, 0);
            size_t skipPos = getCurrentPosition(&ctx->as);
            emit_jle(&ctx->as, 0);  // Jump if less or equal
            emit_mov_reg_imm32(&ctx->as, STACK_REG_0, 1);
            size_t skipTarget = getCurrentPosition(&ctx->as);
            patchJumpOffset(&ctx->as, skipPos + 2, skipTarget);
            ctx->stackDepth--;
            break;
        }
        
        case OP_GE_II: {
            // Integer greater than or equal: RAX >= RBX
            emit_cmp_reg_reg(&ctx->as, STACK_REG_0, STACK_REG_1);
            emit_mov_reg_imm32(&ctx->as, STACK_REG_0, 0);
            size_t skipPos = getCurrentPosition(&ctx->as);
            emit_jl(&ctx->as, 0);  // Jump if less
            emit_mov_reg_imm32(&ctx->as, STACK_REG_0, 1);
            size_t skipTarget = getCurrentPosition(&ctx->as);
            patchJumpOffset(&ctx->as, skipPos + 2, skipTarget);
            ctx->stackDepth--;
            break;
        }
        
        case OP_EQ_II: {
            // Integer equality: RAX == RBX
            emit_cmp_reg_reg(&ctx->as, STACK_REG_0, STACK_REG_1);
            emit_mov_reg_imm32(&ctx->as, STACK_REG_0, 0);
            size_t skipPos = getCurrentPosition(&ctx->as);
            emit_jne(&ctx->as, 0);  // Jump if not equal
            emit_mov_reg_imm32(&ctx->as, STACK_REG_0, 1);
            size_t skipTarget = getCurrentPosition(&ctx->as);
            patchJumpOffset(&ctx->as, skipPos + 2, skipTarget);
            ctx->stackDepth--;
            break;
        }
        
        case OP_NE_II: {
            // Integer not equal: RAX != RBX
            emit_cmp_reg_reg(&ctx->as, STACK_REG_0, STACK_REG_1);
            emit_mov_reg_imm32(&ctx->as, STACK_REG_0, 0);
            size_t skipPos = getCurrentPosition(&ctx->as);
            emit_je(&ctx->as, 0);  // Jump if equal
            emit_mov_reg_imm32(&ctx->as, STACK_REG_0, 1);
            size_t skipTarget = getCurrentPosition(&ctx->as);
            patchJumpOffset(&ctx->as, skipPos + 2, skipTarget);
            ctx->stackDepth--;
            break;
        }
        
        case OP_LT_FF:
        case OP_LE_FF:
        case OP_GT_FF:
        case OP_GE_FF:
        case OP_EQ_FF:
        case OP_NE_FF:
            // Float comparisons - not implemented yet
            fprintf(stderr, "JIT: Float comparisons not implemented yet\n");
            return false;
        
        case OP_LT:
        case OP_LE:
        case OP_GT:
        case OP_GE:
            // Generic comparisons (simplified - treat as integer for now)
            switch (op) {
                case OP_LT:
                    emit_cmp_reg_reg(&ctx->as, STACK_REG_0, STACK_REG_1);
                    emit_mov_reg_imm32(&ctx->as, STACK_REG_0, 0);
                    {
                        size_t skipPos = getCurrentPosition(&ctx->as);
                        emit_jge(&ctx->as, 0);
                        emit_mov_reg_imm32(&ctx->as, STACK_REG_0, 1);
                        size_t skipTarget = getCurrentPosition(&ctx->as);
                        patchJumpOffset(&ctx->as, skipPos + 2, skipTarget);
                    }
                    break;
                case OP_LE:
                    emit_cmp_reg_reg(&ctx->as, STACK_REG_0, STACK_REG_1);
                    emit_mov_reg_imm32(&ctx->as, STACK_REG_0, 0);
                    {
                        size_t skipPos = getCurrentPosition(&ctx->as);
                        emit_jg(&ctx->as, 0);
                        emit_mov_reg_imm32(&ctx->as, STACK_REG_0, 1);
                        size_t skipTarget = getCurrentPosition(&ctx->as);
                        patchJumpOffset(&ctx->as, skipPos + 2, skipTarget);
                    }
                    break;
                case OP_GT:
                    emit_cmp_reg_reg(&ctx->as, STACK_REG_0, STACK_REG_1);
                    emit_mov_reg_imm32(&ctx->as, STACK_REG_0, 0);
                    {
                        size_t skipPos = getCurrentPosition(&ctx->as);
                        emit_jle(&ctx->as, 0);
                        emit_mov_reg_imm32(&ctx->as, STACK_REG_0, 1);
                        size_t skipTarget = getCurrentPosition(&ctx->as);
                        patchJumpOffset(&ctx->as, skipPos + 2, skipTarget);
                    }
                    break;
                case OP_GE:
                    emit_cmp_reg_reg(&ctx->as, STACK_REG_0, STACK_REG_1);
                    emit_mov_reg_imm32(&ctx->as, STACK_REG_0, 0);
                    {
                        size_t skipPos = getCurrentPosition(&ctx->as);
                        emit_jl(&ctx->as, 0);
                        emit_mov_reg_imm32(&ctx->as, STACK_REG_0, 1);
                        size_t skipTarget = getCurrentPosition(&ctx->as);
                        patchJumpOffset(&ctx->as, skipPos + 2, skipTarget);
                    }
                    break;
                case OP_NE:
                    emit_cmp_reg_reg(&ctx->as, STACK_REG_0, STACK_REG_1);
                    emit_mov_reg_imm32(&ctx->as, STACK_REG_0, 0);
                    {
                        size_t skipPos = getCurrentPosition(&ctx->as);
                        emit_je(&ctx->as, 0);
                        emit_mov_reg_imm32(&ctx->as, STACK_REG_0, 1);
                        size_t skipTarget = getCurrentPosition(&ctx->as);
                        patchJumpOffset(&ctx->as, skipPos + 2, skipTarget);
                    }
                    break;
                default:
                    break;
            }
            ctx->stackDepth--;
            break;
        
        case OP_EQ: {
            // Generic equality (simplified - treat as integer for now)
            emit_cmp_reg_reg(&ctx->as, STACK_REG_0, STACK_REG_1);
            emit_mov_reg_imm32(&ctx->as, STACK_REG_0, 0);
            size_t skipPos = getCurrentPosition(&ctx->as);
            emit_jne(&ctx->as, 0);
            emit_mov_reg_imm32(&ctx->as, STACK_REG_0, 1);
            size_t skipTarget = getCurrentPosition(&ctx->as);
            patchJumpOffset(&ctx->as, skipPos + 2, skipTarget);
            ctx->stackDepth--;
            break;
        }
        
        case OP_NE: {
            // Generic not equal
            emit_cmp_reg_reg(&ctx->as, STACK_REG_0, STACK_REG_1);
            emit_mov_reg_imm32(&ctx->as, STACK_REG_0, 0);
            size_t skipPos = getCurrentPosition(&ctx->as);
            emit_je(&ctx->as, 0);
            emit_mov_reg_imm32(&ctx->as, STACK_REG_0, 1);
            size_t skipTarget = getCurrentPosition(&ctx->as);
            patchJumpOffset(&ctx->as, skipPos + 2, skipTarget);
            ctx->stackDepth--;
            break;
        }
        
        case OP_NOT:
        case OP_AND:
        case OP_OR:
            // Logical operations - simplified implementation
            // For now, just emit NOP (will implement properly later)
            emit_nop(&ctx->as);
            break;
        
        case OP_PRINT: {
            // Print operation - for now, just pop the value
            // In full implementation, would call native print function
            ctx->stackDepth--;
            break;
        }
        
        case OP_JUMP: {
            // Unconditional jump - read 2-byte offset
            int16_t bytecodeOffset = (int16_t)((code[*ip] << 8) | code[*ip + 1]);
            *ip += 2;
            
            // For now, emit a short forward jump (will be patched later)
            // JMP with placeholder offset
            emit_jmp(&ctx->as, 5);  // Skip 5 bytes (placeholder)
            break;
        }
        
        case OP_JUMP_IF_FALSE: {
            // Conditional jump - read 2-byte offset
            int16_t bytecodeOffset = (int16_t)((code[*ip] << 8) | code[*ip + 1]);
            *ip += 2;
            
            // TEST RAX, RAX (check if false)
            emit_test_reg_reg(&ctx->as, STACK_REG_0, STACK_REG_0);
            
            // JE (jump if zero/false) with placeholder offset
            emit_je(&ctx->as, 5);  // Skip 5 bytes (placeholder)
            
            ctx->stackDepth--;
            break;
        }
        
        case OP_LOOP: {
            // Backward jump for loops
            int16_t offset = (int16_t)((code[*ip] << 8) | code[*ip + 1]);
            *ip += 2;
            
            // Emit backward jump
            // Calculate relative offset (negative)
            emit_jmp(&ctx->as, -(int32_t)offset);
            break;
        }
        
        case OP_LOOP_HEADER: {
            // Loop header marker - just a NOP in JIT
            emit_nop(&ctx->as);
            break;
        }
        
        case OP_RETURN: {
            // Return from function
            // Result is already in RAX
            emitEpilog(ctx);
            return true;  // End of compilation
        }
        
        case OP_RETURN_NIL: {
            // Return nil (0)
            emit_mov_reg_imm32(&ctx->as, REG_RAX, 0);
            emitEpilog(ctx);
            return true;
        }
        
        case OP_HALT: {
            // Halt execution
            emitEpilog(ctx);
            return true;
        }
        
        case OP_NOP: {
            // No operation
            emit_nop(&ctx->as);
            break;
        }
        
        default: {
            // Unsupported opcode - compilation failed
            fprintf(stderr, "JIT: Unsupported opcode %d\n", op);
            return false;
        }
    }
    
    return true;
}

JITFunction* compileFunction(VM* vm, BytecodeChunk* chunk) {
    if (!chunk || chunk->codeSize == 0) {
        return NULL;
    }
    
    CompileContext ctx;
    initCompileContext(&ctx, vm, chunk);
    
    // Emit function prolog
    emitProlog(&ctx);
    
    // Compile bytecode instructions
    int ip = 0;
    bool success = true;
    while (ip < chunk->codeSize && success) {
        success = compileInstruction(&ctx, &ip);
    }
    
    if (!success) {
        freeCompileContext(&ctx);
        return NULL;
    }
    
    // If we didn't hit a return, emit epilog
    if (ip >= chunk->codeSize) {
        emitEpilog(&ctx);
    }
    
    // Finalize code
    size_t codeSize;
    void* tempCode = finalizeAssembler(&ctx.as, &codeSize);
    
    // Allocate executable memory
    void* execMem = allocateExecutableMemory(codeSize);
    if (!execMem) {
        freeCompileContext(&ctx);
        return NULL;
    }
    
    // Copy code to executable memory
    memcpy(execMem, tempCode, codeSize);
    
    // Make memory executable (W^X)
    if (!makeExecutable(execMem, codeSize)) {
        freeExecutableMemory(execMem, codeSize);
        freeCompileContext(&ctx);
        return NULL;
    }
    
    // Create JIT function object
    JITFunction* jitFunc = (JITFunction*)malloc(sizeof(JITFunction));
    jitFunc->code = execMem;
    jitFunc->codeSize = codeSize;
    jitFunc->stackDepth = ctx.maxStackDepth;
    jitFunc->isValid = true;
    
    freeCompileContext(&ctx);
    
    printf("JIT: Compiled %d bytes of bytecode to %zu bytes of native code\n", 
           chunk->codeSize, codeSize);
    
    return jitFunc;
}

Value executeJIT(VM* vm, JITFunction* jitFunc) {
    if (!jitFunc || !jitFunc->isValid) {
        // Return error value (nil)
        return NIL_VAL;
    }
    
    // Cast to function pointer and call
    // Function signature: Value (*)(VM*)
    typedef Value (*JITFuncPtr)(VM*);
    JITFuncPtr func = (JITFuncPtr)jitFunc->code;
    
    return func(vm);
}

void freeJITFunction(JITFunction* jitFunc) {
    if (!jitFunc) {
        return;
    }
    
    if (jitFunc->code) {
        freeExecutableMemory(jitFunc->code, jitFunc->codeSize);
    }
    
    free(jitFunc);
}
