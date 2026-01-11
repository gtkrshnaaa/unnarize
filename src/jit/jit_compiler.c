#include "jit/jit_compiler.h"
#include "jit/assembler.h"
#include "jit/memory.h"
#include "bytecode/opcodes.h"
#include "bytecode/chunk.h"
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
// R12 = operand stack pointer (grows downward, 8-byte values)
// RAX = scratch register for operations (top of stack loaded here)
// RBX = scratch register (second value loaded here for binary ops)
#define STACK_REG_0 REG_RAX  // Scratch: top of stack value
#define STACK_REG_1 REG_RBX  // Scratch: second value for binary ops
#define STACK_REG_2 REG_RCX  // Scratch: third value
#define STACK_REG_3 REG_RDX  // Scratch: fourth value
#define OPSTACK_PTR REG_R12  // Operand stack pointer

// Helper: Push RAX to operand stack (R12)
// R12 -= 8; [R12] = RAX
static inline void emitOpPush(Assembler* as) {
    emit_sub_reg_imm(as, REG_R12, 8);           // R12 -= 8
    emit_mov_mem_reg(as, REG_R12, 0, REG_RAX);  // [R12] = RAX
}

// Helper: Pop from operand stack to RAX
// RAX = [R12]; R12 += 8
static inline void emitOpPop(Assembler* as) {
    emit_mov_reg_mem(as, REG_RAX, REG_R12, 0);  // RAX = [R12]
    emit_add_reg_imm(as, REG_R12, 8);           // R12 += 8
}

// Helper: Pop from operand stack to RBX (for binary ops)
static inline void emitOpPopToBX(Assembler* as) {
    emit_mov_reg_mem(as, REG_RBX, REG_R12, 0);  // RBX = [R12]
    emit_add_reg_imm(as, REG_R12, 8);           // R12 += 8
}
// Helper: Peek top of operand stack to RAX (don't pop)
static inline void emitOpPeek(Assembler* as) {
    emit_mov_reg_mem(as, REG_RAX, REG_R12, 0);  // RAX = [R12]
}

// ============================================================================
// JIT Runtime Helpers (NanBoxing Compatible)
// ============================================================================

Value jitAdd(VM* vm, Value a, Value b) {
    if (IS_INT(a) && IS_INT(b)) {
        return INT_VAL(AS_INT(a) + AS_INT(b));
    }
    if (IS_NUMBER(a) && IS_NUMBER(b)) {
        double da = IS_INT(a) ? (double)AS_INT(a) : AS_FLOAT(a);
        double db = IS_INT(b) ? (double)AS_INT(b) : AS_FLOAT(b);
        return FLOAT_VAL(da + db);
    }
    if (IS_STRING(a) || IS_STRING(b)) {
        // GC Safety: Push operands to VM stack so they are rooted during allocation
        if (vm->stackTop + 2 >= STACK_MAX) return NIL_VAL; // Stack overflow check?
        vm->stack[vm->stackTop++] = a;
        vm->stack[vm->stackTop++] = b;
        
        Value result = vm_concatenate(vm, a, b);
        
        vm->stackTop -= 2; // Pop
        return result;
    }
    return NIL_VAL;
}

Value jitSub(Value a, Value b) {
    if (IS_INT(a) && IS_INT(b)) {
        return INT_VAL(AS_INT(a) - AS_INT(b));
    }
    if (IS_NUMBER(a) && IS_NUMBER(b)) {
        double da = IS_INT(a) ? (double)AS_INT(a) : AS_FLOAT(a);
        double db = IS_INT(b) ? (double)AS_INT(b) : AS_FLOAT(b);
        return FLOAT_VAL(da - db);
    }
    return NIL_VAL;
}

Value jitMul(Value a, Value b) {
    if (IS_INT(a) && IS_INT(b)) {
        return INT_VAL(AS_INT(a) * AS_INT(b));
    }
    if (IS_NUMBER(a) && IS_NUMBER(b)) {
        double da = IS_INT(a) ? (double)AS_INT(a) : AS_FLOAT(a);
        double db = IS_INT(b) ? (double)AS_INT(b) : AS_FLOAT(b);
        return FLOAT_VAL(da * db);
    }
    return NIL_VAL;
}

Value jitDiv(Value a, Value b) {
    if (IS_INT(a) && IS_INT(b)) {
        int ib = AS_INT(b);
        if (ib == 0) return NIL_VAL; // TODO: Error
        return INT_VAL(AS_INT(a) / ib);
    }
    if (IS_NUMBER(a) && IS_NUMBER(b)) {
        double da = IS_INT(a) ? (double)AS_INT(a) : AS_FLOAT(a);
        double db = IS_INT(b) ? (double)AS_INT(b) : AS_FLOAT(b);
        return FLOAT_VAL(da / db);
    }
    return NIL_VAL;
}

