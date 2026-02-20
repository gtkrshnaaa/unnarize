#include "bytecode/opcodes.h"
#include <string.h>

/**
 * Register-based opcode metadata table
 * format: 0=ABC, 1=ABx, 2=AsBx, 3=sBx, 4=A-only
 */

static const OpcodeInfo OPCODE_TABLE[] = {
    // Data movement
    [OP_MOVE]       = {"MOVE",       0, false},
    [OP_LOADK]      = {"LOADK",      1, false},
    [OP_LOADI]      = {"LOADI",      1, false},
    [OP_LOADNIL]    = {"LOADNIL",    4, false},
    [OP_LOADTRUE]   = {"LOADTRUE",   4, false},
    [OP_LOADFALSE]  = {"LOADFALSE",  4, false},

    // Global variables
    [OP_GETGLOBAL]  = {"GETGLOBAL",  1, false},
    [OP_SETGLOBAL]  = {"SETGLOBAL",  1, true},
    [OP_DEFGLOBAL]  = {"DEFGLOBAL",  1, true},

    // Arithmetic
    [OP_ADD]        = {"ADD",        0, false},
    [OP_SUB]        = {"SUB",        0, false},
    [OP_MUL]        = {"MUL",        0, false},
    [OP_DIV]        = {"DIV",        0, false},
    [OP_MOD]        = {"MOD",        0, false},
    [OP_NEG]        = {"NEG",        0, false},

    // Comparisons
    [OP_LT]         = {"LT",        0, false},
    [OP_LE]         = {"LE",        0, false},
    [OP_GT]         = {"GT",        0, false},
    [OP_GE]         = {"GE",        0, false},
    [OP_EQ]         = {"EQ",        0, false},
    [OP_NE]         = {"NE",        0, false},

    // Logical
    [OP_NOT]        = {"NOT",        0, false},

    // Control flow
    [OP_JMP]        = {"JMP",        3, false},
    [OP_JMPF]       = {"JMPF",       2, false},
    [OP_JMPT]       = {"JMPT",       2, false},
    [OP_LOOP]       = {"LOOP",       3, false},

    // Function calls
    [OP_CALL]       = {"CALL",       0, true},
    [OP_RETURN]     = {"RETURN",     4, true},
    [OP_RETURNNIL]  = {"RETURNNIL",  4, true},

    // Property access
    [OP_GETPROP]    = {"GETPROP",    0, false},
    [OP_SETPROP]    = {"SETPROP",    0, true},

    // Index access
    [OP_GETIDX]     = {"GETIDX",     0, false},
    [OP_SETIDX]     = {"SETIDX",     0, true},

    // Object creation
    [OP_NEWARRAY]   = {"NEWARRAY",   1, true},
    [OP_NEWMAP]     = {"NEWMAP",     4, true},
    [OP_NEWSTRUCT]  = {"NEWSTRUCT",  0, true},

    // Struct definition
    [OP_STRUCTDEF]  = {"STRUCTDEF",  1, true},

    // Array builtins
    [OP_PUSH]       = {"PUSH",       0, true},
    [OP_POP]        = {"POP",        0, true},
    [OP_LEN]        = {"LEN",        0, false},

    // Import
    [OP_IMPORT]     = {"IMPORT",     1, true},

    // Async
    [OP_ASYNC]      = {"ASYNC",      0, true},
    [OP_AWAIT]      = {"AWAIT",      0, true},

    // Special
    [OP_PRINT]      = {"PRINT",      4, true},
    [OP_HALT]       = {"HALT",       4, true},
    [OP_NOP]        = {"NOP",        4, false},

    // Foreach
    [OP_FOREACH_PREP] = {"FOREACH_PREP", 0, true},
    [OP_FOREACH_NEXT] = {"FOREACH_NEXT", 2, true},

    // Concatenation
    [OP_CONCAT]     = {"CONCAT",     0, false},
};

const OpcodeInfo* getOpcodeInfo(OpCode op) {
    if (op >= 0 && op < OPCODE_COUNT) {
        return &OPCODE_TABLE[op];
    }
    return NULL;
}
