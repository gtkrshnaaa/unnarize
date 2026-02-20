#include "bytecode/opcodes.h"
#include <string.h>

/**
 * Opcode metadata table
 * Zero dependencies - pure static data
 */

static const OpcodeInfo OPCODE_TABLE[] = {
    // Stack ops
    {"LOAD_IMM", 4, false},
    {"LOAD_CONST", 1, false},
    {"LOAD_NIL", 0, false},
    {"LOAD_TRUE", 0, false},
    {"LOAD_FALSE", 0, false},
    {"POP", 0, false},
    {"DUP", 0, false},
    
    // Local variables
    {"LOAD_LOCAL", 1, false},
    {"STORE_LOCAL", 1, true},
    {"LOAD_LOCAL_0", 0, false},
    {"LOAD_LOCAL_1", 0, false},
    {"LOAD_LOCAL_2", 0, false},
    {"INC_LOCAL", 1, true},
    {"DEC_LOCAL", 1, true},
    
    // Global variables
    {"LOAD_GLOBAL", 2, false},
    {"STORE_GLOBAL", 2, true},
    {"DEFINE_GLOBAL", 2, true},
    
    // Specialized arithmetic (int)
    {"ADD_II", 0, false},
    {"SUB_II", 0, false},
    {"MUL_II", 0, false},
    {"DIV_II", 0, false},
    {"MOD_II", 0, false},
    {"NEG_I", 0, false},
    
    // Specialized arithmetic (float)
    {"ADD_FF", 0, false},
    {"SUB_FF", 0, false},
    {"MUL_FF", 0, false},
    {"DIV_FF", 0, false},
    {"NEG_F", 0, false},
    
    // Generic arithmetic
    {"ADD", 0, false},
    {"SUB", 0, false},
    {"MUL", 0, false},
    {"DIV", 0, false},
    {"MOD", 0, false},
    {"NEG", 0, false},
    
    // Specialized comparisons (int)
    {"LT_II", 0, false},
    {"LE_II", 0, false},
    {"GT_II", 0, false},
    {"GE_II", 0, false},
    {"EQ_II", 0, false},
    {"NE_II", 0, false},
    
    // Specialized comparisons (float)
    {"LT_FF", 0, false},
    {"LE_FF", 0, false},
    {"GT_FF", 0, false},
    {"GE_FF", 0, false},
    {"EQ_FF", 0, false},
    {"NE_FF", 0, false},
    
    // Generic comparisons
    {"LT", 0, false},
    {"LE", 0, false},
    {"GT", 0, false},
    {"GE", 0, false},
    {"EQ", 0, false},
    {"NE", 0, false},
    
    // Logical
    {"NOT", 0, false},
    {"AND", 0, false},
    {"OR", 0, false},
    
    // Control flow
    {"JUMP", 2, false},
    {"JUMP_IF_FALSE", 2, false},
    {"JUMP_IF_TRUE", 2, false},
    {"LOOP", 2, false},
    {"LOOP_HEADER", 0, false},
    
    // Function calls
    {"CALL", 1, true},
    {"CALL_0", 0, true},
    {"CALL_1", 0, true},
    {"CALL_2", 0, true},
    {"RETURN", 0, true},
    {"RETURN_NIL", 0, true},
    
    // Object/property
    {"LOAD_PROPERTY", 2, false},
    {"STORE_PROPERTY", 2, true},
    {"LOAD_INDEX", 0, false},
    {"STORE_INDEX", 0, true},
    
    // Object creation
    {"NEW_ARRAY", 1, true},
    {"NEW_MAP", 0, true},
    {"NEW_OBJECT", 0, true},
    
    // Array ops
    {"ARRAY_PUSH", 0, true},
    {"ARRAY_POP", 0, true},
    {"ARRAY_LEN", 0, false},
    
    // Async ops
    {"ASYNC_CALL", 1, true},
    {"AWAIT", 0, true},
    
    // Special
    {"PRINT", 0, true},
    {"HALT", 0, true},
    [OP_NOP] = {"NOP", 0, false},
    [OP_ARRAY_PUSH_CLEAN] = {"ARRAY_PUSH_CLEAN", 0, true},
};

const OpcodeInfo* getOpcodeInfo(OpCode op) {
    if (op >= 0 && op < OPCODE_COUNT) {
        return &OPCODE_TABLE[op];
    }
    return NULL;
}