Value jitMod(Value a, Value b) {
    if (IS_INT(a) && IS_INT(b)) {
        int ib = AS_INT(b);
        if (ib == 0) return NIL_VAL;
        return INT_VAL(AS_INT(a) % ib);
    }
    return NIL_VAL;
}

Value jitNeg(Value a) {
    if (IS_INT(a)) return INT_VAL(-AS_INT(a));
    if (IS_FLOAT(a)) return FLOAT_VAL(-AS_FLOAT(a));
    return NIL_VAL;
}

Value jitLt(Value a, Value b) {
    if (IS_INT(a) && IS_INT(b)) return BOOL_VAL(AS_INT(a) < AS_INT(b));
    if (IS_NUMBER(a) && IS_NUMBER(b)) {
        double da = IS_INT(a) ? (double)AS_INT(a) : AS_FLOAT(a);
        double db = IS_INT(b) ? (double)AS_INT(b) : AS_FLOAT(b);
        return BOOL_VAL(da < db);
    }
    return BOOL_VAL(false);
}

Value jitLe(Value a, Value b) {
    if (IS_INT(a) && IS_INT(b)) return BOOL_VAL(AS_INT(a) <= AS_INT(b));
    if (IS_NUMBER(a) && IS_NUMBER(b)) {
        double da = IS_INT(a) ? (double)AS_INT(a) : AS_FLOAT(a);
        double db = IS_INT(b) ? (double)AS_INT(b) : AS_FLOAT(b);
        return BOOL_VAL(da <= db);
    }
    return BOOL_VAL(false);
}

Value jitGt(Value a, Value b) {
    if (IS_INT(a) && IS_INT(b)) return BOOL_VAL(AS_INT(a) > AS_INT(b));
    if (IS_NUMBER(a) && IS_NUMBER(b)) {
        double da = IS_INT(a) ? (double)AS_INT(a) : AS_FLOAT(a);
        double db = IS_INT(b) ? (double)AS_INT(b) : AS_FLOAT(b);
        return BOOL_VAL(da > db);
    }
    return BOOL_VAL(false);
}

Value jitGe(Value a, Value b) {
    if (IS_INT(a) && IS_INT(b)) return BOOL_VAL(AS_INT(a) >= AS_INT(b));
    if (IS_NUMBER(a) && IS_NUMBER(b)) {
        double da = IS_INT(a) ? (double)AS_INT(a) : AS_FLOAT(a);
        double db = IS_INT(b) ? (double)AS_INT(b) : AS_FLOAT(b);
        return BOOL_VAL(da >= db);
    }
    return BOOL_VAL(false);
}

Value jitEq(Value a, Value b) {
    return BOOL_VAL(a == b); // Basic identity for now
}

Value jitNe(Value a, Value b) {
    return BOOL_VAL(a != b);
}

// ============================================================================
// JIT Runtime Helpers
// These functions are called from JIT-compiled code to access VM state
// ============================================================================

// Load global variable by constant index
// Called from JIT: loadGlobalJIT(VM* vm, BytecodeChunk* chunk, uint8_t constIdx)
// Returns the global variable value as an int64_t (raw bits)
// Returns the global variable value (NanBoxed)
int64_t jitLoadGlobal(VM* vm, void* chunkPtr, int constIdx) {
    BytecodeChunk* chunk = (BytecodeChunk*)chunkPtr;
    ObjString* name = AS_STRING(chunk->constants[constIdx]);
    unsigned int h = name->hash % TABLE_SIZE;
    VarEntry* entry = vm->globalEnv->buckets[h];
    while (entry) {
        if (entry->key == name->chars) {
            // Return the full value (NanBoxed)
            return (int64_t)entry->value;
        }
        entry = entry->next;
    }
    printf("Runtime Error: Undefined global variable '%s'\n", name->chars);
    exit(1);
}

// Store global variable by constant index
// Called from JIT: jitStoreGlobal(VM* vm, BytecodeChunk* chunk, uint8_t constIdx, int64_t value)
// Store global variable (Value is already NanBoxed)
void jitStoreGlobal(VM* vm, void* chunkPtr, int constIdx, int64_t value) {
    BytecodeChunk* chunk = (BytecodeChunk*)chunkPtr;
    ObjString* name = AS_STRING(chunk->constants[constIdx]);
    unsigned int h = name->hash % TABLE_SIZE;
    VarEntry* entry = vm->globalEnv->buckets[h];
    while (entry) {
        if (entry->key == name->chars) {
            entry->value = (Value)value;
            return;
        }
        entry = entry->next;
    }
    // Insert new
    VarEntry* newEntry = malloc(sizeof(VarEntry));
    newEntry->key = name->chars;
    newEntry->keyLength = name->length;
    newEntry->ownsKey = false;
    newEntry->value = (Value)value;
    newEntry->next = vm->globalEnv->buckets[h];
    vm->globalEnv->buckets[h] = newEntry;
}

