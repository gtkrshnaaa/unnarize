#ifndef BYTECODE_OPCODES_H
#define BYTECODE_OPCODES_H

#include <stdbool.h>
#include <stdint.h>

/**
 * Unnarize Bytecode Instruction Set
 * 
 * Zero external dependencies - pure C implementation
 * Designed for maximum performance with computed goto dispatch
 */

typedef enum {
    // === Stack Operations ===
    OP_LOAD_IMM,        // Load 32-bit immediate value
    OP_LOAD_CONST,      // Load from constant pool
    OP_LOAD_NIL,        // Push nil
    OP_LOAD_TRUE,       // Push true
    OP_LOAD_FALSE,      // Push false
    OP_POP,             // Pop and discard
    OP_DUP,             // Duplicate top of stack
    
    // === Local Variables ===
    OP_LOAD_LOCAL,      // Load local variable (1-byte index)
    OP_STORE_LOCAL,     // Store to local variable
    OP_LOAD_LOCAL_0,    // Fast path for local 0
    OP_LOAD_LOCAL_1,    // Fast path for local 1
    OP_LOAD_LOCAL_2,    // Fast path for local 2
    
    // === Global Variables ===
    OP_LOAD_GLOBAL,     // Load global (with inline cache)
    OP_STORE_GLOBAL,    // Store global (with inline cache)
    OP_DEFINE_GLOBAL,   // Define new global
    
    // === Specialized Arithmetic (Type-Specific) ===
    OP_ADD_II,          // int + int (no type check!)
    OP_SUB_II,          // int - int
    OP_MUL_II,          // int * int
    OP_DIV_II,          // int / int
    OP_MOD_II,          // int % int
    OP_NEG_I,           // -int
    
    OP_ADD_FF,          // float + float
    OP_SUB_FF,          // float - float
    OP_MUL_FF,          // float * float
    OP_DIV_FF,          // float / float
    OP_NEG_F,           // -float
    
    // === Generic Arithmetic (Polymorphic with type checks) ===
    OP_ADD,             // Generic add (checks types, handles string concat)
    OP_SUB,             // Generic subtract
    OP_MUL,             // Generic multiply
    OP_DIV,             // Generic divide
    OP_MOD,             // Generic modulo
    OP_NEG,             // Generic negate
    
    // === Specialized Comparisons ===
    OP_LT_II,           // int < int
    OP_LE_II,           // int <= int
    OP_GT_II,           // int > int
    OP_GE_II,           // int >= int
    OP_EQ_II,           // int == int
    OP_NE_II,           // int != int
    
    OP_LT_FF,           // float < float
    OP_LE_FF,           // float <= float
    OP_GT_FF,           // float > float
    OP_GE_FF,           // float >= float
    OP_EQ_FF,           // float == float
    OP_NE_FF,           // float != float
    
    // === Generic Comparisons ===
    OP_LT,              // Generic <
    OP_LE,              // Generic <=
    OP_GT,              // Generic >
    OP_GE,              // Generic >=
    OP_EQ,              // Generic ==
    OP_NE,              // Generic !=
    
    // === Logical Operations ===
    OP_NOT,             // Logical NOT
    OP_AND,             // Logical AND (short-circuit)
    OP_OR,              // Logical OR (short-circuit)
    
    // === Control Flow ===
    OP_JUMP,            // Unconditional jump (2-byte offset)
    OP_JUMP_IF_FALSE,   // Jump if top is false (2-byte offset)
    OP_JUMP_IF_TRUE,    // Jump if top is true
    OP_LOOP,            // Backward jump for loops (2-byte offset)
    OP_LOOP_HEADER,     // Loop header marker (for OSR)
    
    // === Function Calls ===
    OP_CALL,            // Call function (1-byte arg count)
    OP_CALL_0,          // Optimized call with 0 args
    OP_CALL_1,          // Optimized call with 1 arg
    OP_CALL_2,          // Optimized call with 2 args
    OP_RETURN,          // Return from function
    OP_RETURN_NIL,      // Return nil (no value on stack)
    
    // === Object/Property Access ===
    OP_LOAD_PROPERTY,   // Load object property (with IC)
    OP_STORE_PROPERTY,  // Store object property (with IC)
    OP_LOAD_INDEX,      // Array/map index load
    OP_STORE_INDEX,     // Array/map index store
    
    // === Object Creation ===
    OP_NEW_ARRAY,       // Create new array (1-byte size)
    OP_NEW_MAP,         // Create new map
    OP_NEW_OBJECT,      // Create new object/struct
    
    // === Structs ===
    OP_STRUCT_DEF,      // Define struct (count byte + fields on stack)
    
    // === Array Operations ===
    OP_ARRAY_PUSH,      // Push to array
    OP_ARRAY_POP,       // Pop from array
    OP_ARRAY_LEN,       // Get array length
    

    
    // === Special ===
    OP_PRINT,           // Built-in print (debug)
    OP_HALT,            // Halt execution
    OP_NOP,             // No operation
    OP_ARRAY_PUSH_CLEAN, // Push to array and pop array from stack (Net -2)
    
    // === Total: ~100 opcodes for complete coverage ===
    OPCODE_COUNT
} OpCode;

// Opcode properties for disassembler and validation
typedef struct {
    const char* name;
    int operandBytes;   // Number of operand bytes
    bool hasSideEffect; // Does it modify state?
} OpcodeInfo;

// Get opcode metadata
const OpcodeInfo* getOpcodeInfo(OpCode op);

#endif // BYTECODE_OPCODES_H
