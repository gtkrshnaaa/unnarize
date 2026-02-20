#include "bytecode/interpreter.h"
#include "bytecode/opcodes.h"
#include <unistd.h>
#include "parser.h"
#include "lexer.h"
#include "bytecode/compiler.h"
#include "vm.h"
#include <libgen.h>
#include <stdio.h>
#include <sys/time.h>

/**
 * Register-Based Bytecode Interpreter
 * Direct threading with computed goto (GCC/Clang)
 * Each instruction word is 32-bit; operands are register indices.
 */

static inline uint64_t getMicroseconds() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

static char* readFile_internal(const char* path) {
    FILE* file = fopen(path, "rb");
    if (!file) return NULL;
    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);
    char* buffer = malloc(fileSize + 1);
    if (!buffer) { fclose(file); return NULL; }
    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    if (bytesRead < fileSize) { free(buffer); fclose(file); return NULL; }
    buffer[bytesRead] = '\0';
    fclose(file);
    return buffer;
}

// Truthiness evaluation
static inline bool isTruthy(Value v) {
    if (IS_BOOL(v)) return AS_BOOL(v);
    if (IS_NIL(v))  return false;
    if (IS_INT(v))  return AS_INT(v) != 0;
    if (IS_FLOAT(v)) return AS_FLOAT(v) != 0.0;
    return true; // Objects are truthy
}

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

