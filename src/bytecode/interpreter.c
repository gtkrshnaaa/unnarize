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
    
    // Stack machine (Use VM stack properties)
    Value* sp = vm->stack + vm->stackTop;
    Value* fp = vm->stack + vm->fp;
    
    // Code pointer
    register uint8_t* ip = chunk->code;  // Instruction pointer in register
    uint8_t argCount; // For call opcodes
    
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
            printf("Runtime Error: Attempt to call non-function value. (Val Type: %d)\n", funcVal.type);
            exit(1);
        }
        
        Obj* obj = AS_OBJ(funcVal);
        // printf("DEBUG: Call object type: %d (Expected %d, OBJ_FUNCTION)\n", obj->type, OBJ_FUNCTION);
        if (obj->type == OBJ_FUNCTION) {
             Function* func = (Function*)obj;
             if (func->isNative) {
                 Value result = func->native(vm, sp - argCount, argCount);
                 sp -= argCount + 1; 
                 *sp++ = result;
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
                 
                 vm->fp = (int)((sp - argCount) - vm->stack);
                 fp = vm->stack + vm->fp;
                 
                 chunk = func->bytecodeChunk;
                 ip = chunk->code;
                 NEXT();
             }
        } else {
             printf("Runtime Error: Call on non-function object. (Type: %d)\n", obj->type);
             exit(1);
        }
    }

    op_return_nil: {
        Value val; val.type = VAL_NIL; val.intVal = 0;
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
        Value target = *(--sp);
        uint8_t constIdx = *ip++;
        ObjString* name = AS_STRING(chunk->constants[constIdx]);
        
        if (IS_OBJ(target)) {
             if (AS_OBJ(target)->type == OBJ_MODULE) {
                 Module* mod = (Module*)AS_OBJ(target);
                 unsigned int h = name->hash % TABLE_SIZE;
                 VarEntry* e = mod->env->buckets[h];
                 while (e) {
                     if (e->key == name->chars) { // Pointer equality safe due to interning
                         *sp++ = e->value;
                         goto property_loaded;
                     }
                     e = e->next;
                 }
                 // Not found
                 printf("Runtime Error: Undefined property '%s' in module '%s'\n", name->chars, mod->name);
                 exit(1);
             } else if (AS_OBJ(target)->type == OBJ_STRUCT_INSTANCE) {
                 // TODO: Struct property access
                 *sp++ = NIL_VAL; 
             } else {
                 printf("Runtime Error: Only instances and modules have properties.\n");
                 exit(1);
             }
        } else {
             printf("Runtime Error: Only instances and modules have properties.\n");
             exit(1);
        }
        
        property_loaded:
        NEXT();
    }

    op_store_property:
        // TODO: Implement property assignment
        NEXT();
    
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
        Array* arr = newArray(vm);
        *sp++ = OBJ_VAL(arr);
        NEXT();
    }
    
    op_new_map: {
        Map* m = newMap(vm);
        *sp++ = OBJ_VAL(m);
        NEXT();
    }
    
    op_new_object: {
        // TODO: Struct instantiation
        Value v; v.type = VAL_NIL; v.intVal = 0;
        *sp++ = v;
        NEXT();
    }
    
    op_array_push: {
        // Stack: [array, value] -> [] (Compiler emits LOAD_NIL after)
        Value val = *(--sp);
        Value arrVal = *(--sp); // Pop array too
        
        if (IS_ARRAY(arrVal)) {
            Array* arr = (Array*)AS_OBJ(arrVal);
            arrayPush(vm, arr, val);
        }
        NEXT();
    }
    
    op_array_pop: {
        // Stack: [array] -> [result] (Overwrite)
        Value arrVal = sp[-1];  // Use top value
        
        if (IS_ARRAY(arrVal)) {
            Array* arr = (Array*)AS_OBJ(arrVal);
            if (arr->count > 0) {
                arr->count--;
                sp[-1] = arr->items[arr->count]; // Overwrite array with popped value
            } else {
                sp[-1] = NIL_VAL;
            }
        } else {
            sp[-1] = NIL_VAL;
        }
        NEXT();
    }
    

    
    op_array_len: {
        Value arrVal = sp[-1];  // Peek
        
        if (IS_ARRAY(arrVal)) {
            Array* arr = (Array*)AS_OBJ(arrVal);
            sp[-1] = INT_VAL(arr->count);
        } else if (IS_MAP(arrVal)) {
            Map* m = (Map*)AS_OBJ(arrVal);
            // Count map entries
            int count = 0;
            for (int i = 0; i < TABLE_SIZE; i++) {
                MapEntry* e = m->buckets[i];
                while (e) {
                    count++;
                    e = e->next;
                }
            }
            sp[-1] = INT_VAL(count);
        } else {
            sp[-1] = INT_VAL(0);
        }
        NEXT();
    }
    
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
    
    // Performance stats removed - use ucoreTimer for benchmarking
    
    return elapsed;
}