// Print Value from JIT
void jitPrint(int64_t value) {
    printValue((Value)value);
    printf("\n");
}

// Jump patch entry for forward jumps
typedef struct {
    size_t nativePos;       // Position of jump instruction in native code
    int targetBytecodeIP;   // Target bytecode IP
    int jumpSize;           // Size of jump instruction (5 for JMP, 6 for Jcc)
} JumpPatch;

#define MAX_JUMP_PATCHES 256

// Compilation context
typedef struct {
    Assembler as;
    BytecodeChunk* chunk;
    VM* vm;
    int stackDepth;
    int maxStackDepth;
    size_t* bytecodeToNative;  // Map: bytecode IP -> native code offset
    int offsetMapSize;
    // Jump patching
    JumpPatch pendingJumps[MAX_JUMP_PATCHES];
    int pendingJumpCount;
} CompileContext;

static void initCompileContext(CompileContext* ctx, VM* vm, BytecodeChunk* chunk) {
    initAssembler(&ctx->as, 4096);
    ctx->chunk = chunk;
    ctx->vm = vm;
    ctx->stackDepth = 0;
    ctx->maxStackDepth = 0;
    ctx->offsetMapSize = chunk->codeSize + 1;
    ctx->bytecodeToNative = calloc(ctx->offsetMapSize, sizeof(size_t));
    ctx->pendingJumpCount = 0;
}

static void freeCompileContext(CompileContext* ctx) {
    freeAssembler(&ctx->as);
    if (ctx->bytecodeToNative) {
        free(ctx->bytecodeToNative);
    }
}

// Add a pending jump that needs to be patched after compilation
static void addPendingJump(CompileContext* ctx, size_t nativePos, int targetBytecodeIP, int jumpSize) {
    if (ctx->pendingJumpCount >= MAX_JUMP_PATCHES) {
        fprintf(stderr, "JIT: Too many pending jumps\n");
        return;
    }
    JumpPatch* patch = &ctx->pendingJumps[ctx->pendingJumpCount++];
    patch->nativePos = nativePos;
    patch->targetBytecodeIP = targetBytecodeIP;
    patch->jumpSize = jumpSize;
}

// Patch all pending jumps with correct native offsets
static void patchAllJumps(CompileContext* ctx) {
    for (int i = 0; i < ctx->pendingJumpCount; i++) {
        JumpPatch* patch = &ctx->pendingJumps[i];
        
        // Look up target native position
        size_t targetNativePos = 0;
        if (patch->targetBytecodeIP >= 0 && patch->targetBytecodeIP < ctx->offsetMapSize) {
            targetNativePos = ctx->bytecodeToNative[patch->targetBytecodeIP];
        }
        
        // Calculate relative offset from end of jump instruction
        size_t jumpEnd = patch->nativePos + patch->jumpSize;
        int32_t relOffset = (int32_t)(targetNativePos - jumpEnd);
        
        // Patch the offset in the code buffer
        // Jump offset starts at nativePos + (jumpSize - 4) for near jumps
        size_t offsetPos = patch->nativePos + (patch->jumpSize - 4);
        ctx->as.code[offsetPos]     = (uint8_t)(relOffset & 0xFF);
        ctx->as.code[offsetPos + 1] = (uint8_t)((relOffset >> 8) & 0xFF);
        ctx->as.code[offsetPos + 2] = (uint8_t)((relOffset >> 16) & 0xFF);
        ctx->as.code[offsetPos + 3] = (uint8_t)((relOffset >> 24) & 0xFF);
    }
}

