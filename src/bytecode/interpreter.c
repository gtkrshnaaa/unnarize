#include "bytecode/interpreter.h"
#include "bytecode/opcodes.h"
#include <stdio.h>
#include <sys/time.h>

/**
 * Fast Bytecode Interpreter
 * Direct threading with computed goto - GCC/Clang only
 * Zero external dependencies, maximum performance
 */

// Get current time in microseconds
static inline uint64_t getMicroseconds() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

#define STACK_MAX 65536

uint64_t executeBytecode(VM* vm, BytecodeChunk* chunk) {
    // Performance tracking
    uint64_t startTime = getMicroseconds();
    uint64_t instructionCount = 0;
    
    // Stack machine
    Value stack[STACK_MAX];
    Value* sp = stack;  // Stack pointer in register
    
    // Code pointer
    register uint8_t* ip = chunk->code;  // Instruction pointer in register
    
#ifdef __GNUC__
    // === COMPUTED GOTO DISPATCH TABLE ===
    // This is the KEY to performance - direct threading!
    static void* dispatchTable[] = {
        &&op_load_imm, &&op_load_const, &&op_load_nil, &&op_load_true, &&op_load_false,
        &&op_pop, &&op_dup,
        &&op_load_local, &&op_store_local, &&op_load_local_0, &&op_load_local_1, &&op_load_local_2,
        &&op_load_global, &&op_store_global, &&op_define_global,
        &&op_add_ii, &&op_sub_ii, &&op_mul_ii, &&op_div_ii, &&op_mod_ii, &&op_neg_i,
        &&op_add_ff, &&op_sub_ff, &&op_mul_ff, &&op_div_ff, &&op_neg_f,
        &&op_add, &&op_sub, &&op_mul, &&op_div, &&op_mod, &&op_neg,
        &&op_lt_ii, &&op_le_ii, &&op_gt_ii, &&op_ge_ii, &&op_eq_ii, &&op_ne_ii,
        &&op_lt_ff, &&op_le_ff, &&op_gt_ff, &&op_ge_ff, &&op_eq_ff, &&op_ne_ff,
        &&op_lt, &&op_le, &&op_gt, &&op_ge, &&op_eq, &&op_ne,
        &&op_not, &&op_and, &&op_or,
        &&op_jump, &&op_jump_if_false, &&op_jump_if_true, &&op_loop, &&op_loop_header,
        &&op_call, &&op_call_0, &&op_call_1, &&op_call_2, &&op_return, &&op_return_nil,
        &&op_load_property, &&op_store_property, &&op_load_index, &&op_store_index,
        &&op_new_array, &&op_new_map, &&op_new_object,
        &&op_array_push, &&op_array_pop, &&op_array_len,
        &&op_hotspot_check, &&op_osr_entry, &&op_compiled_call, &&op_deopt,
        &&op_print, &&op_halt, &&op_nop
    };
    
    #define DISPATCH() do { instructionCount++; goto *dispatchTable[*ip++]; } while(0)
    #define NEXT() DISPATCH()
    
    DISPATCH();  // Start execution
    
    // === STACK OPERATIONS ===
    op_load_imm: {
        // Load 32-bit immediate (4 bytes)
        int32_t value = (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | ip[3];
        *sp++ = ((Value){VAL_INT, .intVal = value);
        ip += 4;
        NEXT();
    }
    
    op_load_const: {
        uint8_t idx = *ip++;
        *sp++ = chunk->constants[idx];
        NEXT();
    }
    
    op_load_nil: {
        Value nil = {VAL_NIL, .intVal = 0};
        *sp++ = nil;
        NEXT();
    }
    
    op_load_true: {
        Value v = {VAL_BOOL, .boolVal = true};
        *sp++ = v;
        NEXT();
    }
    
    op_load_false: {
        Value v = {VAL_BOOL, .boolVal = false};
        *sp++ = v;
        NEXT();
    }
    
    op_pop: {
        sp--;
        NEXT();
    }
    
    op_dup: {
        *sp = sp[-1];
        sp++;
        NEXT();
    }
    
    // === LOCAL VARIABLES ===
    op_load_local: {
        uint8_t slot = *ip++;
        *sp++ = stack[slot];
        NEXT();
    }
    
    op_store_local: {
        uint8_t slot = *ip++;
        stack[slot] = sp[-1];
        NEXT();
    }
    
    op_load_local_0: {
        *sp++ = stack[0];
        NEXT();
    }
    
    op_load_local_1: {
        *sp++ = stack[1];
        NEXT();
    }
    
    op_load_local_2: {
        *sp++ = stack[2];
        NEXT();
    }
    
    // === GLOBAL VARIABLES (Stubs for now) ===
    op_load_global:
    op_store_global:
    op_define_global: {
        ip += 2;  // Skip operands
        NEXT();
    }
    
    // === SPECIALIZED ARITHMETIC (INT) ===
    op_add_ii: {
        int64_t b = AS_INT(*(--sp));
        int64_t a = AS_INT(*(--sp));
        *sp++ = ((Value){VAL_INT, .intVal = a + b);
        NEXT();
    }
    
    op_sub_ii: {
        int64_t b = AS_INT(*(--sp));
        int64_t a = AS_INT(*(--sp));
        *sp++ = ((Value){VAL_INT, .intVal = a - b);
        NEXT();
    }
    
    op_mul_ii: {
        int64_t b = AS_INT(*(--sp));
        int64_t a = AS_INT(*(--sp));
        *sp++ = ((Value){VAL_INT, .intVal = a * b);
        NEXT();
    }
    
    op_div_ii: {
        int64_t b = AS_INT(*(--sp));
        int64_t a = AS_INT(*(--sp));
        *sp++ = ((Value){VAL_INT, .intVal = a / b);
        NEXT();
    }
    
    op_mod_ii: {
        int64_t b = AS_INT(*(--sp));
        int64_t a = AS_INT(*(--sp));
        *sp++ = ((Value){VAL_INT, .intVal = a % b);
        NEXT();
    }
    
    op_neg_i: {
        sp[-1] = ((Value){VAL_INT, .intVal = -AS_INT(sp[-1]));
        NEXT();
    }
    
    // === SPECIALIZED ARITHMETIC (FLOAT) ===
    op_add_ff: {
        double b = AS_FLOAT(*(--sp));
        double a = AS_FLOAT(*(--sp));
        *sp++ = ((Value){VAL_FLOAT, .floatVal = a + b);
        NEXT();
    }
    
    op_sub_ff: {
        double b = AS_FLOAT(*(--sp));
        double a = AS_FLOAT(*(--sp));
        *sp++ = ((Value){VAL_FLOAT, .floatVal = a - b);
        NEXT();
    }
    
    op_mul_ff: {
        double b = AS_FLOAT(*(--sp));
        double a = AS_FLOAT(*(--sp));
        *sp++ = ((Value){VAL_FLOAT, .floatVal = a * b);
        NEXT();
    }
    
    op_div_ff: {
        double b = AS_FLOAT(*(--sp));
        double a = AS_FLOAT(*(--sp));
        *sp++ = ((Value){VAL_FLOAT, .floatVal = a / b);
        NEXT();
    }
    
    op_neg_f: {
        sp[-1] = ((Value){VAL_FLOAT, .floatVal = -AS_FLOAT(sp[-1]));
        NEXT();
    }
    
    // === GENERIC ARITHMETIC (with type checks) ===
    op_add: {
        Value b = *(--sp);
        Value a = *(--sp);
        if (IS_INT(a) && IS_INT(b)) {
            *sp++ = ((Value){VAL_INT, .intVal = AS_INT(a) + AS_INT(b));
        } else if (IS_FLOAT(a) && IS_FLOAT(b)) {
            *sp++ = ((Value){VAL_FLOAT, .floatVal = AS_FLOAT(a) + AS_FLOAT(b));
        }
        NEXT();
    }
    
    op_sub: {
        Value b = *(--sp);
        Value a = *(--sp);
        if (IS_INT(a) && IS_INT(b)) {
            *sp++ = ((Value){VAL_INT, .intVal = AS_INT(a) - AS_INT(b));
        }
        NEXT();
    }
    
    op_mul: {
        Value b = *(--sp);
        Value a = *(--sp);
        if (IS_INT(a) && IS_INT(b)) {
            *sp++ = ((Value){VAL_INT, .intVal = AS_INT(a) * AS_INT(b));
        }
        NEXT();
    }
    
    op_div: {
        Value b = *(--sp);
        Value a = *(--sp);
        if (IS_INT(a) && IS_INT(b)) {
            *sp++ = ((Value){VAL_INT, .intVal = AS_INT(a) / AS_INT(b));
        }
        NEXT();
    }
    
    op_mod: {
        Value b = *(--sp);
        Value a = *(--sp);
        if (IS_INT(a) && IS_INT(b)) {
            *sp++ = ((Value){VAL_INT, .intVal = AS_INT(a) % AS_INT(b));
        }
        NEXT();
    }
    
    op_neg: {
        if (IS_INT(sp[-1])) {
            sp[-1] = ((Value){VAL_INT, .intVal = -AS_INT(sp[-1]));
        }
        NEXT();
    }
    
    // === SPECIALIZED COMPARISONS (INT) ===
    op_lt_ii: {
        int64_t b = AS_INT(*(--sp));
        int64_t a = AS_INT(*(--sp));
        *sp++ = ((Value){VAL_BOOL, .boolVal = a < b);
        NEXT();
    }
    
    op_le_ii: {
        int64_t b = AS_INT(*(--sp));
        int64_t a = AS_INT(*(--sp));
        *sp++ = ((Value){VAL_BOOL, .boolVal = a <= b);
        NEXT();
    }
    
    op_gt_ii: {
        int64_t b = AS_INT(*(--sp));
        int64_t a = AS_INT(*(--sp));
        *sp++ = ((Value){VAL_BOOL, .boolVal = a > b);
        NEXT();
    }
    
    op_ge_ii: {
        int64_t b = AS_INT(*(--sp));
        int64_t a = AS_INT(*(--sp));
        *sp++ = ((Value){VAL_BOOL, .boolVal = a >= b);
        NEXT();
    }
    
    op_eq_ii: {
        int64_t b = AS_INT(*(--sp));
        int64_t a = AS_INT(*(--sp));
        *sp++ = ((Value){VAL_BOOL, .boolVal = a == b);
        NEXT();
    }
    
    op_ne_ii: {
        int64_t b = AS_INT(*(--sp));
        int64_t a = AS_INT(*(--sp));
        *sp++ = ((Value){VAL_BOOL, .boolVal = a != b);
        NEXT();
    }
    
    // === FLOAT COMPARISONS (Stubs) ===
    op_lt_ff: op_le_ff: op_gt_ff: op_ge_ff: op_eq_ff: op_ne_ff:
        sp -= 2;
        *sp++ = ((Value){VAL_BOOL, .boolVal = false);
        NEXT();
    
    // === GENERIC COMPARISONS ===
    op_lt: {
        Value b = *(--sp);
        Value a = *(--sp);
        if (IS_INT(a) && IS_INT(b)) {
            *sp++ = ((Value){VAL_BOOL, .boolVal = AS_INT(a) < AS_INT(b));
        }
        NEXT();
    }
    
    op_le: op_gt: op_ge: op_eq: op_ne:
        sp -= 2;
        *sp++ = ((Value){VAL_BOOL, .boolVal = false);
        NEXT();
    
    // === LOGICAL ===
    op_not: {
        sp[-1] = ((Value){VAL_BOOL, .boolVal = IS_INT(sp[-1]) && AS_INT(sp[-1]) == 0);
        NEXT();
    }
    
    op_and: op_or:
        NEXT();
    
    // === CONTROL FLOW ===
    op_jump: {
        uint16_t offset = (*ip++) << 8;
        offset |= *ip++;
        ip += offset;
        NEXT();
    }
    
    op_jump_if_false: {
        uint16_t offset = (*ip++) << 8;
        offset |= *ip++;
        Value cond = sp[-1];
        if (IS_INT(cond) && AS_INT(cond) == 0) {
            ip += offset;
        }
        NEXT();
    }
    
    op_jump_if_true: {
        uint16_t offset = (*ip++) << 8;
        offset |= *ip++;
        Value cond = sp[-1];
        if (IS_INT(cond) && AS_INT(cond) != 0) {
            ip += offset;
        }
        NEXT();
    }
    
    op_loop: {
        uint16_t offset = (*ip++) << 8;
        offset |= *ip++;
        ip -= offset;
        NEXT();
    }
    
    op_loop_header: {
        // Loop header marker - JIT uses this
        NEXT();
    }
    
    // === FUNCTION CALLS (Stubs) ===
    op_call: op_call_0: op_call_1: op_call_2: op_return: op_return_nil:
        NEXT();
    
    // === OBJECTS (Stubs) ===
    op_load_property: op_store_property: op_load_index: op_store_index:
    op_new_array: op_new_map: op_new_object:
    op_array_push: op_array_pop: op_array_len:
        NEXT();
    
    // === JIT INTEGRATION ===
    op_hotspot_check: {
        // Increment hotspot counter
        int offset = (int)(ip - chunk->code - 1);
        if (offset >= 0 && offset < chunk->hotspotCapacity) {
            chunk->hotspots[offset]++;
        }
        NEXT();
    }
    
    op_osr_entry: op_compiled_call: op_deopt:
        NEXT();
    
    // === SPECIAL ===
    op_print: {
        Value val = *(--sp);
        if (IS_INT(val)) {
            printf("%ld\n", AS_INT(val));
        } else if (IS_BOOL(val)) {
            printf("%s\n", AS_BOOL(val) ? "true" : "false");
        }
        NEXT();
    }
    
    op_halt: {
        goto done;
    }
    
    op_nop: {
        NEXT();
    }
    
#else
    #error "Computed goto not supported. Use GCC or Clang."
#endif
    
done:
    uint64_t elapsed = getMicroseconds() - startTime;
    
    // Print performance stats
    if (instructionCount > 0) {
        double seconds = elapsed / 1000000.0;
        double mops = (instructionCount / 1000000.0) / seconds;
        printf("\n[Bytecode VM] Executed %lu instructions in %.6f seconds\n",
               instructionCount, seconds);
        printf("[Bytecode VM] Performance: %.2f million ops/sec\n", mops);
    }
    
    return elapsed;
}
