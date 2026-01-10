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
    (void)vm; // Silence unused parameter warning for now (used later for Alloc/GC)
    
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
    
    // #define TRACE_EXECUTION 1
    #ifdef TRACE_EXECUTION
        #define DISPATCH() do { \
            instructionCount++; \
            const OpcodeInfo* info = getOpcodeInfo(*ip); \
            printf("Exec: %s (StackDepth: %ld)\n", info ? info->name : "???", sp - stack); fflush(stdout); \
            goto *dispatchTable[*ip++]; \
        } while(0)
    #else
        #define DISPATCH() do { instructionCount++; goto *dispatchTable[*ip++]; } while(0)
    #endif
    #define NEXT() DISPATCH()
    
    DISPATCH();  // Start execution
    
    // === STACK OPERATIONS ===
    op_load_imm: {
        // Load 32-bit immediate (4 bytes)
        int32_t value = (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | ip[3];
        *sp++ = ((Value){VAL_INT, .intVal = value});
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
    
    // === GLOBAL VARIABLES ===
    op_load_global: {
        uint8_t constIdx = *ip++;
        ObjString* name = AS_STRING(chunk->constants[constIdx]);
        unsigned int h = name->hash % TABLE_SIZE;
        VarEntry* entry = vm->globalEnv->buckets[h];
        while (entry) {
            if (entry->key == name->chars) {
                *sp++ = entry->value;
                goto global_loaded;
            }
            entry = entry->next;
        }
        // Not found - standard Lua/JS behavior is nil or error. Unnarize strictness says error.
        printf("Runtime Error: Undefined global variable '%s'\n", name->chars);
        exit(1);

        global_loaded:
        NEXT();
    }
    
    op_store_global: {
        uint8_t constIdx = *ip++;
        ObjString* name = AS_STRING(chunk->constants[constIdx]);
        unsigned int h = name->hash % TABLE_SIZE;
        VarEntry* entry = vm->globalEnv->buckets[h];
        while (entry) {
            if (entry->key == name->chars) {
                entry->value = sp[-1]; // Peek, assignment evaluates to value
                goto global_stored;
            }
            entry = entry->next;
        }
        // Not found, assume implicit declaration (or error if strict)
        // Auto-define (fallback)
        
        // Insert new
        VarEntry* newEntry = malloc(sizeof(VarEntry));
        newEntry->key = name->chars;
        newEntry->keyLength = name->length;
        newEntry->ownsKey = false;
        newEntry->value = sp[-1];
        newEntry->next = vm->globalEnv->buckets[h];
        vm->globalEnv->buckets[h] = newEntry;
        
        global_stored:
        NEXT();
    }
    
    op_define_global: {
        uint8_t constIdx = *ip++;
        ObjString* name = AS_STRING(chunk->constants[constIdx]);
        unsigned int h = name->hash % TABLE_SIZE;
        
        // Check if exists (update) or new
        VarEntry* entry = vm->globalEnv->buckets[h];
        while (entry) {
            if (entry->key == name->chars) {
                entry->value = sp[-1];
                NEXT();
            }
            entry = entry->next;
        }

        // Insert new
        VarEntry* newEntry = malloc(sizeof(VarEntry));
        newEntry->key = name->chars;
        newEntry->keyLength = name->length;
        newEntry->ownsKey = false; 
        newEntry->value = sp[-1];
        newEntry->next = vm->globalEnv->buckets[h];
        vm->globalEnv->buckets[h] = newEntry;
        
        NEXT();
    }
    
    // === SPECIALIZED ARITHMETIC (INT) ===
    op_add_ii: {
        int64_t b = AS_INT(*(--sp));
        int64_t a = AS_INT(*(--sp));
        *sp++ = ((Value){VAL_INT, .intVal = a + b});
        NEXT();
    }
    
    op_sub_ii: {
        int64_t b = AS_INT(*(--sp));
        int64_t a = AS_INT(*(--sp));
        *sp++ = ((Value){VAL_INT, .intVal = a - b});
        NEXT();
    }
    
    op_mul_ii: {
        int64_t b = AS_INT(*(--sp));
        int64_t a = AS_INT(*(--sp));
        *sp++ = ((Value){VAL_INT, .intVal = a * b});
        NEXT();
    }
    
    op_div_ii: {
        int64_t b = AS_INT(*(--sp));
        int64_t a = AS_INT(*(--sp));
        *sp++ = ((Value){VAL_INT, .intVal = a / b});
        NEXT();
    }
    
    op_mod_ii: {
        int64_t b = AS_INT(*(--sp));
        int64_t a = AS_INT(*(--sp));
        *sp++ = ((Value){VAL_INT, .intVal = a % b});
        NEXT();
    }
    
    op_neg_i: {
        sp[-1] = ((Value){VAL_INT, .intVal = -AS_INT(sp[-1])});
        NEXT();
    }
    
    // === SPECIALIZED ARITHMETIC (FLOAT) ===
    op_add_ff: {
        double b = AS_FLOAT(*(--sp));
        double a = AS_FLOAT(*(--sp));
        *sp++ = ((Value){VAL_FLOAT, .floatVal = a + b});
        NEXT();
    }
    
    op_sub_ff: {
        double b = AS_FLOAT(*(--sp));
        double a = AS_FLOAT(*(--sp));
        *sp++ = ((Value){VAL_FLOAT, .floatVal = a - b});
        NEXT();
    }
    
    op_mul_ff: {
        double b = AS_FLOAT(*(--sp));
        double a = AS_FLOAT(*(--sp));
        *sp++ = ((Value){VAL_FLOAT, .floatVal = a * b});
        NEXT();
    }
    
    op_div_ff: {
        double b = AS_FLOAT(*(--sp));
        double a = AS_FLOAT(*(--sp));
        *sp++ = ((Value){VAL_FLOAT, .floatVal = a / b});
        NEXT();
    }
    
    op_neg_f: {
        sp[-1] = ((Value){VAL_FLOAT, .floatVal = -AS_FLOAT(sp[-1])});
        NEXT();
    }
    
    // === GENERIC ARITHMETIC (with type checks) ===
    op_add: {
        Value b = *(--sp);
        Value a = *(--sp);
        
        // Int + Int
        if (IS_INT(a) && IS_INT(b)) {
            *sp++ = ((Value){VAL_INT, .intVal = AS_INT(a) + AS_INT(b)});
        } 
        // Float + Float
        else if (IS_FLOAT(a) && IS_FLOAT(b)) {
            *sp++ = ((Value){VAL_FLOAT, .floatVal = AS_FLOAT(a) + AS_FLOAT(b)});
        }
        // String concatenation
        else if (IS_STRING(a) || IS_STRING(b)) {
            // Convert both to strings and concatenate
            char bufferA[64], bufferB[64];
            const char* strA;
            const char* strB;
            
            // Convert a to string
            if (IS_STRING(a)) {
                strA = AS_CSTRING(a);
            } else if (IS_INT(a)) {
                snprintf(bufferA, sizeof(bufferA), "%ld", AS_INT(a));
                strA = bufferA;
            } else if (IS_FLOAT(a)) {
                snprintf(bufferA, sizeof(bufferA), "%g", AS_FLOAT(a));
                strA = bufferA;
            } else if (IS_BOOL(a)) {
                strA = AS_BOOL(a) ? "true" : "false";
            } else if (IS_NIL(a)) {
                strA = "nil";
            } else {
                strA = "[object]";
            }
            
            // Convert b to string
            if (IS_STRING(b)) {
                strB = AS_CSTRING(b);
            } else if (IS_INT(b)) {
                snprintf(bufferB, sizeof(bufferB), "%ld", AS_INT(b));
                strB = bufferB;
            } else if (IS_FLOAT(b)) {
                snprintf(bufferB, sizeof(bufferB), "%g", AS_FLOAT(b));
                strB = bufferB;
            } else if (IS_BOOL(b)) {
                strB = AS_BOOL(b) ? "true" : "false";
            } else if (IS_NIL(b)) {
                strB = "nil";
            } else {
                strB = "[object]";
            }
            
            // Concatenate
            int lenA = strlen(strA);
            int lenB = strlen(strB);
            int totalLen = lenA + lenB;
            
            char* result = malloc(totalLen + 1);
            memcpy(result, strA, lenA);
            memcpy(result + lenA, strB, lenB);
            result[totalLen] = '\0';
            
            // Intern the result string
            ObjString* str = internString(vm, result, totalLen);
            free(result);
            
            *sp++ = OBJ_VAL(str);
        }
        // Fallback: nil
        else {
            *sp++ = NIL_VAL;
        }
        NEXT();
    }
    
    op_sub: {
        Value b = *(--sp);
        Value a = *(--sp);
        if (IS_INT(a) && IS_INT(b)) {
            *sp++ = ((Value){VAL_INT, .intVal = AS_INT(a) - AS_INT(b)});
        }
        NEXT();
    }
    
    op_mul: {
        Value b = *(--sp);
        Value a = *(--sp);
        if (IS_INT(a) && IS_INT(b)) {
            *sp++ = ((Value){VAL_INT, .intVal = AS_INT(a) * AS_INT(b)});
        }
        NEXT();
    }
    
    op_div: {
        Value b = *(--sp);
        Value a = *(--sp);
        if (IS_INT(a) && IS_INT(b)) {
            *sp++ = ((Value){VAL_INT, .intVal = AS_INT(a) / AS_INT(b)});
        }
        NEXT();
    }
    
    op_mod: {
        Value b = *(--sp);
        Value a = *(--sp);
        if (IS_INT(a) && IS_INT(b)) {
            *sp++ = ((Value){VAL_INT, .intVal = AS_INT(a) % AS_INT(b)});
        }
        NEXT();
    }
    
    op_neg: {
        if (IS_INT(sp[-1])) {
            sp[-1] = ((Value){VAL_INT, .intVal = -AS_INT(sp[-1])});
        }
        NEXT();
    }
    
    // === SPECIALIZED COMPARISONS (INT) ===
    op_lt_ii: {
        int64_t b = AS_INT(*(--sp));
        int64_t a = AS_INT(*(--sp));
        *sp++ = ((Value){VAL_BOOL, .boolVal = a < b});
        NEXT();
    }
    
    op_le_ii: {
        int64_t b = AS_INT(*(--sp));
        int64_t a = AS_INT(*(--sp));
        *sp++ = ((Value){VAL_BOOL, .boolVal = a <= b});
        NEXT();
    }
    
    op_gt_ii: {
        int64_t b = AS_INT(*(--sp));
        int64_t a = AS_INT(*(--sp));
        *sp++ = ((Value){VAL_BOOL, .boolVal = a > b});
        NEXT();
    }
    
    op_ge_ii: {
        int64_t b = AS_INT(*(--sp));
        int64_t a = AS_INT(*(--sp));
        *sp++ = ((Value){VAL_BOOL, .boolVal = a >= b});
        NEXT();
    }
    
    op_eq_ii: {
        int64_t b = AS_INT(*(--sp));
        int64_t a = AS_INT(*(--sp));
        *sp++ = ((Value){VAL_BOOL, .boolVal = a == b});
        NEXT();
    }
    
    op_ne_ii: {
        int64_t b = AS_INT(*(--sp));
        int64_t a = AS_INT(*(--sp));
        *sp++ = ((Value){VAL_BOOL, .boolVal = a != b});
        NEXT();
    }
    
    // === FLOAT COMPARISONS ===
    op_lt_ff: {
        double b = AS_FLOAT(*(--sp));
        double a = AS_FLOAT(*(--sp));
        *sp++ = ((Value){VAL_BOOL, .boolVal = a < b});
        NEXT();
    }
    
    op_le_ff: {
        double b = AS_FLOAT(*(--sp));
        double a = AS_FLOAT(*(--sp));
        *sp++ = ((Value){VAL_BOOL, .boolVal = a <= b});
        NEXT();
    }
    
    op_gt_ff: {
        double b = AS_FLOAT(*(--sp));
        double a = AS_FLOAT(*(--sp));
        *sp++ = ((Value){VAL_BOOL, .boolVal = a > b});
        NEXT();
    }
    
    op_ge_ff: {
        double b = AS_FLOAT(*(--sp));
        double a = AS_FLOAT(*(--sp));
        *sp++ = ((Value){VAL_BOOL, .boolVal = a >= b});
        NEXT();
    }
    
    op_eq_ff: {
        double b = AS_FLOAT(*(--sp));
        double a = AS_FLOAT(*(--sp));
        *sp++ = ((Value){VAL_BOOL, .boolVal = a == b});
        NEXT();
    }
    
    op_ne_ff: {
        double b = AS_FLOAT(*(--sp));
        double a = AS_FLOAT(*(--sp));
        *sp++ = ((Value){VAL_BOOL, .boolVal = a != b});
        NEXT();
    }
    
    // === GENERIC COMPARISONS ===
    op_lt: {
        Value b = *(--sp);
        Value a = *(--sp);
        if (IS_INT(a) && IS_INT(b)) {
            *sp++ = ((Value){VAL_BOOL, .boolVal = AS_INT(a) < AS_INT(b)});
        } else if (IS_FLOAT(a) && IS_FLOAT(b)) {
            *sp++ = ((Value){VAL_BOOL, .boolVal = AS_FLOAT(a) < AS_FLOAT(b)});
        } else {
            *sp++ = ((Value){VAL_BOOL, .boolVal = false});
        }
        NEXT();
    }
    
    op_le: {
        Value b = *(--sp);
        Value a = *(--sp);
        if (IS_INT(a) && IS_INT(b)) {
            *sp++ = ((Value){VAL_BOOL, .boolVal = AS_INT(a) <= AS_INT(b)});
        } else if (IS_FLOAT(a) && IS_FLOAT(b)) {
            *sp++ = ((Value){VAL_BOOL, .boolVal = AS_FLOAT(a) <= AS_FLOAT(b)});
        } else {
            *sp++ = ((Value){VAL_BOOL, .boolVal = false});
        }
        NEXT();
    }
    
    op_gt: {
        Value b = *(--sp);
        Value a = *(--sp);
        if (IS_INT(a) && IS_INT(b)) {
            *sp++ = ((Value){VAL_BOOL, .boolVal = AS_INT(a) > AS_INT(b)});
        } else if (IS_FLOAT(a) && IS_FLOAT(b)) {
            *sp++ = ((Value){VAL_BOOL, .boolVal = AS_FLOAT(a) > AS_FLOAT(b)});
        } else {
            *sp++ = ((Value){VAL_BOOL, .boolVal = false});
        }
        NEXT();
    }
    
    op_ge: {
        Value b = *(--sp);
        Value a = *(--sp);
        if (IS_INT(a) && IS_INT(b)) {
            *sp++ = ((Value){VAL_BOOL, .boolVal = AS_INT(a) >= AS_INT(b)});
        } else if (IS_FLOAT(a) && IS_FLOAT(b)) {
            *sp++ = ((Value){VAL_BOOL, .boolVal = AS_FLOAT(a) >= AS_FLOAT(b)});
        } else {
            *sp++ = ((Value){VAL_BOOL, .boolVal = false});
        }
        NEXT();
    }
    
    op_eq: {
        Value b = *(--sp);
        Value a = *(--sp);
        if (IS_INT(a) && IS_INT(b)) {
            *sp++ = ((Value){VAL_BOOL, .boolVal = AS_INT(a) == AS_INT(b)});
        } else if (IS_FLOAT(a) && IS_FLOAT(b)) {
            *sp++ = ((Value){VAL_BOOL, .boolVal = AS_FLOAT(a) == AS_FLOAT(b)});
        } else if (IS_BOOL(a) && IS_BOOL(b)) {
            *sp++ = ((Value){VAL_BOOL, .boolVal = AS_BOOL(a) == AS_BOOL(b)});
        } else if (IS_NIL(a) && IS_NIL(b)) {
            *sp++ = ((Value){VAL_BOOL, .boolVal = true});
        } else {
            *sp++ = ((Value){VAL_BOOL, .boolVal = false});
        }
        NEXT();
    }
    
    op_ne: {
        Value b = *(--sp);
        Value a = *(--sp);
        if (IS_INT(a) && IS_INT(b)) {
            *sp++ = ((Value){VAL_BOOL, .boolVal = AS_INT(a) != AS_INT(b)});
        } else if (IS_FLOAT(a) && IS_FLOAT(b)) {
            *sp++ = ((Value){VAL_BOOL, .boolVal = AS_FLOAT(a) != AS_FLOAT(b)});
        } else if (IS_BOOL(a) && IS_BOOL(b)) {
            *sp++ = ((Value){VAL_BOOL, .boolVal = AS_BOOL(a) != AS_BOOL(b)});
        } else if (IS_NIL(a) && IS_NIL(b)) {
            *sp++ = ((Value){VAL_BOOL, .boolVal = false});
        } else {
            *sp++ = ((Value){VAL_BOOL, .boolVal = true});
        }
        NEXT();
    }
    
    // === LOGICAL ===
    op_not: {
        sp[-1] = ((Value){VAL_BOOL, .boolVal = IS_INT(sp[-1]) && AS_INT(sp[-1]) == 0});
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
        Value cond = sp[-1];  // Peek (compiler will emit OP_POP)
        
        // Truthiness: false if: int(0), bool(false), nil
        bool isFalse = false;
        if (IS_NIL(cond)) {
            isFalse = true;
        } else if (IS_BOOL(cond)) {
            isFalse = !AS_BOOL(cond);
        } else if (IS_INT(cond)) {
            isFalse = (AS_INT(cond) == 0);
        } else if (IS_FLOAT(cond)) {
            isFalse = (AS_FLOAT(cond) == 0.0);
        }
        
        if (isFalse) {
            ip += offset;
        }
        NEXT();
    }
    
    op_jump_if_true: {
        uint16_t offset = (*ip++) << 8;
        offset |= *ip++;
        Value cond = sp[-1];  // Peek (compiler will emit OP_POP)
        
        // Truthiness: true if: int(!=0), bool(true), any object
        bool isTrue = false;
        if (IS_BOOL(cond)) {
            isTrue = AS_BOOL(cond);
        } else if (IS_INT(cond)) {
            isTrue = (AS_INT(cond) != 0);
        } else if (IS_FLOAT(cond)) {
            isTrue = (AS_FLOAT(cond) != 0.0);
        } else if (IS_OBJ(cond)) {
            isTrue = true;  // Objects are truthy
        }
        
        if (isTrue) {
            ip += offset;
        }
        NEXT();
    }
    
    op_loop: {
        uint16_t offset = (*ip++) << 8;
        offset |= *ip++;
        // printf("Looping back by %d bytes. Current IP: %p, Target IP: %p\n", offset, ip, ip - offset);
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
        } else if (IS_STRING(val)) {
            printf("%s\n", AS_CSTRING(val));
        } else if (IS_NIL(val)) {
            printf("nil\n");
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