uint64_t executeBytecode(VM* vm, BytecodeChunk* chunk, int entryStackDepth) {
    uint64_t startTime = getMicroseconds();

    // Register file: base pointer into vm->registers
    Value* regs = vm->registers + vm->regBase;

    // Instruction pointer (32-bit words)
    register uint32_t* ip = chunk->code;
    register Value* constants = chunk->constants;

#ifdef __GNUC__
    // Computed goto dispatch table (must match OpCode enum order exactly)
    static void* dispatchTable[OPCODE_COUNT] = {
        [OP_MOVE]       = &&op_move,
        [OP_LOADK]      = &&op_loadk,
        [OP_LOADI]      = &&op_loadi,
        [OP_LOADNIL]    = &&op_loadnil,
        [OP_LOADTRUE]   = &&op_loadtrue,
        [OP_LOADFALSE]  = &&op_loadfalse,
        [OP_GETGLOBAL]  = &&op_getglobal,
        [OP_SETGLOBAL]  = &&op_setglobal,
        [OP_DEFGLOBAL]  = &&op_defglobal,
        [OP_ADD]        = &&op_add,
        [OP_SUB]        = &&op_sub,
        [OP_MUL]        = &&op_mul,
        [OP_DIV]        = &&op_div,
        [OP_MOD]        = &&op_mod,
        [OP_NEG]        = &&op_neg,
        [OP_LT]         = &&op_lt,
        [OP_LE]         = &&op_le,
        [OP_GT]         = &&op_gt,
        [OP_GE]         = &&op_ge,
        [OP_EQ]         = &&op_eq,
        [OP_NE]         = &&op_ne,
        [OP_NOT]        = &&op_not,
        [OP_JMP]        = &&op_jmp,
        [OP_JMPF]       = &&op_jmpf,
        [OP_JMPT]       = &&op_jmpt,
        [OP_LOOP]       = &&op_loop,
        [OP_CALL]       = &&op_call,
        [OP_RETURN]     = &&op_return,
        [OP_RETURNNIL]  = &&op_returnnil,
        [OP_GETPROP]    = &&op_getprop,
        [OP_SETPROP]    = &&op_setprop,
        [OP_GETIDX]     = &&op_getidx,
        [OP_SETIDX]     = &&op_setidx,
        [OP_NEWARRAY]   = &&op_newarray,
        [OP_NEWMAP]     = &&op_newmap,
        [OP_NEWSTRUCT]  = &&op_newstruct,
        [OP_STRUCTDEF]  = &&op_structdef,
        [OP_PUSH]       = &&op_push,
        [OP_POP]        = &&op_pop_arr,
        [OP_LEN]        = &&op_len,
        [OP_IMPORT]     = &&op_import,
        [OP_ASYNC]      = &&op_async,
        [OP_AWAIT]      = &&op_await,
        [OP_PRINT]      = &&op_print,
        [OP_HALT]       = &&op_halt,
        [OP_NOP]        = &&op_nop,
        [OP_FOREACH_PREP] = &&op_nop,
        [OP_FOREACH_NEXT] = &&op_nop,
        [OP_CONCAT]     = &&op_concat,
    };

    #define DISPATCH() do { \
        uint32_t _inst = *ip; \
        goto *dispatchTable[DECODE_OP(_inst)]; \
    } while(0)
    #define NEXT() do { ip++; DISPATCH(); } while(0)
    #define FETCH() (*ip)

    DISPATCH();

    // ===== DATA MOVEMENT =====
    op_move: {
        uint32_t inst = FETCH();
        regs[DECODE_A(inst)] = regs[DECODE_B(inst)];
        NEXT();
    }

    op_loadk: {
        uint32_t inst = FETCH();
        regs[DECODE_A(inst)] = constants[DECODE_Bx(inst)];
        NEXT();
    }

    op_loadi: {
        uint32_t inst = FETCH();
        int val = DECODE_sBx(inst); // Signed 16-bit
        regs[DECODE_A(inst)] = INT_VAL(val);
        NEXT();
    }

    op_loadnil: {
        uint32_t inst = FETCH();
        regs[DECODE_A(inst)] = NIL_VAL;
        NEXT();
    }

    op_loadtrue: {
        uint32_t inst = FETCH();
        regs[DECODE_A(inst)] = BOOL_VAL(true);
        NEXT();
    }

    op_loadfalse: {
        uint32_t inst = FETCH();
        regs[DECODE_A(inst)] = BOOL_VAL(false);
        NEXT();
    }

    // ===== GLOBAL VARIABLES =====
    op_getglobal: {
        uint32_t inst = FETCH();
        uint8_t a = DECODE_A(inst);
        uint16_t bx = DECODE_Bx(inst);
        ObjString* name = AS_STRING(constants[bx]);
        unsigned int h = name->hash % TABLE_SIZE;
        VarEntry* entry = vm->globalEnv->buckets[h];
        while (entry) {
            if (entry->key == name->chars ||
                (entry->keyLength == name->length && memcmp(entry->key, name->chars, name->length) == 0)) {
                regs[a] = entry->value;
                goto getglobal_done;
            }
            entry = entry->next;
        }
        printf("Runtime Error: Undefined variable '%s'\n", name->chars);
        exit(1);
        getglobal_done:
        NEXT();
    }

    op_setglobal: {
        uint32_t inst = FETCH();
        uint8_t a = DECODE_A(inst);
        uint16_t bx = DECODE_Bx(inst);
        ObjString* name = AS_STRING(constants[bx]);
        unsigned int h = name->hash % TABLE_SIZE;
        VarEntry* entry = vm->globalEnv->buckets[h];
        while (entry) {
            if (entry->key == name->chars ||
                (entry->keyLength == name->length && memcmp(entry->key, name->chars, name->length) == 0)) {
                entry->value = regs[a];
                WRITE_BARRIER(vm, vm->globalEnv);
                NEXT();
            }
            entry = entry->next;
        }
        // Insert new
        VarEntry* ne = malloc(sizeof(VarEntry));
        ne->key = name->chars;
        ne->keyString = name;
        ne->keyLength = name->length;
        ne->ownsKey = false;
        ne->value = regs[a];
        ne->next = vm->globalEnv->buckets[h];
        vm->globalEnv->buckets[h] = ne;
        WRITE_BARRIER(vm, vm->globalEnv);
        NEXT();
    }

    op_defglobal: {
        uint32_t inst = FETCH();
        uint8_t a = DECODE_A(inst);
        uint16_t bx = DECODE_Bx(inst);
        ObjString* name = AS_STRING(constants[bx]);
        unsigned int h = name->hash % TABLE_SIZE;

        VarEntry* entry = vm->globalEnv->buckets[h];
        while (entry) {
            if (entry->key == name->chars ||
                (entry->keyLength == name->length && memcmp(entry->key, name->chars, name->length) == 0)) {
                entry->value = regs[a];
                WRITE_BARRIER(vm, vm->globalEnv);
                goto defglobal_done;
            }
            entry = entry->next;
        }
        // Insert new
        {
            VarEntry* ne = malloc(sizeof(VarEntry));
            ne->key = name->chars;
            ne->keyString = name;
            ne->keyLength = name->length;
            ne->ownsKey = false;
            ne->value = regs[a];
            ne->next = vm->globalEnv->buckets[h];
            vm->globalEnv->buckets[h] = ne;
            WRITE_BARRIER(vm, vm->globalEnv);
        }
        defglobal_done:
        NEXT();
    }

    // ===== ARITHMETIC =====
    op_add: {
        uint32_t inst = FETCH();
        uint8_t a = DECODE_A(inst), b = DECODE_B(inst), c = DECODE_C(inst);
        Value vb = regs[b], vc = regs[c];

        if (likely(IS_INT(vb) && IS_INT(vc))) {
            regs[a] = INT_VAL(AS_INT(vb) + AS_INT(vc));
        } else if (IS_NUMBER(vb) && IS_NUMBER(vc)) {
            double db = IS_INT(vb) ? (double)AS_INT(vb) : AS_FLOAT(vb);
            double dc = IS_INT(vc) ? (double)AS_INT(vc) : AS_FLOAT(vc);
            regs[a] = FLOAT_VAL(db + dc);
        } else if (IS_STRING(vb) || IS_STRING(vc)) {
            // String concatenation
            vm->regTop = vm->regBase + (int)(chunk->maxRegs + 1);
            char bufB[64], bufC[64];
            const char* sB;
            const char* sC;

            if (IS_STRING(vb)) sB = AS_CSTRING(vb);
            else if (IS_INT(vb)) { snprintf(bufB, 64, "%ld", (long)AS_INT(vb)); sB = bufB; }
            else if (IS_FLOAT(vb)) { snprintf(bufB, 64, "%g", AS_FLOAT(vb)); sB = bufB; }
            else if (IS_BOOL(vb)) sB = AS_BOOL(vb) ? "true" : "false";
            else if (IS_NIL(vb)) sB = "nil";
            else sB = "[object]";

            if (IS_STRING(vc)) sC = AS_CSTRING(vc);
            else if (IS_INT(vc)) { snprintf(bufC, 64, "%ld", (long)AS_INT(vc)); sC = bufC; }
            else if (IS_FLOAT(vc)) { snprintf(bufC, 64, "%g", AS_FLOAT(vc)); sC = bufC; }
            else if (IS_BOOL(vc)) sC = AS_BOOL(vc) ? "true" : "false";
            else if (IS_NIL(vc)) sC = "nil";
            else sC = "[object]";

            size_t lenB = strlen(sB), lenC = strlen(sC);
            char* result = malloc(lenB + lenC + 1);
            memcpy(result, sB, lenB);
            memcpy(result + lenB, sC, lenC);
            result[lenB + lenC] = '\0';
            ObjString* str = internString(vm, result, lenB + lenC);
            free(result);
            regs[a] = OBJ_VAL(str);
        } else {
            regs[a] = NIL_VAL;
        }
        NEXT();
    }

    op_sub: {
        uint32_t inst = FETCH();
        uint8_t a = DECODE_A(inst), b = DECODE_B(inst), c = DECODE_C(inst);
        Value vb = regs[b], vc = regs[c];
        if (likely(IS_INT(vb) && IS_INT(vc))) {
            regs[a] = INT_VAL(AS_INT(vb) - AS_INT(vc));
        } else {
            double db = IS_INT(vb) ? (double)AS_INT(vb) : AS_FLOAT(vb);
            double dc = IS_INT(vc) ? (double)AS_INT(vc) : AS_FLOAT(vc);
            regs[a] = FLOAT_VAL(db - dc);
        }
        NEXT();
    }

    op_mul: {
        uint32_t inst = FETCH();
        uint8_t a = DECODE_A(inst), b = DECODE_B(inst), c = DECODE_C(inst);
        Value vb = regs[b], vc = regs[c];
        if (likely(IS_INT(vb) && IS_INT(vc))) {
            regs[a] = INT_VAL(AS_INT(vb) * AS_INT(vc));
        } else {
            double db = IS_INT(vb) ? (double)AS_INT(vb) : AS_FLOAT(vb);
            double dc = IS_INT(vc) ? (double)AS_INT(vc) : AS_FLOAT(vc);
            regs[a] = FLOAT_VAL(db * dc);
        }
        NEXT();
    }

    op_div: {
        uint32_t inst = FETCH();
        uint8_t a = DECODE_A(inst), b = DECODE_B(inst), c = DECODE_C(inst);
        Value vb = regs[b], vc = regs[c];
        if (likely(IS_INT(vb) && IS_INT(vc))) {
            int64_t ic = AS_INT(vc);
            if (unlikely(ic == 0)) { printf("Runtime Error: Division by zero.\n"); exit(1); }
            regs[a] = INT_VAL(AS_INT(vb) / ic);
        } else {
            double db = IS_INT(vb) ? (double)AS_INT(vb) : AS_FLOAT(vb);
            double dc = IS_INT(vc) ? (double)AS_INT(vc) : AS_FLOAT(vc);
            regs[a] = FLOAT_VAL(db / dc);
        }
        NEXT();
    }

    op_mod: {
        uint32_t inst = FETCH();
        uint8_t a = DECODE_A(inst), b = DECODE_B(inst), c = DECODE_C(inst);
        if (IS_INT(regs[b]) && IS_INT(regs[c])) {
            regs[a] = INT_VAL(AS_INT(regs[b]) % AS_INT(regs[c]));
        }
        NEXT();
    }

    op_neg: {
        uint32_t inst = FETCH();
        uint8_t a = DECODE_A(inst), b = DECODE_B(inst);
        if (IS_INT(regs[b])) regs[a] = INT_VAL(-AS_INT(regs[b]));
        else if (IS_FLOAT(regs[b])) regs[a] = FLOAT_VAL(-AS_FLOAT(regs[b]));
        NEXT();
    }

    // ===== COMPARISONS =====
    op_lt: {
        uint32_t inst = FETCH();
        uint8_t a = DECODE_A(inst), b = DECODE_B(inst), c = DECODE_C(inst);
        Value vb = regs[b], vc = regs[c];
        if (likely(IS_INT(vb) && IS_INT(vc))) { regs[a] = BOOL_VAL(AS_INT(vb) < AS_INT(vc)); }
        else if (IS_NUMBER(vb) && IS_NUMBER(vc)) {
            double db = IS_INT(vb) ? (double)AS_INT(vb) : AS_FLOAT(vb);
            double dc = IS_INT(vc) ? (double)AS_INT(vc) : AS_FLOAT(vc);
            regs[a] = BOOL_VAL(db < dc);
        } else regs[a] = BOOL_VAL(false);
        NEXT();
    }

    op_le: {
        uint32_t inst = FETCH();
        uint8_t a = DECODE_A(inst), b = DECODE_B(inst), c = DECODE_C(inst);
        Value vb = regs[b], vc = regs[c];
        if (likely(IS_INT(vb) && IS_INT(vc))) { regs[a] = BOOL_VAL(AS_INT(vb) <= AS_INT(vc)); }
        else if (IS_NUMBER(vb) && IS_NUMBER(vc)) {
            double db = IS_INT(vb) ? (double)AS_INT(vb) : AS_FLOAT(vb);
            double dc = IS_INT(vc) ? (double)AS_INT(vc) : AS_FLOAT(vc);
            regs[a] = BOOL_VAL(db <= dc);
        } else regs[a] = BOOL_VAL(false);
        NEXT();
    }

    op_gt: {
        uint32_t inst = FETCH();
        uint8_t a = DECODE_A(inst), b = DECODE_B(inst), c = DECODE_C(inst);
        Value vb = regs[b], vc = regs[c];
        if (likely(IS_INT(vb) && IS_INT(vc))) { regs[a] = BOOL_VAL(AS_INT(vb) > AS_INT(vc)); }
        else if (IS_NUMBER(vb) && IS_NUMBER(vc)) {
            double db = IS_INT(vb) ? (double)AS_INT(vb) : AS_FLOAT(vb);
            double dc = IS_INT(vc) ? (double)AS_INT(vc) : AS_FLOAT(vc);
            regs[a] = BOOL_VAL(db > dc);
        } else regs[a] = BOOL_VAL(false);
        NEXT();
    }

    op_ge: {
        uint32_t inst = FETCH();
        uint8_t a = DECODE_A(inst), b = DECODE_B(inst), c = DECODE_C(inst);
        Value vb = regs[b], vc = regs[c];
        if (likely(IS_INT(vb) && IS_INT(vc))) { regs[a] = BOOL_VAL(AS_INT(vb) >= AS_INT(vc)); }
        else if (IS_NUMBER(vb) && IS_NUMBER(vc)) {
            double db = IS_INT(vb) ? (double)AS_INT(vb) : AS_FLOAT(vb);
            double dc = IS_INT(vc) ? (double)AS_INT(vc) : AS_FLOAT(vc);
            regs[a] = BOOL_VAL(db >= dc);
        } else regs[a] = BOOL_VAL(false);
        NEXT();
    }

    op_eq: {
        uint32_t inst = FETCH();
        uint8_t a = DECODE_A(inst), b = DECODE_B(inst), c = DECODE_C(inst);
        Value vb = regs[b], vc = regs[c];
        if (IS_INT(vb) && IS_INT(vc)) { regs[a] = BOOL_VAL(AS_INT(vb) == AS_INT(vc)); }
        else if (IS_FLOAT(vb) && IS_FLOAT(vc)) { regs[a] = BOOL_VAL(AS_FLOAT(vb) == AS_FLOAT(vc)); }
        else if (IS_BOOL(vb) && IS_BOOL(vc)) { regs[a] = BOOL_VAL(AS_BOOL(vb) == AS_BOOL(vc)); }
        else if (IS_NIL(vb) && IS_NIL(vc)) { regs[a] = BOOL_VAL(true); }
        else if (IS_OBJ(vb) && IS_OBJ(vc)) { regs[a] = BOOL_VAL(AS_OBJ(vb) == AS_OBJ(vc)); }
        else regs[a] = BOOL_VAL(false);
        NEXT();
    }

    op_ne: {
        uint32_t inst = FETCH();
        uint8_t a = DECODE_A(inst), b = DECODE_B(inst), c = DECODE_C(inst);
        Value vb = regs[b], vc = regs[c];
        if (IS_INT(vb) && IS_INT(vc)) { regs[a] = BOOL_VAL(AS_INT(vb) != AS_INT(vc)); }
        else if (IS_FLOAT(vb) && IS_FLOAT(vc)) { regs[a] = BOOL_VAL(AS_FLOAT(vb) != AS_FLOAT(vc)); }
        else if (IS_BOOL(vb) && IS_BOOL(vc)) { regs[a] = BOOL_VAL(AS_BOOL(vb) != AS_BOOL(vc)); }
        else if (IS_NIL(vb) && IS_NIL(vc)) { regs[a] = BOOL_VAL(false); }
        else regs[a] = BOOL_VAL(true);
        NEXT();
    }

    op_not: {
        uint32_t inst = FETCH();
        regs[DECODE_A(inst)] = BOOL_VAL(!isTruthy(regs[DECODE_B(inst)]));
        NEXT();
    }

    // ===== CONTROL FLOW =====
    op_jmp: {
        uint32_t inst = FETCH();
        int offset = DECODE_sBx24(inst);
        ip += offset + 1;
        DISPATCH();
    }

    op_jmpf: {
        uint32_t inst = FETCH();
        if (!isTruthy(regs[DECODE_A(inst)])) {
            int offset = DECODE_sBx(inst);
            ip += offset + 1;
            DISPATCH();
        }
        NEXT();
    }

    op_jmpt: {
        uint32_t inst = FETCH();
        if (isTruthy(regs[DECODE_A(inst)])) {
            int offset = DECODE_sBx(inst);
            ip += offset + 1;
            DISPATCH();
        }
        NEXT();
    }

    op_loop: {
        uint32_t inst = FETCH();
        int offset = DECODE_sBx24(inst);
        ip -= offset - 1;
        DISPATCH();
    }

    // ===== FUNCTION CALLS =====
    op_call: {
        uint32_t inst = FETCH();
        uint8_t funcReg = DECODE_A(inst);
        uint8_t argCount = DECODE_B(inst);
        // uint8_t resultCount = DECODE_C(inst); // Currently always 1

        Value funcVal = regs[funcReg];
        if (!IS_OBJ(funcVal)) {
            printf("Runtime Error: Attempt to call non-function value.\n");
            exit(1);
        }

        Obj* obj = AS_OBJ(funcVal);
        if (obj->type == OBJ_FUNCTION) {
            Function* func = (Function*)obj;
            if (func->isNative) {
                // Native call: pass args from regs[funcReg+1..funcReg+argCount]
                vm->regTop = vm->regBase + (int)(chunk->maxRegs + 1);
                Value* args = &regs[funcReg + 1];
                Value result = func->native(vm, args, argCount);
                regs[funcReg] = result;
                NEXT();
            }

            if (argCount != func->paramCount) {
                printf("Runtime Error: Expected %d args but got %d.\n", func->paramCount, argCount);
                exit(1);
            }

            if (unlikely(vm->callStackTop >= CALL_STACK_MAX)) {
                printf("Runtime Error: Stack overflow.\n");
                exit(1);
            }

            // Save current frame
            CallFrame* frame = &vm->callStack[vm->callStackTop++];
            frame->ip = ip + 1; // Return after this CALL instruction
            frame->chunk = chunk;
            frame->function = NULL;
            frame->regBase = vm->regBase;
            frame->resultReg = funcReg; // Caller wants result in this register
            frame->prevGlobalEnv = vm->globalEnv;

            if (func->moduleEnv) {
                vm->globalEnv = func->moduleEnv;
            }

            // Set up callee's register window
            int calleeBase = vm->regBase + funcReg + 1;
            // Args are already at regs[funcReg+1..funcReg+argCount]
            // which is registers[calleeBase..calleeBase+argCount-1]
            // Callee sees these as its R(1)..R(paramCount)
            // R(0) is the function itself (at registers[calleeBase-1])

            // Adjust: callee expects params at R(1), but they're at absolute calleeBase
            // Set regBase so R(0) = function slot
            vm->regBase = vm->regBase + funcReg;
            regs = vm->registers + vm->regBase;

            chunk = func->bytecodeChunk;
            constants = chunk->constants;
            ip = chunk->code;
            DISPATCH();

        } else if (obj->type == OBJ_STRUCT_DEF) {
            StructDef* def = (StructDef*)obj;
            if (argCount != def->fieldCount) {
                printf("Runtime Error: Struct '%s' expected %d args but got %d.\n",
                       def->name, def->fieldCount, argCount);
                exit(1);
            }

            vm->regTop = vm->regBase + (int)(chunk->maxRegs + 1);
            StructInstance* inst = ALLOCATE_OBJ(vm, StructInstance, OBJ_STRUCT_INSTANCE);
            inst->def = def;
            inst->fields = malloc(sizeof(Value) * def->fieldCount);
            for (int i = 0; i < argCount; i++) {
                inst->fields[i] = regs[funcReg + 1 + i];
            }
            regs[funcReg] = OBJ_VAL(inst);
            NEXT();
        } else {
            printf("Runtime Error: Call on non-function object (type %d).\n", obj->type);
            exit(1);
        }
    }

    op_return: {
        uint32_t inst = FETCH();
        Value retVal = regs[DECODE_A(inst)];

        vm->callStackTop--;
        if (vm->callStackTop == entryStackDepth) {
            return getMicroseconds() - startTime;
        }

        CallFrame* frame = &vm->callStack[vm->callStackTop];
        if (frame->prevGlobalEnv) {
            vm->globalEnv = frame->prevGlobalEnv;
        }

        vm->regBase = frame->regBase;
        regs = vm->registers + vm->regBase;
        chunk = frame->chunk;
        constants = chunk->constants;
        ip = frame->ip;

        // Store return value in caller's result register
        regs[frame->resultReg] = retVal;
        DISPATCH();
    }

    op_returnnil: {
        vm->callStackTop--;
        if (vm->callStackTop == entryStackDepth) {
            return getMicroseconds() - startTime;
        }

        CallFrame* frame = &vm->callStack[vm->callStackTop];
        if (frame->prevGlobalEnv) {
            vm->globalEnv = frame->prevGlobalEnv;
        }

        vm->regBase = frame->regBase;
        regs = vm->registers + vm->regBase;
        chunk = frame->chunk;
        constants = chunk->constants;
        ip = frame->ip;

        regs[frame->resultReg] = NIL_VAL;
        DISPATCH();
    }

    // ===== PROPERTY ACCESS =====
    op_getprop: {
        uint32_t inst = FETCH();
        uint8_t a = DECODE_A(inst), b = DECODE_B(inst), c = DECODE_C(inst);
        Value objVal = regs[b];
        ObjString* name = AS_STRING(constants[c]);

        if (IS_OBJ(objVal)) {
            Obj* obj = AS_OBJ(objVal);

            if (obj->type == OBJ_STRUCT_INSTANCE) {
                StructInstance* si = (StructInstance*)obj;
                for (int i = 0; i < si->def->fieldCount; i++) {
                    if (strcmp(si->def->fields[i], name->chars) == 0) {
                        regs[a] = si->fields[i];
                        NEXT();
                    }
                }
                regs[a] = NIL_VAL;
                NEXT();
            }
            else if (obj->type == OBJ_MODULE) {
                Module* mod = (Module*)obj;
                unsigned int h = name->hash % TABLE_SIZE;
                VarEntry* e = mod->env->buckets[h];
                while (e) {
                    if (strcmp(e->key, name->chars) == 0) {
                        regs[a] = e->value;
                        NEXT();
                    }
                    e = e->next;
                }
                printf("Runtime Error: Undefined property '%s' in module '%s'.\n", name->chars, mod->name);
                exit(1);
            }
            else if (obj->type == OBJ_STRING) {
                if (name->length == 6 && memcmp(name->chars, "length", 6) == 0) {
                    regs[a] = INT_VAL(((ObjString*)obj)->length);
                    NEXT();
                }
            }
        }
        printf("Runtime Error: Cannot read property '%s' on this type.\n", name->chars);
        exit(1);
    }

    op_setprop: {
        uint32_t inst = FETCH();
        uint8_t a = DECODE_A(inst), b = DECODE_B(inst), c = DECODE_C(inst);
        Value objVal = regs[a];
        ObjString* name = AS_STRING(constants[b]);
        Value val = regs[c];

        if (IS_OBJ(objVal) && AS_OBJ(objVal)->type == OBJ_STRUCT_INSTANCE) {
            StructInstance* si = (StructInstance*)AS_OBJ(objVal);
            for (int i = 0; i < si->def->fieldCount; i++) {
                if (strcmp(si->def->fields[i], name->chars) == 0) {
                    si->fields[i] = val;
                    WRITE_BARRIER(vm, si);
                    NEXT();
                }
            }
            printf("Runtime Error: Struct '%s' has no field '%s'.\n", si->def->name, name->chars);
            exit(1);
        }
        printf("Runtime Error: Only struct instances have settable properties.\n");
        exit(1);
    }

    // ===== INDEX ACCESS =====
    op_getidx: {
        uint32_t inst = FETCH();
        uint8_t a = DECODE_A(inst), b = DECODE_B(inst), c = DECODE_C(inst);
        Value target = regs[b], index = regs[c];

        if (IS_ARRAY(target) && IS_INT(index)) {
            Array* arr = (Array*)AS_OBJ(target);
            int idx = (int)AS_INT(index);
            regs[a] = (idx >= 0 && idx < arr->count) ? arr->items[idx] : NIL_VAL;
        } else if (IS_MAP(target)) {
            Map* map = (Map*)AS_OBJ(target);
            if (IS_STRING(index)) {
                ObjString* key = AS_STRING(index);
                int bucket;
                MapEntry* e = mapFindEntry(map, key->chars, key->length, &bucket);
                regs[a] = e ? e->value : NIL_VAL;
            } else if (IS_INT(index)) {
                int bucket;
                MapEntry* e = mapFindEntryInt(map, (int)AS_INT(index), &bucket);
                regs[a] = e ? e->value : NIL_VAL;
            } else regs[a] = NIL_VAL;
        } else regs[a] = NIL_VAL;
        NEXT();
    }

    op_setidx: {
        uint32_t inst = FETCH();
        uint8_t a = DECODE_A(inst), b = DECODE_B(inst), c = DECODE_C(inst);
        Value target = regs[a], index = regs[b], value = regs[c];

        if (IS_ARRAY(target) && IS_INT(index)) {
            Array* arr = (Array*)AS_OBJ(target);
            int idx = (int)AS_INT(index);
            if (idx >= 0) {
                if (idx >= arr->capacity) {
                    int newCap = idx + 1;
                    if (newCap < arr->capacity * 2) newCap = arr->capacity * 2;
                    arr->items = realloc(arr->items, newCap * sizeof(Value));
                    arr->capacity = newCap;
                }
                if (idx >= arr->count) {
                    for (int i = arr->count; i < idx; i++) arr->items[i] = NIL_VAL;
                    arr->count = idx + 1;
                }
                arr->items[idx] = value;
                WRITE_BARRIER(vm, arr);
            }
        } else if (IS_MAP(target)) {
            Map* map = (Map*)AS_OBJ(target);
            if (IS_STRING(index)) {
                ObjString* key = AS_STRING(index);
                mapSetStr(map, key->chars, key->length, value);
            } else if (IS_INT(index)) {
                mapSetInt(map, (int)AS_INT(index), value);
            }
            WRITE_BARRIER(vm, map);
        }
        NEXT();
    }

    // ===== OBJECT CREATION =====
    op_newarray: {
        uint32_t inst = FETCH();
        uint8_t a = DECODE_A(inst);
        vm->regTop = vm->regBase + (int)(chunk->maxRegs + 1);
        Array* arr = newArray(vm);
        regs[a] = OBJ_VAL(arr);
        NEXT();
    }

    op_newmap: {
        uint32_t inst = FETCH();
        uint8_t a = DECODE_A(inst);
        vm->regTop = vm->regBase + (int)(chunk->maxRegs + 1);
        Map* m = newMap(vm);
        regs[a] = OBJ_VAL(m);
        NEXT();
    }

    op_newstruct: {
        // OP_NEWSTRUCT A B C: R(A) = new instance of StructDef R(B) with C fields
        uint32_t inst = FETCH();
        uint8_t a = DECODE_A(inst), b = DECODE_B(inst), c = DECODE_C(inst);
        StructDef* def = (StructDef*)AS_OBJ(regs[b]);
        vm->regTop = vm->regBase + (int)(chunk->maxRegs + 1);
        StructInstance* si = ALLOCATE_OBJ(vm, StructInstance, OBJ_STRUCT_INSTANCE);
        si->def = def;
        si->fields = malloc(sizeof(Value) * c);
        for (int i = 0; i < c; i++) {
            si->fields[i] = regs[a + 1 + i];
        }
        regs[a] = OBJ_VAL(si);
        NEXT();
    }

    op_structdef: {
        uint32_t inst = FETCH();
        uint8_t fieldCount = DECODE_A(inst);
        uint16_t nameIdx = DECODE_Bx(inst);

        vm->regTop = vm->regBase + (int)(chunk->maxRegs + 1);
        StructDef* s = ALLOCATE_OBJ(vm, StructDef, OBJ_STRUCT_DEF);
        s->fieldCount = fieldCount;
        s->fields = malloc(sizeof(char*) * fieldCount);

        ObjString* nameStr = AS_STRING(constants[nameIdx]);
        s->name = nameStr->chars;

        // Field names are in constants following nameIdx
        // The compiler emits field names as constants, but we need to
        // know which constants they are. For now, use nameIdx+1..nameIdx+fieldCount
        for (int i = 0; i < fieldCount; i++) {
            ObjString* fStr = AS_STRING(constants[nameIdx + 1 + i]);
            char* f = malloc(fStr->length + 1);
            memcpy(f, fStr->chars, fStr->length);
            f[fStr->length] = 0;
            s->fields[i] = f;
        }

        // Put StructDef into a register for subsequent DEFGLOBAL
        // The next instruction should be DEFGLOBAL
        // Find the first register that was loaded with nameIdx
        // Actually the compiler uses nameReg for this. We need a convention.
        // Use a temp register approach: put into regs[0] area
        // Actually: the compiler emits LOADK nameReg nameIdx, then STRUCTDEF fieldCount nameIdx, then DEFGLOBAL nameReg nameIdx
        // So nameReg already has the name string. We overwrite it with the StructDef.
        // But STRUCTDEF doesn't specify which register to put the result in!
        // Fix: Since DEFGLOBAL follows and references nameReg, we need to overwrite nameReg.
        // The compiler should handle this. For now, put it on a known register.
        // Actually let's look at what the compiler does... it emits:
        //   LOADK nameReg nameIdx
        //   LOADK fReg1 fIdx1
        //   ...
        //   STRUCTDEF fieldCount nameIdx
        //   DEFGLOBAL nameReg nameIdx
        // So we overwrite nameReg with the struct def value.
        // But we don't know nameReg here... We need to find it.
        // The simplest fix: peek at the next instruction to find A of DEFGLOBAL.
        // Or: use a convention that result goes into a known register.
        // Let's use: look at next instruction
        {
            uint32_t nextInst = ip[1];
            uint8_t nextOp = DECODE_OP(nextInst);
            if (nextOp == OP_DEFGLOBAL || nextOp == OP_MOVE) {
                uint8_t destReg = DECODE_A(nextInst);
                regs[destReg] = OBJ_VAL(s);
            }
        }
        NEXT();
    }

    // ===== ARRAY BUILTINS =====
    op_push: {
        uint32_t inst = FETCH();
        uint8_t a = DECODE_A(inst), b = DECODE_B(inst);
        Value arrVal = regs[a], val = regs[b];
        if (IS_ARRAY(arrVal)) {
            vm->regTop = vm->regBase + (int)(chunk->maxRegs + 1);
            Array* arr = (Array*)AS_OBJ(arrVal);
            arrayPush(vm, arr, val);
            WRITE_BARRIER(vm, arr);
        }
        NEXT();
    }

    op_pop_arr: {
        uint32_t inst = FETCH();
        uint8_t a = DECODE_A(inst), b = DECODE_B(inst);
        Value arrVal = regs[b];
        if (IS_ARRAY(arrVal)) {
            Array* arr = (Array*)AS_OBJ(arrVal);
            Value result;
            if (arrayPop(arr, &result)) {
                regs[a] = result;
            } else {
                regs[a] = NIL_VAL;
            }
        } else {
            regs[a] = NIL_VAL;
        }
        NEXT();
    }

    op_len: {
        uint32_t inst = FETCH();
        uint8_t a = DECODE_A(inst), b = DECODE_B(inst);
        Value v = regs[b];
        int count = 0;
        if (IS_ARRAY(v)) count = ((Array*)AS_OBJ(v))->count;
        else if (IS_STRING(v)) count = ((ObjString*)AS_OBJ(v))->length;
        else if (IS_MAP(v)) {
            Map* m = (Map*)AS_OBJ(v);
            for (int i = 0; i < TABLE_SIZE; i++) {
                MapEntry* e = m->buckets[i];
                while (e) { count++; e = e->next; }
            }
        }
        regs[a] = INT_VAL(count);
        NEXT();
    }

    // ===== CONCAT =====
    op_concat: {
        uint32_t inst = FETCH();
        uint8_t a = DECODE_A(inst), b = DECODE_B(inst), c = DECODE_C(inst);
        // Same as string concat in op_add
        const char* sB = AS_CSTRING(regs[b]);
        const char* sC = AS_CSTRING(regs[c]);
        size_t lenB = strlen(sB), lenC = strlen(sC);
        char* result = malloc(lenB + lenC + 1);
        memcpy(result, sB, lenB);
        memcpy(result + lenB, sC, lenC);
        result[lenB + lenC] = '\0';
        ObjString* str = internString(vm, result, lenB + lenC);
        free(result);
        regs[a] = OBJ_VAL(str);
        NEXT();
    }

    // ===== IMPORT =====
    op_import: {
        uint32_t inst = FETCH();
        uint8_t a = DECODE_A(inst);
        uint16_t bx = DECODE_Bx(inst);
        Value nameVal = constants[bx];
        char* rawPath = AS_CSTRING(nameVal);

        char* importPath = rawPath;
        char* resolvedPath = NULL;

        if (rawPath[0] == '.') {
            if (vm->callStackTop > 0) {
                CallFrame* frame = &vm->callStack[vm->callStackTop - 1];
                if (frame->function && frame->function->modulePath) {
                    char* tmp = strdup(frame->function->modulePath);
                    char* dir = dirname(tmp);
                    size_t lenDir = strlen(dir);
                    size_t lenName = strlen(rawPath);
                    resolvedPath = malloc(lenDir + 1 + lenName + 1);
                    sprintf(resolvedPath, "%s/%s", dir, rawPath);
                    importPath = resolvedPath;
                    free(tmp);
                }
            }
        }

        char* source = readFile_internal(importPath);
        if (!source) {
            fprintf(stderr, "Runtime Error: Could not import module '%s'\n", rawPath);
            exit(1);
        }

        Lexer lex;
        initLexer(&lex, source);
        Parser p;
        p.tokens = malloc(64 * sizeof(Token));
        p.count = 0; p.capacity = 64; p.current = 0;
        while (true) {
            Token t = scanToken(&lex);
            if (p.count >= p.capacity) { p.capacity *= 2; p.tokens = realloc(p.tokens, p.capacity * sizeof(Token)); }
            p.tokens[p.count++] = t;
            if (t.type == TOKEN_EOF) break;
        }
        Node* ast = parse(&p);

        Environment* oldEnv = vm->globalEnv;
        Environment* modEnv = ALLOCATE_OBJ(vm, Environment, OBJ_ENVIRONMENT);
        modEnv->enclosing = oldEnv;
        memset(modEnv->buckets, 0, sizeof(modEnv->buckets));
        memset(modEnv->funcBuckets, 0, sizeof(modEnv->funcBuckets));
        vm->globalEnv = modEnv;

        BytecodeChunk* modChunk = malloc(sizeof(BytecodeChunk));
        initChunk(modChunk);
        compileToBytecode(vm, ast, modChunk, importPath);

        Function* modFunc = malloc(sizeof(Function));
        modFunc->obj.type = OBJ_FUNCTION;
        modFunc->obj.isMarked = false;
        modFunc->obj.isPermanent = false;
        modFunc->obj.generation = 0;
        modFunc->obj.next = vm->objects;
        vm->objects = (Obj*)modFunc;
        modFunc->name = (Token){0};
        modFunc->params = NULL;
        modFunc->paramCount = 0;
        modFunc->isNative = false;
        modFunc->isAsync = false;
        modFunc->bytecodeChunk = modChunk;
        modFunc->modulePath = importPath ? strdup(importPath) : NULL;
        modFunc->moduleEnv = modEnv;
        modFunc->closure = NULL;
        modFunc->native = NULL;
        modFunc->body = NULL;

        // Execute module
        if (vm->callStackTop >= CALL_STACK_MAX) {
            printf("Runtime Error: Stack overflow during import\n");
            exit(1);
        }
        CallFrame* frame = &vm->callStack[vm->callStackTop++];
        frame->ip = ip + 1;
        frame->chunk = chunk;
        frame->function = modFunc;
        frame->regBase = vm->regBase;
        frame->resultReg = a;
        frame->prevGlobalEnv = oldEnv;

        // Allocate register window for module
        int modBase = vm->regBase + chunk->maxRegs + 1;
        vm->regBase = modBase;
        regs = vm->registers + vm->regBase;

        int modEntryDepth = vm->callStackTop - 1;
        executeBytecode(vm, modChunk, modEntryDepth);

        // Restore
        vm->regBase = frame->regBase;
        regs = vm->registers + vm->regBase;
        vm->globalEnv = oldEnv;
        chunk = frame->chunk;
        constants = chunk->constants;
        ip = frame->ip;
        vm->callStackTop--;

        // Create module object
        Module* mod = ALLOCATE_OBJ(vm, Module, OBJ_MODULE);
        mod->env = modEnv;
        mod->name = strdup(importPath);
        mod->source = NULL;
        regs[a] = OBJ_VAL(mod);

        free(source);
        if (resolvedPath) free(resolvedPath);
        if (p.tokens) free(p.tokens);

        NEXT();
    }

    // ===== ASYNC =====
    op_async: {
        // Simplified: execute synchronously, wrap in Future
        uint32_t inst = FETCH();
        uint8_t a = DECODE_A(inst), b = DECODE_B(inst), c = DECODE_C(inst);
        // b = function reg, c = arg count
        Value funcVal = regs[b];
        if (!IS_OBJ(funcVal) || AS_OBJ(funcVal)->type != OBJ_FUNCTION) {
            printf("Runtime Error: Async call on non-function\n"); exit(1);
        }
        Function* func = (Function*)AS_OBJ(funcVal);

        vm->regTop = vm->regBase + (int)(chunk->maxRegs + 1);
        Future* future = ALLOCATE_OBJ(vm, Future, OBJ_FUTURE);
        future->done = false;
        future->result = NIL_VAL;
        pthread_mutex_init(&future->mu, NULL);
        pthread_cond_init(&future->cv, NULL);

        // Execute synchronously
        Value result = callFunction(vm, func, &regs[b + 1], c);

        pthread_mutex_lock(&future->mu);
        future->result = result;
        future->done = true;
        pthread_cond_broadcast(&future->cv);
        pthread_mutex_unlock(&future->mu);

        regs[a] = OBJ_VAL(future);
        NEXT();
    }

    op_await: {
        uint32_t inst = FETCH();
        uint8_t a = DECODE_A(inst), b = DECODE_B(inst);
        Value v = regs[b];
        if (!IS_OBJ(v) || AS_OBJ(v)->type != OBJ_FUTURE) {
            regs[a] = v;
            NEXT();
        }
        Future* f = (Future*)AS_OBJ(v);
        pthread_mutex_lock(&f->mu);
        while (!f->done) pthread_cond_wait(&f->cv, &f->mu);
        regs[a] = f->result;
        pthread_mutex_unlock(&f->mu);
        NEXT();
    }

    // ===== SPECIAL =====
    op_print: {
        uint32_t inst = FETCH();
        printValue(regs[DECODE_A(inst)]);
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
    return getMicroseconds() - startTime;
}
