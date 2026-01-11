#include "bytecode/interpreter.h"
#include "bytecode/opcodes.h"
#include "jit/jit_compiler.h"  // For JIT compilation
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
    
    // Stack machine (Use VM stack properties)
    Value* sp = vm->stack + vm->stackTop;
    Value* fp = vm->stack + vm->fp;
    
    // Code pointer
    register uint8_t* ip = chunk->code;  // Instruction pointer in register
    uint8_t argCount; // For call opcodes

    // FULL JIT MODE: Compile function at entry
    // Only if explicitly enabled via flag
    if (vm->jitEnabled && !chunk->isCompiled) {
        JITFunction* jitFunc = compileFunction(vm, chunk);
        if (jitFunc) {
            if (!chunk->jitCode) chunk->jitCode = malloc(sizeof(void*));
            chunk->jitCode[0] = jitFunc;
            chunk->isCompiled = true;
            chunk->tierLevel = 1;
            vm->jitCompilations++;
        }
    }
    
    // Execute JIT if compiled
    if (chunk->isCompiled && chunk->jitCode && chunk->jitCode[0]) {
        vm->jitExecutions++;
        Value result = executeJIT(vm, (JITFunction*)chunk->jitCode[0]);
        *sp++ = result;
        return instructionCount;
    }
    
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
        &&op_struct_def, // NEW
        &&op_array_push, &&op_array_pop, &&op_array_len,
        &&op_hotspot_check, &&op_osr_entry, &&op_compiled_call, &&op_deopt,
        &&op_print, &&op_halt, &&op_nop,
        &&op_array_push_clean
    };
    
    // #define TRACE_EXECUTION 1
    #ifdef TRACE_EXECUTION
        #define DISPATCH() do { \
            instructionCount++; \
            const OpcodeInfo* info = getOpcodeInfo(*ip); \
            printf("Exec: %s (StackDepth: %ld)\n", info ? info->name : "???", sp - vm->stack); fflush(stdout); \
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
        *sp++ = INT_VAL(value);
        ip += 4;
        NEXT();
    }
    
    op_load_const: {
        uint8_t constant = *ip++;
        *sp++ = chunk->constants[constant];
        NEXT();
    }
    
    op_load_nil: {
        Value nil = NIL_VAL;
        *sp++ = nil;
        NEXT();
    }
    
    op_load_true: {
        Value v = BOOL_VAL(true);
        *sp++ = v;
        NEXT();
    }
    
    op_load_false: {
        Value v = BOOL_VAL(false);
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
        *sp++ = fp[slot];
        NEXT();
    }
    
    op_store_local: {
        uint8_t slot = *ip++;
        fp[slot] = sp[-1]; 
        NEXT();
    }
    

    op_load_local_0: {
        *sp++ = fp[0];
        NEXT();
    }
    
    op_load_local_1: {
        *sp++ = fp[1];
        NEXT();
    }
    
    op_load_local_2: {
        *sp++ = fp[2];
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
        // Not found
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
                entry->value = sp[-1]; 
                goto global_stored;
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
        
        global_stored:
        NEXT();
    }
    
    op_define_global: {
        uint8_t constIdx = *ip++;
        ObjString* name = AS_STRING(chunk->constants[constIdx]);
        unsigned int h = name->hash % TABLE_SIZE;
        
        // Define or Update
        VarEntry* entry = vm->globalEnv->buckets[h];
        while (entry) {
            if (entry->key == name->chars) {
                entry->value = sp[-1];
                goto global_defined;
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
        
        global_defined:
        NEXT();
    }
    
    // === SPECIALIZED ARITHMETIC (INT) ===
    op_add_ii: {
        int64_t b = AS_INT(*(--sp));
        int64_t a = AS_INT(*(--sp));
        *sp++ = INT_VAL(a + b);
        NEXT();
    }
    
    op_sub_ii: {
        int64_t b = AS_INT(*(--sp));
        int64_t a = AS_INT(*(--sp));
        *sp++ = INT_VAL(a - b);
        NEXT();
    }
    
    op_mul_ii: {
        int64_t b = AS_INT(*(--sp));
        int64_t a = AS_INT(*(--sp));
        *sp++ = INT_VAL(a * b);
        NEXT();
    }
    
    op_div_ii: {
        int64_t b = AS_INT(*(--sp));
        int64_t a = AS_INT(*(--sp));
        *sp++ = INT_VAL(a / b);
        NEXT();
    }
    
    op_mod_ii: {
        int64_t b = AS_INT(*(--sp));
        int64_t a = AS_INT(*(--sp));
        *sp++ = INT_VAL(a % b);
        NEXT();
    }
    
    op_neg_i: {
        sp[-1] = INT_VAL(-AS_INT(sp[-1]));
        NEXT();
    }
    
    // === SPECIALIZED ARITHMETIC (FLOAT) ===
    op_add_ff: {
        double b = AS_FLOAT(*(--sp));
        double a = AS_FLOAT(*(--sp));
        *sp++ = FLOAT_VAL(a + b);
        NEXT();
    }
    
    op_sub_ff: {
        double b = AS_FLOAT(*(--sp));
        double a = AS_FLOAT(*(--sp));
        *sp++ = FLOAT_VAL(a - b);
        NEXT();
    }
    
    op_mul_ff: {
        double b = AS_FLOAT(*(--sp));
        double a = AS_FLOAT(*(--sp));
        *sp++ = FLOAT_VAL(a * b);
        NEXT();
    }
    
    op_div_ff: {
        double b = AS_FLOAT(*(--sp));
        double a = AS_FLOAT(*(--sp));
        *sp++ = FLOAT_VAL(a / b);
        NEXT();
    }
    
    op_neg_f: {
        sp[-1] = FLOAT_VAL(-AS_FLOAT(sp[-1]));
        NEXT();
    }
    
    // === GENERIC ARITHMETIC (with type checks) ===
    op_add: {
        Value b = sp[-1];
        Value a = sp[-2];
        
        // Int + Int
        if (IS_INT(a) && IS_INT(b)) {
            sp -= 2;
            *sp++ = INT_VAL(AS_INT(a) + AS_INT(b));
        } 
        // Float + Float (or mixed)
        else if (IS_NUMBER(a) && IS_NUMBER(b)) {
            double da = IS_INT(a) ? (double)AS_INT(a) : AS_FLOAT(a);
            double db = IS_INT(b) ? (double)AS_INT(b) : AS_FLOAT(b);
            sp -= 2;
            *sp++ = FLOAT_VAL(da + db);
        }
        // String concatenation
        else if (IS_STRING(a) || IS_STRING(b)) {
            // Sync stack for GC safety BEFORE any allocation
            vm->stackTop = (int)(sp - vm->stack);
            
            // Operands are still on stack, so they are rooted for GC
            char bufferA[64], bufferB[64];
            const char* strA;
            const char* strB;
            
            // Convert a to string
            if (IS_STRING(a)) {
                strA = AS_CSTRING(a);
            } else if (IS_INT(a)) {
                snprintf(bufferA, sizeof(bufferA), "%ld", (long)AS_INT(a));
                strA = bufferA;
            } else if (IS_FLOAT(a)) {
                snprintf(bufferA, sizeof(bufferA), "%g", AS_FLOAT(a));
                strA = bufferA;
            } else if (IS_BOOL(a)) {
                strA = AS_BOOL(a) ? "true" : "false";
            } else if (IS_NIL(a)) {
                strA = "nil";
            } else if (IS_ARRAY(a)) {
                // Convert array to string representation
                Array* arr = (Array*)AS_OBJ(a);
                char* arrStr = malloc(1024); // Temp buffer for array string
                int pos = 0;
                pos += snprintf(arrStr + pos, 1024 - pos, "[");
                for (int i = 0; i < arr->count && pos < 1000; i++) {
                    if (i > 0) pos += snprintf(arrStr + pos, 1024 - pos, ", ");
                    Value item = arr->items[i];
                    if (IS_INT(item)) {
                        pos += snprintf(arrStr + pos, 1024 - pos, "%ld", (long)AS_INT(item));
                    } else if (IS_STRING(item)) {
                        pos += snprintf(arrStr + pos, 1024 - pos, "%s", AS_CSTRING(item));
                    } else if (IS_BOOL(item)) {
                        pos += snprintf(arrStr + pos, 1024 - pos, "%s", AS_BOOL(item) ? "true" : "false");
                    }
                }
                snprintf(arrStr + pos, 1024 - pos, "]");
                strA = arrStr;
            } else {
                strA = "[object]";
            }
            
            // Convert b to string
            if (IS_STRING(b)) {
                strB = AS_CSTRING(b);
            } else if (IS_INT(b)) {
                snprintf(bufferB, sizeof(bufferB), "%ld", (long)AS_INT(b));
                strB = bufferB;
            } else if (IS_FLOAT(b)) {
                snprintf(bufferB, sizeof(bufferB), "%g", AS_FLOAT(b));
                strB = bufferB;
            } else if (IS_BOOL(b)) {
                strB = AS_BOOL(b) ? "true" : "false";
            } else if (IS_NIL(b)) {
                strB = "nil";
            } else if (IS_ARRAY(b)) {
                // Convert array to string representation
                Array* arr = (Array*)AS_OBJ(b);
                char* arrStr = malloc(1024); // Temp buffer for array string
                int pos = 0;
                pos += snprintf(arrStr + pos, 1024 - pos, "[");
                for (int i = 0; i < arr->count && pos < 1000; i++) {
                    if (i > 0) pos += snprintf(arrStr + pos, 1024 - pos, ", ");
                    Value item = arr->items[i];
                    if (IS_INT(item)) {
                        pos += snprintf(arrStr + pos, 1024 - pos, "%ld", (long)AS_INT(item));
                    } else if (IS_STRING(item)) {
                        pos += snprintf(arrStr + pos, 1024 - pos, "%s", AS_CSTRING(item));
                    } else if (IS_BOOL(item)) {
                        pos += snprintf(arrStr + pos, 1024 - pos, "%s", AS_BOOL(item) ? "true" : "false");
                    }
                }
                snprintf(arrStr + pos, 1024 - pos, "]");
                strB = arrStr;
            } else {
                strB = "[object]";
            }
            
            // Concatenate
            size_t lenA = strlen(strA);
            size_t lenB = strlen(strB);
            char* result = malloc(lenA + lenB + 1);
            memcpy(result, strA, lenA);
            memcpy(result + lenA, strB, lenB);
            result[lenA + lenB] = '\0';
            
            // Intern result string
            ObjString* str = internString(vm, result, lenA + lenB);
            
            // Free temp buffers if allocated
            if (IS_ARRAY(a) && strA != bufferA) free((void*)strA);
            if (IS_ARRAY(b) && strB != bufferB) free((void*)strB);
            free(result);
            
            sp -= 2;
            *sp++ = OBJ_VAL(str);
        }
        // Fallback: nil
        else {
            sp -= 2;
            *sp++ = NIL_VAL;
        }
        NEXT();
    }
    
    op_sub: {
        Value b = *(--sp);
        Value a = *(--sp);
        if (IS_INT(a) && IS_INT(b)) {
            *sp++ = INT_VAL(AS_INT(a) - AS_INT(b));
        } else {
            double da = IS_INT(a) ? (double)AS_INT(a) : AS_FLOAT(a);
            double db = IS_INT(b) ? (double)AS_INT(b) : AS_FLOAT(b);
            *sp++ = FLOAT_VAL(da - db);
        }
        NEXT();
    }
    
    op_mul: {
        Value b = *(--sp);
        Value a = *(--sp);
        if (IS_INT(a) && IS_INT(b)) {
            *sp++ = INT_VAL(AS_INT(a) * AS_INT(b));
        } else {
            double da = IS_INT(a) ? (double)AS_INT(a) : AS_FLOAT(a);
            double db = IS_INT(b) ? (double)AS_INT(b) : AS_FLOAT(b);
            *sp++ = FLOAT_VAL(da * db);
        }
        NEXT();
    }
    
    op_div: {
        Value b = *(--sp);
        Value a = *(--sp);
        if (IS_INT(a) && IS_INT(b)) {
             int ib = AS_INT(b);
             if (ib == 0) {
                 printf("Runtime Error: Division by zero.\n");
                 exit(1);
             }
             *sp++ = INT_VAL(AS_INT(a) / ib);
        } else {
            double da = IS_INT(a) ? (double)AS_INT(a) : AS_FLOAT(a);
            double db = IS_INT(b) ? (double)AS_INT(b) : AS_FLOAT(b);
            *sp++ = FLOAT_VAL(da / db);
        }
        NEXT();
    }
    
    op_mod: {
        Value b = *(--sp);
        Value a = *(--sp);
        if (IS_INT(a) && IS_INT(b)) {
            *sp++ = INT_VAL(AS_INT(a) % AS_INT(b));
        }
        NEXT();
    }
    
    op_neg: {
        if (IS_INT(sp[-1])) {
            sp[-1] = INT_VAL(-AS_INT(sp[-1]));
        } else if (IS_FLOAT(sp[-1])) {
            sp[-1] = FLOAT_VAL(-AS_FLOAT(sp[-1]));
        }
        NEXT();
    }
    
    // === SPECIALIZED COMPARISONS (INT) ===
    op_lt_ii: {
        int64_t b = AS_INT(*(--sp));
        int64_t a = AS_INT(*(--sp));
        *sp++ = BOOL_VAL(a < b);
        NEXT();
    }
    
    op_le_ii: {
        int64_t b = AS_INT(*(--sp));
        int64_t a = AS_INT(*(--sp));
        *sp++ = BOOL_VAL(a <= b);
        NEXT();
    }
    
    op_gt_ii: {
        int64_t b = AS_INT(*(--sp));
        int64_t a = AS_INT(*(--sp));
        *sp++ = BOOL_VAL(a > b);
        NEXT();
    }
    
    op_ge_ii: {
        int64_t b = AS_INT(*(--sp));
        int64_t a = AS_INT(*(--sp));
        *sp++ = BOOL_VAL(a >= b);
        NEXT();
    }
    
    op_eq_ii: {
        int64_t b = AS_INT(*(--sp));
        int64_t a = AS_INT(*(--sp));
        *sp++ = BOOL_VAL(a == b);
        NEXT();
    }
    
    op_ne_ii: {
        int64_t b = AS_INT(*(--sp));
        int64_t a = AS_INT(*(--sp));
        *sp++ = BOOL_VAL(a != b);
        NEXT();
    }
    
    // === FLOAT COMPARISONS ===
    op_lt_ff: {
        double b = AS_FLOAT(*(--sp));
        double a = AS_FLOAT(*(--sp));
        *sp++ = BOOL_VAL(a < b);
        NEXT();
    }
    
    op_le_ff: {
        double b = AS_FLOAT(*(--sp));
        double a = AS_FLOAT(*(--sp));
        *sp++ = BOOL_VAL(a <= b);
        NEXT();
    }
    
    op_gt_ff: {
        double b = AS_FLOAT(*(--sp));
        double a = AS_FLOAT(*(--sp));
        *sp++ = BOOL_VAL(a > b);
        NEXT();
    }
    
    op_ge_ff: {
        double b = AS_FLOAT(*(--sp));
        double a = AS_FLOAT(*(--sp));
        *sp++ = BOOL_VAL(a >= b);
        NEXT();
    }
    
    op_eq_ff: {
        double b = AS_FLOAT(*(--sp));
        double a = AS_FLOAT(*(--sp));
        *sp++ = BOOL_VAL(a == b);
        NEXT();
    }
    
    op_ne_ff: {
        double b = AS_FLOAT(*(--sp));
        double a = AS_FLOAT(*(--sp));
        *sp++ = BOOL_VAL(a != b);
        NEXT();
    }
    
    // === GENERIC COMPARISONS ===
    op_lt: {
        Value b = *(--sp);
        Value a = *(--sp);
        if (IS_INT(a) && IS_INT(b)) {
            *sp++ = BOOL_VAL(AS_INT(a) < AS_INT(b));
        } else if (IS_NUMBER(a) && IS_NUMBER(b)) {
            double da = IS_INT(a) ? (double)AS_INT(a) : AS_FLOAT(a);
            double db = IS_INT(b) ? (double)AS_INT(b) : AS_FLOAT(b);
            *sp++ = BOOL_VAL(da < db);
        } else {
            *sp++ = BOOL_VAL(false);
        }
        NEXT();
    }
    
    op_le: {
        Value b = *(--sp);
        Value a = *(--sp);
        if (IS_INT(a) && IS_INT(b)) {
            *sp++ = BOOL_VAL(AS_INT(a) <= AS_INT(b));
        } else if (IS_NUMBER(a) && IS_NUMBER(b)) {
            double da = IS_INT(a) ? (double)AS_INT(a) : AS_FLOAT(a);
            double db = IS_INT(b) ? (double)AS_INT(b) : AS_FLOAT(b);
            *sp++ = BOOL_VAL(da <= db);
        } else {
            *sp++ = BOOL_VAL(false);
        }
        NEXT();
    }
    
    op_gt: {
        Value b = *(--sp);
        Value a = *(--sp);
        if (IS_INT(a) && IS_INT(b)) {
            *sp++ = BOOL_VAL(AS_INT(a) > AS_INT(b));
        } else if (IS_NUMBER(a) && IS_NUMBER(b)) {
            double da = IS_INT(a) ? (double)AS_INT(a) : AS_FLOAT(a);
            double db = IS_INT(b) ? (double)AS_INT(b) : AS_FLOAT(b);
            *sp++ = BOOL_VAL(da > db);
        } else {
            *sp++ = BOOL_VAL(false);
        }
        NEXT();
    }
    
    op_ge: {
        Value b = *(--sp);
        Value a = *(--sp);
        if (IS_INT(a) && IS_INT(b)) {
            *sp++ = BOOL_VAL(AS_INT(a) >= AS_INT(b));
        } else if (IS_NUMBER(a) && IS_NUMBER(b)) {
            double da = IS_INT(a) ? (double)AS_INT(a) : AS_FLOAT(a);
            double db = IS_INT(b) ? (double)AS_INT(b) : AS_FLOAT(b);
            *sp++ = BOOL_VAL(da >= db);
        } else {
            *sp++ = BOOL_VAL(false);
        }
        NEXT();
    }
    
    op_eq: {
        Value b = *(--sp);
        Value a = *(--sp);
        if (IS_INT(a) && IS_INT(b)) {
            *sp++ = BOOL_VAL(AS_INT(a) == AS_INT(b));
        } else if (IS_FLOAT(a) && IS_FLOAT(b)) {
            *sp++ = BOOL_VAL(AS_FLOAT(a) == AS_FLOAT(b));
        } else if (IS_BOOL(a) && IS_BOOL(b)) {
            *sp++ = BOOL_VAL(AS_BOOL(a) == AS_BOOL(b));
        } else if (IS_NIL(a) && IS_NIL(b)) {
            *sp++ = BOOL_VAL(true);
        } else {
            *sp++ = BOOL_VAL(false);
        }
        NEXT();
    }
    
    op_ne: {
        Value b = *(--sp);
        Value a = *(--sp);
        if (IS_INT(a) && IS_INT(b)) {
            *sp++ = BOOL_VAL(AS_INT(a) != AS_INT(b));
        } else if (IS_FLOAT(a) && IS_FLOAT(b)) {
            *sp++ = BOOL_VAL(AS_FLOAT(a) != AS_FLOAT(b));
        } else if (IS_BOOL(a) && IS_BOOL(b)) {
            *sp++ = BOOL_VAL(AS_BOOL(a) != AS_BOOL(b));
        } else if (IS_NIL(a) && IS_NIL(b)) {
            *sp++ = BOOL_VAL(false);
        } else {
            *sp++ = BOOL_VAL(true);
        }
        NEXT();
    }
    
    // === LOGICAL ===
    op_not: {
        sp[-1] = BOOL_VAL(IS_INT(sp[-1]) && AS_INT(sp[-1]) == 0);
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
        uint16_t offset = (uint16_t)(chunk->code[(int)(ip - chunk->code)] << 8);
        offset |= chunk->code[(int)(ip - chunk->code) + 1];
        ip += 2;
        ip -= offset;
        NEXT();
    }
    
    op_loop_header: {
        // Hotspot detection for JIT compilation (Phase 2 - Full Native JIT)
        int offset = (int)(ip - chunk->code - 1);
        
        // DEBUG: Print offset calculation
        #ifdef DEBUG_LOOP_HEADER
        printf("DEBUG: OP_LOOP_HEADER offset=%d, capacity=%d, hasHotspots=%d\n", 
               offset, chunk->hotspotCapacity, chunk->hotspots != NULL);
        fflush(stdout);
        #endif
        
        if (chunk->hotspots && offset >= 0 && offset < chunk->hotspotCapacity) {
            chunk->hotspots[offset]++;
            
            // Check if we should JIT compile this function
            if (vm->jitEnabled && (int)chunk->hotspots[offset] >= vm->jitThreshold) {
                if (!chunk->isCompiled) {
                    // Compile entire function to native code
                    JITFunction* jitFunc = compileFunction(vm, chunk);
                    if (jitFunc) {
                        // Store compiled function
                        if (!chunk->jitCode) {
                            chunk->jitCode = malloc(sizeof(void*));
                        }
                        chunk->jitCode[0] = jitFunc;
                        chunk->isCompiled = true;
                        chunk->tierLevel = 1;
                        vm->jitCompilations++;
                        
                        // Switch to JIT execution immediately
                        vm->jitExecutions++;
                        Value result = executeJIT(vm, jitFunc);
                        *sp++ = result;
                        goto done;
                    }
                }
            }
        } else if (offset < 0 || offset >= chunk->hotspotCapacity) {
            // Safety: Invalid offset - this shouldn't happen
            printf("ERROR: OP_LOOP_HEADER invalid offset %d (capacity: %d)\n", 
                   offset, chunk->hotspotCapacity);
            fflush(stdout);
        }
        NEXT();
    }
    
    // === FUNCTION CALLS (Stubs) ===
    op_call: {
        argCount = *ip++;
        goto do_call;
    }
        
    op_call_0: {
        argCount = 0;
        goto do_call;
    }
        
    op_call_1: {
        argCount = 1;
        goto do_call;
    }
        
    op_call_2: {
        argCount = 2;
    }
        
    do_call: {
        Value funcVal = sp[-argCount - 1];
        if (!IS_OBJ(funcVal)) {
            printf("Runtime Error: Attempt to call non-function value. (Val Type: %d, VAL_OBJ=%d)\n", getValueType(funcVal), VAL_OBJ);
            exit(1);
        }
        
        Obj* obj = AS_OBJ(funcVal);
        // printf("DEBUG: Call object type: %d (Expected %d, OBJ_FUNCTION)\n", obj->type, OBJ_FUNCTION);
        if (obj->type == OBJ_FUNCTION) {
             Function* func = (Function*)obj;
             if (func->isNative) {
                 vm->stackTop = (int)(sp - vm->stack); // Sync stack for GC
                 Value result = func->native(vm, sp - argCount, argCount);
                 sp -= argCount + 1; 
                 *sp++ = result;
                 // vm->stackTop is potentially modified by native (e.g. if it uses stack)
                 // But typically native returns value and we manage stack.
                 // We don't need to reload sp from stackTop unless native messed with it uniquely?
                 // Standard native contract: uses args pointer, returns Value.
                 NEXT();
             } else {
                 if (argCount != func->paramCount) {
                     printf("Runtime Error: Argument mismatch. Expected %d but got %d.\n", func->paramCount, argCount);
                     exit(1);
                 }
                 
                 if (vm->callStackTop >= 64) { // hardcoded for now or use CALL_STACK_MAX
                     printf("Runtime Error: Stack overflow.\n");
                     exit(1);
                 }
                 
                 CallFrame* frame = &vm->callStack[vm->callStackTop++];
                 frame->ip = ip;
                 frame->chunk = chunk;
                 frame->fp = vm->fp; 
                 frame->function = func; // Root the function 
                 
                 // Fix: Compiler expects slot 0 to be the function/receiver.
                 // So FP must point to the Function object on stack (sp - argCount - 1).
                 vm->fp = (int)((sp - argCount - 1) - vm->stack);
                 fp = vm->stack + vm->fp;
                 
                 chunk = func->bytecodeChunk;
                 ip = chunk->code;
                 NEXT();
             }
        } else if (obj->type == OBJ_STRUCT_DEF) {
             StructDef* def = (StructDef*)obj;
             if (argCount != def->fieldCount) {
                 printf("Runtime Error: Struct constructor for '%s' expected %d arguments but got %d.\n", 
                        def->name, def->fieldCount, argCount);
                 exit(1);
             }
             
             vm->stackTop = (int)(sp - vm->stack); // Sync stack for GC
             StructInstance* inst = ALLOCATE_OBJ(vm, StructInstance, OBJ_STRUCT_INSTANCE);
             inst->def = def;
             inst->fields = malloc(sizeof(Value) * def->fieldCount);
             
             // Copy args to fields
             // Stack: [..., func, arg0, arg1, ..., argN] (sp points after argN)
             for (int i = 0; i < argCount; i++) {
                 inst->fields[i] = sp[-argCount + i];
             }
             
             sp -= argCount + 1; // Pop func and args
             *sp++ = OBJ_VAL(inst);
             NEXT();
        } else {
             printf("Runtime Error: Call on non-function object. (Type: %d)\n", obj->type);
             exit(1);
        }
    }

    op_return_nil: {
        Value val = NIL_VAL;
        *sp++ = val;
        goto do_return;
    }
        
    op_return:
    do_return: {
        Value retVal = *(--sp);
        
        if (vm->callStackTop == 0) {
            return instructionCount;
        }
        
        // Save pointer to current frame base (Callee args start)
        Value* calleeFp = vm->stack + vm->fp;
        
        vm->callStackTop--;
        CallFrame* frame = &vm->callStack[vm->callStackTop];
        vm->fp = frame->fp;
        fp = vm->stack + vm->fp;
        chunk = frame->chunk;
        ip = frame->ip;
        
        // Reset SP to where Function object was (CalleeFP - 1)
        sp = calleeFp - 1; 
        *sp++ = retVal;
        NEXT();
    }
    
    
    // === OBJECTS ===
    op_load_property: {
        Value nameVal = chunk->constants[*ip++];
        ObjString* name = AS_STRING(nameVal);
        Value objVal = sp[-1]; // Peek
        
        if (IS_OBJ(objVal)) {
            Obj* obj = AS_OBJ(objVal);
            
            // Struct Instance Handling
            if (obj->type == OBJ_STRUCT_INSTANCE) {
                StructInstance* inst = (StructInstance*)obj;
                bool found = false;
                for (int i = 0; i < inst->def->fieldCount; i++) {
                    if (strcmp(inst->def->fields[i], name->chars) == 0) {
                        sp[-1] = inst->fields[i]; // Replace instance with property value
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    sp[-1] = NIL_VAL; // Property not found, replace with NIL
                }
                NEXT();
            }
            // Module Handling
            else if (obj->type == OBJ_MODULE) {
                 Module* mod = (Module*)AS_OBJ(objVal);
                 unsigned int h = name->hash % TABLE_SIZE;
                 VarEntry* e = mod->env->buckets[h];
                 while (e) {
                     if (e->key == name->chars) { // Pointer equality safe
                         sp--; // Pop instance
                         *sp++ = e->value;
                         NEXT();
                     }
                     e = e->next;
                 }
            }
            else if (obj->type == OBJ_STRING) {
                 if (name->length == 6 && memcmp(name->chars, "length", 6) == 0) {
                     sp[-1] = INT_VAL(((ObjString*)obj)->length);
                     NEXT();
                 }
            }
        }
        
        printf("Runtime Error: Only instances and structs have properties. Type: %d, Name: %s\n", IS_OBJ(objVal) ? (int)AS_OBJ(objVal)->type : -1, name->chars);
        exit(1);
    }
    
    op_store_property: {
        Value val = *(--sp);      // Value to store
        Value nameVal = chunk->constants[*ip++]; // Name
        ObjString* name = AS_STRING(nameVal);
        Value objVal = sp[-1];    // Target object (peek)
        
        if (IS_OBJ(objVal)) {
            Obj* obj = AS_OBJ(objVal);
            
            if (obj->type == OBJ_STRUCT_INSTANCE) {
                StructInstance* inst = (StructInstance*)obj;
                int fieldIdx = -1;
                for (int i = 0; i < inst->def->fieldCount; i++) {
                    if (strcmp(inst->def->fields[i], name->chars) == 0) {
                        fieldIdx = i;
                        break;
                    }
                }
                if (fieldIdx != -1) {
                    inst->fields[fieldIdx] = val;
                } else {  printf("Runtime Error: Struct '%s' has no field '%s'.\n", inst->def->name, name->chars);
                    exit(1);
                }
                sp[-1] = val; // Replace instance with value
                *sp++ = val; // Push value back onto stack
                NEXT();
            }
        }
        printf("Runtime Error: Only instances and structs have properties.\n");
        exit(1);
    }
    
    op_load_index: {
        // target[index] - read
        Value index = *(--sp);
        Value target = *(--sp);
        
        if (IS_ARRAY(target) && IS_INT(index)) {
            Array* arr = (Array*)AS_OBJ(target);
            int idx = (int)AS_INT(index);
            
            if (idx >= 0 && idx < arr->count) {
                *sp++ = arr->items[idx];
            } else {
                // Out of bounds - return nil
                *sp++ = NIL_VAL;
            }
        } else if (IS_MAP(target)) {
            Map* map = (Map*)AS_OBJ(target);
            if (IS_STRING(index)) {
                ObjString* key = AS_STRING(index);
                int bucket;
                MapEntry* e = mapFindEntry(map, key->chars, key->length, &bucket);
                *sp++ = e ? e->value : NIL_VAL;
            } else if (IS_INT(index)) {
                int key = (int)AS_INT(index);
                int bucket;
                MapEntry* e = mapFindEntryInt(map, key, &bucket);
                *sp++ = e ? e->value : NIL_VAL;
            } else {
                *sp++ = NIL_VAL;
            }
        } else {
            *sp++ = NIL_VAL;
        }
        NEXT();
    }
    
    op_store_index: {
        // target[index] = value
        Value value = *(--sp);
        Value index = *(--sp);
        Value target = *(--sp);
        
        if (IS_ARRAY(target) && IS_INT(index)) {
            Array* arr = (Array*)AS_OBJ(target);
            int idx = (int)AS_INT(index);
            
            // Auto-grow array if needed
            if (idx >= 0) {
                if (idx >= arr->capacity) {
                    int newCap = idx + 1;
                    if (newCap < arr->capacity * 2) newCap = arr->capacity * 2;
                    arr->items = realloc(arr->items, newCap * sizeof(Value));
                    arr->capacity = newCap;
                }
                if (idx >= arr->count) {
                    // Fill gaps with nil
                    for (int i = arr->count; i < idx; i++) {
                        arr->items[i] = NIL_VAL;
                    }
                    arr->count = idx + 1;
                }
                arr->items[idx] = value;
            }
        } else if (IS_MAP(target)) {
            Map* map = (Map*)AS_OBJ(target);
            if (IS_STRING(index)) {
                ObjString* key = AS_STRING(index);
                mapSetStr(map, key->chars, key->length, value);
            } else if (IS_INT(index)) {
                mapSetInt(map, (int)AS_INT(index), value);
            }
        }
        // Map indexing handled separately
        *sp++ = value;  // Assignment evaluates to value
        NEXT();
    }
    
    op_new_array: {
        vm->stackTop = (int)(sp - vm->stack); // Sync stack for GC
        Array* arr = newArray(vm);
        *sp++ = OBJ_VAL(arr);
        NEXT();
    }
    
    op_new_map: {
        vm->stackTop = (int)(sp - vm->stack); // Sync stack for GC
        Map* m = newMap(vm);
        *sp++ = OBJ_VAL(m);
        NEXT();
    }
    
    op_new_object: {
        // TODO: Struct instantiation
        Value v = NIL_VAL;
        *sp++ = v;
        NEXT();
    }
    
    op_struct_def: {
        uint8_t fieldCount = *ip++;
        
        vm->stackTop = (int)(sp - vm->stack); // Sync stack for GC
        
        StructDef* s = ALLOCATE_OBJ(vm, StructDef, OBJ_STRUCT_DEF);
        s->fieldCount = fieldCount;
        s->fields = malloc(sizeof(char*) * fieldCount);
        
        for (int i = fieldCount - 1; i >= 0; i--) {
            Value fVal = *(--sp); // Pop field name
            ObjString* fStr = AS_STRING(fVal);
            char* f = malloc(fStr->length + 1);
            memcpy(f, fStr->chars, fStr->length);
            f[fStr->length] = 0;
            s->fields[i] = f;
        }
        
        Value nameVal = *(--sp); // Pop name
        ObjString* nameStr = AS_STRING(nameVal);
        s->name = nameStr->chars; 
        
        // Push StructDef as Value
        *sp++ = OBJ_VAL(s);
        
        NEXT();
    }
    
    op_array_push: {
        // Stack: [array, value] -> [] (Compiler emits OP_LOAD_NIL after this)
        Value val = *(--sp);      // Pop value
        Value arrVal = sp[-1];    // Peek array
        
        vm->stackTop = (int)(sp - vm->stack); // Sync stack for GC
        
        if (IS_ARRAY(arrVal)) {
            Array* arr = (Array*)AS_OBJ(arrVal);
            arrayPush(vm, arr, val);
        }
        NEXT();
    }
    
    op_array_push_clean: {
        // Stack: [array, value] -> [] (Consumes both)
        Value val = *(--sp);      // Pop value
        Value arrVal = *(--sp);   // Pop array
        
        vm->stackTop = (int)(sp - vm->stack); // Sync stack for GC
        
        if (IS_ARRAY(arrVal)) {
            Array* arr = (Array*)AS_OBJ(arrVal);
            arrayPush(vm, arr, val);
        }
        NEXT();
    }
    
    op_array_pop: {
        // Stack: [array] -> [result]
        Value arrVal = sp[-1];  // Peek array (Stack size 1 -> 1)
        
        // We overwrite the array slot with the result
        if (IS_ARRAY(arrVal)) {
            Array* arr = (Array*)AS_OBJ(arrVal);
            Value result;
            if (arrayPop(arr, &result)) {
                sp[-1] = result;
            } else {
                sp[-1] = NIL_VAL;
            }
        } else {
            sp[-1] = NIL_VAL;
        }
        NEXT();
    }
    
    op_array_len: {
        // Stack: [array] -> [int]
        Value arrVal = sp[-1];  // Peek
        int count = 0;
        
        if (IS_ARRAY(arrVal)) {
            count = ((Array*)AS_OBJ(arrVal))->count;
        } else if (IS_STRING(arrVal)) {
            count = ((ObjString*)AS_OBJ(arrVal))->length;
        } else if (IS_MAP(arrVal)) {
            Map* m = (Map*)AS_OBJ(arrVal);
            for (int i = 0; i < TABLE_SIZE; i++) {
                MapEntry* e = m->buckets[i];
                while (e) {
                    count++;
                    e = e->next;
                }
            }
        }
        
        sp[-1] = INT_VAL(count); // Overwrite with result
        NEXT();
    }
    
    // === JIT INTEGRATION ===
    op_hotspot_check: {
        // OSR Hotspot Check
        int offset = (int)(ip - chunk->code - 1);
        if (vm->jitEnabled && chunk->hotspots && offset >= 0 && offset < chunk->hotspotCapacity) {
            chunk->hotspots[offset]++;
             // Trigger compilation if hot and not yet compiled
            if (chunk->hotspots[offset] > 50 && !chunk->isCompiled) {
                 // Sync stack for GC safety during compilation
                 vm->stackTop = (int)(sp - vm->stack);
                 JITFunction* jitFunc = compileFunction(vm, chunk);
                 if (jitFunc && jitFunc->isValid) {
                     if (!chunk->jitCode) chunk->jitCode = malloc(sizeof(void*));
                     chunk->jitCode[0] = jitFunc;
                     chunk->isCompiled = true;
                     chunk->jitCodeCount = 1;
                     // Logic continues to NEXT() which interprets this loop. 
                     // Next invocation of this function (or restart) will use JIT.
                     // (For true OSR we'd jump, but this is simple JIT)
                 }
            }
        }
        NEXT();
    }
    
    op_osr_entry: op_compiled_call: op_deopt:
        NEXT();
    
    // === SPECIAL ===
    op_print: {
        Value val = *(--sp);
        printValue(val);
        printf("\n");
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
    
    // Performance stats removed - use ucoreTimer for benchmarking
    
    return elapsed;
}