// Emit function prolog
// Memory layout after prolog:
//   RBP points to saved RBP
//   [RBP - 8] to [RBP - 128] = local variables (16 slots)
//   [RBP - 136] onwards = operand stack (grows down)
//   R12 = operand stack pointer, starts at [RBP - 136]
static void emitProlog(CompileContext* ctx) {
    // Standard function prolog
    // PUSH RBP
    emit_push_reg(&ctx->as, REG_RBP);
    // MOV RBP, RSP
    emit_mov_reg_reg(&ctx->as, REG_RBP, REG_RSP);
    
    // Allocate stack space:
    // - 128 bytes for locals (16 * 8)
    // - 256 bytes for operand stack (32 values)
    // - 16 bytes for alignment
    // Total: 400 bytes, round to 416 for 16-byte alignment
    int stackSpace = 416;
    emit_sub_reg_imm(&ctx->as, REG_RSP, stackSpace);
    
    // Save callee-saved registers
    emit_push_reg(&ctx->as, REG_RBX);
    emit_push_reg(&ctx->as, REG_R12);
    emit_push_reg(&ctx->as, REG_R13);
    emit_push_reg(&ctx->as, REG_R14);
    emit_push_reg(&ctx->as, REG_R15);
    
    // RDI contains VM pointer (first argument)
    // Save it to R15 for later use
    emit_mov_reg_reg(&ctx->as, REG_R15, REG_RDI);
    
    // Initialize R12 as operand stack pointer
    // R12 = RBP - 136 (start of operand stack area)
    emit_mov_reg_reg(&ctx->as, REG_R12, REG_RBP);
    emit_sub_reg_imm(&ctx->as, REG_R12, 136);
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
static bool compileInstruction(CompileContext* ctx, int* ip, bool recordOffsets) {
    uint8_t* code = ctx->chunk->code;
    int startIP = *ip;
    OpCode op = (OpCode)code[*ip];
    (*ip)++;
    
    // Record bytecode-to-native offset mapping
    if (recordOffsets && startIP < ctx->offsetMapSize) {
        ctx->bytecodeToNative[startIP] = getCurrentPosition(&ctx->as);
    }
    
    switch (op) {
        case OP_LOAD_IMM: {
            // Load 32-bit immediate value and push to operand stack
            int32_t value = (int32_t)(
                (code[*ip] << 0) |
                (code[*ip + 1] << 8) |
                (code[*ip + 2] << 16) |
                (code[*ip + 3] << 24)
            );
            *ip += 4;
            
            // MOV RAX, value; then push to operand stack
            emit_mov_reg_imm32(&ctx->as, REG_RAX, value);
            emitOpPush(&ctx->as);
            ctx->stackDepth++;
            if (ctx->stackDepth > ctx->maxStackDepth) {
                ctx->maxStackDepth = ctx->stackDepth;
            }
            break;
        }
        
        case OP_LOAD_CONST: {
            // Load from constant pool - simplified, just push 0
            uint8_t constIdx = code[*ip];
            (*ip)++;
            (void)constIdx;
            
            emit_mov_reg_imm32(&ctx->as, REG_RAX, 0);
            emitOpPush(&ctx->as);
            ctx->stackDepth++;
            if (ctx->stackDepth > ctx->maxStackDepth) {
                ctx->maxStackDepth = ctx->stackDepth;
            }
            break;
        }
        
        case OP_LOAD_NIL: {
            emit_mov_reg_imm32(&ctx->as, REG_RAX, 0);
            emitOpPush(&ctx->as);
            ctx->stackDepth++;
            if (ctx->stackDepth > ctx->maxStackDepth) {
                ctx->maxStackDepth = ctx->stackDepth;
            }
            break;
        }
        
        case OP_LOAD_TRUE: {
            emit_mov_reg_imm32(&ctx->as, REG_RAX, 1);
            emitOpPush(&ctx->as);
            ctx->stackDepth++;
            if (ctx->stackDepth > ctx->maxStackDepth) {
                ctx->maxStackDepth = ctx->stackDepth;
            }
            break;
        }
        
        case OP_LOAD_FALSE: {
            emit_mov_reg_imm32(&ctx->as, REG_RAX, 0);
            emitOpPush(&ctx->as);
            ctx->stackDepth++;
            if (ctx->stackDepth > ctx->maxStackDepth) {
                ctx->maxStackDepth = ctx->stackDepth;
            }
            break;
        }
        
        case OP_POP: {
            // Pop and discard: just adjust R12
            emit_add_reg_imm(&ctx->as, REG_R12, 8);
            ctx->stackDepth--;
            break;
        }
        
        case OP_DUP: {
            // Duplicate top of stack: peek and push
            emitOpPeek(&ctx->as);  // RAX = [R12] (don't pop)
            emitOpPush(&ctx->as);  // Push copy
            ctx->stackDepth++;
            if (ctx->stackDepth > ctx->maxStackDepth) {
                ctx->maxStackDepth = ctx->stackDepth;
            }
            break;
        }
        
        case OP_ADD_II: 
        case OP_ADD_FF:
        case OP_ADD: {
            // Pop b to RSI, a to RDI
            // CALL jitAdd(VM* vm, Value a, Value b)
            // RDI = vm (R15), RSI = a, RDX = b
            
            // 1. Pop b to RDX
            emit_mov_reg_mem(&ctx->as, REG_RDX, REG_R12, 0); emit_add_reg_imm(&ctx->as, REG_R12, 8);
            // 2. Pop a to RSI
            emit_mov_reg_mem(&ctx->as, REG_RSI, REG_R12, 0); emit_add_reg_imm(&ctx->as, REG_R12, 8);
            // 3. VM to RDI
            emit_mov_reg_reg(&ctx->as, REG_RDI, REG_R15);
            
            emit_call_abs(&ctx->as, jitAdd);
            emitOpPush(&ctx->as);
            ctx->stackDepth--;
            break;
        }

        case OP_SUB_II:
        case OP_SUB_FF:
        case OP_SUB: {
            // Pop b to RSI, a to RDI
            emit_mov_reg_mem(&ctx->as, REG_RSI, REG_R12, 0); emit_add_reg_imm(&ctx->as, REG_R12, 8);
            emit_mov_reg_mem(&ctx->as, REG_RDI, REG_R12, 0); emit_add_reg_imm(&ctx->as, REG_R12, 8);
            emit_call_abs(&ctx->as, jitSub);
            emitOpPush(&ctx->as);
            ctx->stackDepth--;
            break;
        }

        case OP_MUL_II: 
        case OP_MUL_FF:
        case OP_MUL: {
            emit_mov_reg_mem(&ctx->as, REG_RSI, REG_R12, 0); emit_add_reg_imm(&ctx->as, REG_R12, 8);
            emit_mov_reg_mem(&ctx->as, REG_RDI, REG_R12, 0); emit_add_reg_imm(&ctx->as, REG_R12, 8);
            emit_call_abs(&ctx->as, jitMul);
            emitOpPush(&ctx->as);
            ctx->stackDepth--;
            break;
        }

        case OP_DIV_II:
        case OP_DIV_FF:
        case OP_DIV: {
            emit_mov_reg_mem(&ctx->as, REG_RSI, REG_R12, 0); emit_add_reg_imm(&ctx->as, REG_R12, 8);
            emit_mov_reg_mem(&ctx->as, REG_RDI, REG_R12, 0); emit_add_reg_imm(&ctx->as, REG_R12, 8);
            emit_call_abs(&ctx->as, jitDiv);
            emitOpPush(&ctx->as);
            ctx->stackDepth--;
            break;
        }

        case OP_MOD_II:
        case OP_MOD: {
            emit_mov_reg_mem(&ctx->as, REG_RSI, REG_R12, 0); emit_add_reg_imm(&ctx->as, REG_R12, 8);
            emit_mov_reg_mem(&ctx->as, REG_RDI, REG_R12, 0); emit_add_reg_imm(&ctx->as, REG_R12, 8);
            emit_call_abs(&ctx->as, jitMod);
            emitOpPush(&ctx->as);
            ctx->stackDepth--;
            break;
        }

        // Duplicate OP_NEG removed

        
        case OP_LOAD_GLOBAL: {
            // Load global variable using JIT helper
            uint8_t constIdx = code[*ip];
            (*ip)++;
            
            // Emit call to jitLoadGlobal(VM* vm, void* chunk, int constIdx)
            // System V AMD64 ABI: RDI=arg1, RSI=arg2, RDX=arg3
            // R15 has VM pointer (saved in prolog)
            emit_mov_reg_reg(&ctx->as, REG_RDI, REG_R15);  // arg1 = vm
            emit_mov_reg_imm64(&ctx->as, REG_RSI, (int64_t)ctx->chunk);  // arg2 = chunk
            emit_mov_reg_imm32(&ctx->as, REG_RDX, constIdx);  // arg3 = constIdx
            emit_call_abs(&ctx->as, jitLoadGlobal);
            // Result is in RAX, push to operand stack
            emitOpPush(&ctx->as);
            ctx->stackDepth++;
            if (ctx->stackDepth > ctx->maxStackDepth) {
                ctx->maxStackDepth = ctx->stackDepth;
            }
            break;
        }
        
        case OP_STORE_GLOBAL: {
            // Store global variable using JIT helper
            uint8_t constIdx = code[*ip];
            (*ip)++;
            
            // Pop value from operand stack to RAX
            emitOpPop(&ctx->as);  // RAX = value
            // Save value in R10 (caller-saved, safe across call)
            emit_mov_reg_reg(&ctx->as, REG_R10, REG_RAX);
            
            // Emit call to jitStoreGlobal(VM* vm, void* chunk, int constIdx, int64_t value)
            // System V AMD64 ABI: RDI=arg1, RSI=arg2, RDX=arg3, RCX=arg4
            emit_mov_reg_reg(&ctx->as, REG_RDI, REG_R15);  // arg1 = vm
            emit_mov_reg_imm64(&ctx->as, REG_RSI, (int64_t)ctx->chunk);  // arg2 = chunk
            emit_mov_reg_imm32(&ctx->as, REG_RDX, constIdx);  // arg3 = constIdx
            emit_mov_reg_reg(&ctx->as, REG_RCX, REG_R10);  // arg4 = value
            emit_call_abs(&ctx->as, jitStoreGlobal);
            // Note: STORE_GLOBAL doesn't pop the value in bytecode VM semantics
            // so we push it back
            emit_mov_reg_reg(&ctx->as, REG_RAX, REG_R10);
            emitOpPush(&ctx->as);
            break;
        }
        
        case OP_DEFINE_GLOBAL: {
            // Define global is same as store for JIT purposes
            uint8_t constIdx = code[*ip];
            (*ip)++;
            
            // Pop value from operand stack
            emitOpPop(&ctx->as);  // RAX = value
            emit_mov_reg_reg(&ctx->as, REG_R10, REG_RAX);  // Save in R10
            
            // Emit call to jitStoreGlobal
            emit_mov_reg_reg(&ctx->as, REG_RDI, REG_R15);  // arg1 = vm
            emit_mov_reg_imm64(&ctx->as, REG_RSI, (int64_t)ctx->chunk);  // arg2 = chunk
            emit_mov_reg_imm32(&ctx->as, REG_RDX, constIdx);  // arg3 = constIdx
            emit_mov_reg_reg(&ctx->as, REG_RCX, REG_R10);  // arg4 = value
            emit_call_abs(&ctx->as, jitStoreGlobal);
            // DEFINE_GLOBAL pops the value (unlike STORE)
            ctx->stackDepth--;
            break;
        }
        
        case OP_NEG_I: {
            // Pop, negate, push back
            emitOpPop(&ctx->as);
            emit_neg_reg(&ctx->as, REG_RAX);
            emitOpPush(&ctx->as);
            break;
        }
        
        case OP_LOAD_LOCAL: {
            // Load local variable and push to operand stack
            uint8_t index = code[*ip];
            (*ip)++;
            
            // MOV RAX, [RBP - offset] then push
            int32_t offset = -((int32_t)index * 8 + 8);
            emit_mov_reg_mem(&ctx->as, REG_RAX, REG_RBP, offset);
            emitOpPush(&ctx->as);
            ctx->stackDepth++;
            if (ctx->stackDepth > ctx->maxStackDepth) {
                ctx->maxStackDepth = ctx->stackDepth;
            }
            break;
        }
        
        case OP_STORE_LOCAL: {
            // Pop from operand stack and store to local
            uint8_t index = code[*ip];
            (*ip)++;
            
            // Pop to RAX, then MOV [RBP - offset], RAX
            emitOpPop(&ctx->as);
            int32_t offset = -((int32_t)index * 8 + 8);
            emit_mov_mem_reg(&ctx->as, REG_RBP, offset, REG_RAX);
            ctx->stackDepth--;
            break;
        }
        
        case OP_LOAD_LOCAL_0: {
            // Fast path for local 0
            emit_mov_reg_mem(&ctx->as, REG_RAX, REG_RBP, -8);
            emitOpPush(&ctx->as);
            ctx->stackDepth++;
            if (ctx->stackDepth > ctx->maxStackDepth) {
                ctx->maxStackDepth = ctx->stackDepth;
            }
            break;
        }
        
        case OP_LOAD_LOCAL_1: {
            // Fast path for local 1
            emit_mov_reg_mem(&ctx->as, REG_RAX, REG_RBP, -16);
            emitOpPush(&ctx->as);
            ctx->stackDepth++;
            if (ctx->stackDepth > ctx->maxStackDepth) {
                ctx->maxStackDepth = ctx->stackDepth;
            }
            break;
        }
        
        case OP_LOAD_LOCAL_2: {
            // Fast path for local 2
            emit_mov_reg_mem(&ctx->as, REG_RAX, REG_RBP, -24);
            emitOpPush(&ctx->as);
            ctx->stackDepth++;
            if (ctx->stackDepth > ctx->maxStackDepth) {
                ctx->maxStackDepth = ctx->stackDepth;
            }
            break;
        }
        
        case OP_LT_II:
        case OP_LT_FF:
        case OP_LT: {
            emit_mov_reg_mem(&ctx->as, REG_RSI, REG_R12, 0); emit_add_reg_imm(&ctx->as, REG_R12, 8);
            emit_mov_reg_mem(&ctx->as, REG_RDI, REG_R12, 0); emit_add_reg_imm(&ctx->as, REG_R12, 8);
            emit_call_abs(&ctx->as, jitLt);
            emitOpPush(&ctx->as);
            ctx->stackDepth--;
            break;
        }

        case OP_LE_II:
        case OP_LE_FF:
        case OP_LE: {
            emit_mov_reg_mem(&ctx->as, REG_RSI, REG_R12, 0); emit_add_reg_imm(&ctx->as, REG_R12, 8);
            emit_mov_reg_mem(&ctx->as, REG_RDI, REG_R12, 0); emit_add_reg_imm(&ctx->as, REG_R12, 8);
            emit_call_abs(&ctx->as, jitLe);
            emitOpPush(&ctx->as);
            ctx->stackDepth--;
            break;
        }

        case OP_GT_II:
        case OP_GT_FF:
        case OP_GT: {
            emit_mov_reg_mem(&ctx->as, REG_RSI, REG_R12, 0); emit_add_reg_imm(&ctx->as, REG_R12, 8);
            emit_mov_reg_mem(&ctx->as, REG_RDI, REG_R12, 0); emit_add_reg_imm(&ctx->as, REG_R12, 8);
            emit_call_abs(&ctx->as, jitGt);
            emitOpPush(&ctx->as);
            ctx->stackDepth--;
            break;
        }

        case OP_GE_II:
        case OP_GE_FF:
        case OP_GE: {
            emit_mov_reg_mem(&ctx->as, REG_RSI, REG_R12, 0); emit_add_reg_imm(&ctx->as, REG_R12, 8);
            emit_mov_reg_mem(&ctx->as, REG_RDI, REG_R12, 0); emit_add_reg_imm(&ctx->as, REG_R12, 8);
            emit_call_abs(&ctx->as, jitGe);
            emitOpPush(&ctx->as);
            ctx->stackDepth--;
            break;
        }

        case OP_EQ_II:
        case OP_EQ_FF:
        case OP_EQ: {
            emit_mov_reg_mem(&ctx->as, REG_RSI, REG_R12, 0); emit_add_reg_imm(&ctx->as, REG_R12, 8);
            emit_mov_reg_mem(&ctx->as, REG_RDI, REG_R12, 0); emit_add_reg_imm(&ctx->as, REG_R12, 8);
            emit_call_abs(&ctx->as, jitEq);
            emitOpPush(&ctx->as);
            ctx->stackDepth--;
            break;
        }

        case OP_NE_II:
        case OP_NE_FF:
        case OP_NE: {
            emit_mov_reg_mem(&ctx->as, REG_RSI, REG_R12, 0); emit_add_reg_imm(&ctx->as, REG_R12, 8);
            emit_mov_reg_mem(&ctx->as, REG_RDI, REG_R12, 0); emit_add_reg_imm(&ctx->as, REG_R12, 8);
            emit_call_abs(&ctx->as, jitNe);
            emitOpPush(&ctx->as);
            ctx->stackDepth--;
            break;
        }
        
        case OP_NOT: {
            // Logical NOT: pop, invert boolean, push
            emitOpPop(&ctx->as);  // RAX = value
            // XOR RAX, 1 (flip boolean: 0->1, 1->0)
            emit_xor_reg_imm(&ctx->as, REG_RAX, 1);
            emitOpPush(&ctx->as);
            break;
        }
        
        case OP_AND: {
            // Logical AND: pop b, pop a, push (a && b)
            emitOpPopToBX(&ctx->as);  // RBX = b
            emitOpPop(&ctx->as);      // RAX = a
            // AND RAX, RBX
            emit_and_reg_reg(&ctx->as, REG_RAX, REG_RBX);
            emitOpPush(&ctx->as);
            ctx->stackDepth--;
            break;
        }
        
        case OP_OR: {
            // Logical OR: pop b, pop a, push (a || b)
            emitOpPopToBX(&ctx->as);  // RBX = b
            emitOpPop(&ctx->as);      // RAX = a
            // OR RAX, RBX
            emit_or_reg_reg(&ctx->as, REG_RAX, REG_RBX);
            emitOpPush(&ctx->as);
            ctx->stackDepth--;
            break;
        }
        
        case OP_PRINT: {
            // Pop value and print using jitPrint helper
            emitOpPop(&ctx->as);  // RAX = value
            // Call jitPrint(int64_t value)
            // System V AMD64 ABI: RDI = first arg
            emit_mov_reg_reg(&ctx->as, REG_RDI, REG_RAX);  // arg1 = value
            emit_call_abs(&ctx->as, jitPrint);
            ctx->stackDepth--;
            break;
        }
        
        case OP_JUMP: {
            // Unconditional jump - read 2-byte offset
            int16_t bytecodeOffset = (int16_t)((code[*ip] << 8) | code[*ip + 1]);
            int targetBytecodeIP = *ip + 2 + bytecodeOffset;
            *ip += 2;
            
            // Record current position for patching
            size_t jumpPos = getCurrentPosition(&ctx->as);
            
            // Check if this is a backward jump (target already compiled)
            if (targetBytecodeIP >= 0 && targetBytecodeIP < ctx->offsetMapSize && 
                ctx->bytecodeToNative[targetBytecodeIP] != 0) {
                // Backward jump - calculate offset now
                size_t targetNativePos = ctx->bytecodeToNative[targetBytecodeIP];
                int32_t nativeOffset = (int32_t)(targetNativePos - jumpPos - 5);
                emit_jmp(&ctx->as, nativeOffset);
            } else {
                // Forward jump - emit placeholder and record for patching
                emit_jmp(&ctx->as, 0);  // Placeholder offset
                addPendingJump(ctx, jumpPos, targetBytecodeIP, 5);  // JMP is 5 bytes
            }
            break;
        }
        
        case OP_JUMP_IF_FALSE: {
            // Conditional jump - read 2-byte offset
            int16_t bytecodeOffset = (int16_t)((code[*ip] << 8) | code[*ip + 1]);
            int targetBytecodeIP = *ip + 2 + bytecodeOffset;
            *ip += 2;
            
            // Pop condition from operand stack and test
            emitOpPop(&ctx->as);  // RAX = condition
            emit_test_reg_reg(&ctx->as, REG_RAX, REG_RAX);  // Check if zero/false
            
            // Record current position for patching
            size_t jumpPos = getCurrentPosition(&ctx->as);
            
            // Check if this is a backward jump (target already compiled)
            if (targetBytecodeIP >= 0 && targetBytecodeIP < ctx->offsetMapSize && 
                ctx->bytecodeToNative[targetBytecodeIP] != 0) {
                // Backward jump - calculate offset now
                size_t targetNativePos = ctx->bytecodeToNative[targetBytecodeIP];
                int32_t nativeOffset = (int32_t)(targetNativePos - jumpPos - 6);
                emit_je(&ctx->as, nativeOffset);
            } else {
                // Forward jump - emit placeholder and record for patching
                emit_je(&ctx->as, 0);  // Placeholder offset
                addPendingJump(ctx, jumpPos, targetBytecodeIP, 6);  // JE is 6 bytes
            }
            
            ctx->stackDepth--;
            break;
        }
        
        case OP_LOOP: {
            // Backward jump for loops - target is always already compiled
            int16_t bytecodeOffset = (int16_t)((code[*ip] << 8) | code[*ip + 1]);
            int targetBytecodeIP = *ip + 2 - bytecodeOffset;  // Backward jump
            *ip += 2;
            
            // For OP_LOOP, target should always be already compiled (it's a backward jump)
            size_t currentNativePos = getCurrentPosition(&ctx->as);
            size_t targetNativePos = 0;
            
            if (targetBytecodeIP >= 0 && targetBytecodeIP < ctx->offsetMapSize) {
                targetNativePos = ctx->bytecodeToNative[targetBytecodeIP];
            }
            
            // Calculate relative offset for JMP instruction (backward)
            int32_t nativeOffset = (int32_t)(targetNativePos - currentNativePos - 5);
            emit_jmp(&ctx->as, nativeOffset);
            break;
        }
        
        case OP_LOOP_HEADER: {
            // Loop header marker - just a NOP in JIT
            emit_nop(&ctx->as);
            break;
        }
        
        case OP_RETURN: {
            // Pop return value from operand stack to RAX
            emitOpPop(&ctx->as);  // RAX = result
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
        
        // === Function Calls ===
        // Function calls are complex and require interpreter fallback
        // JIT compilation of functions with calls will fail, and the
        // bytecode interpreter will handle them instead
        case OP_CALL:
        case OP_CALL_0:
        case OP_CALL_1:
        case OP_CALL_2: {
            // Read argument count (for OP_CALL) and skip
            if (op == OP_CALL) {
                (*ip)++;  // Skip argCount byte
            }
            // Return false to trigger interpreter fallback
            // This is safer than trying to implement full call semantics in JIT
            // fprintf(stderr, "JIT: Function call encountered - falling back to interpreter\n");
            return false;
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
    
    // Compile bytecode instructions (Pass 1: record offsets)
    int ip = 0;
    bool success = true;
    while (ip < chunk->codeSize && success) {
        success = compileInstruction(&ctx, &ip, true);  // Record offsets
    }
    
    if (!success) {
        freeCompileContext(&ctx);
        return NULL;
    }
    
    // If we didn't hit a return, emit epilog
    if (ip >= chunk->codeSize) {
        emitEpilog(&ctx);
    }
    
    // Patch all forward jumps now that we know all target positions
    patchAllJumps(&ctx);
    
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
