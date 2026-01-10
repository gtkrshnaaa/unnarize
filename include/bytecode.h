#ifndef BYTECODE_H
#define BYTECODE_H

#include "common.h"
#include "vm.h"

// Bytecode instruction set
// Compact, stack-based VM instructions for Unnarize
typedef enum {
    // === Stack Operations ===
    OP_CONSTANT,        // Push constant from constant pool (1 byte operand)
    OP_NIL,             // Push nil
    OP_TRUE,            // Push true
    OP_FALSE,           // Push false
    OP_POP,             // Pop and discard top value
    OP_DUP,             // Duplicate top of stack
    
    // === Variables ===
    OP_GET_LOCAL,       // Get local variable (1 byte slot index)
    OP_SET_LOCAL,       // Set local variable (1 byte slot index)
    OP_GET_GLOBAL,      // Get global variable (1 byte constant pool index for name)
    OP_SET_GLOBAL,      // Set global variable (1 byte constant pool index for name)
    OP_DEFINE_GLOBAL,   // Define new global variable
    
    // === Arithmetic (Polymorphic) ===
    OP_ADD,             // + (int/float/string concatenation)
    OP_SUBTRACT,        // -
    OP_MULTIPLY,        // *
    OP_DIVIDE,          // /
    OP_MODULO,          // %
    OP_NEGATE,          // Unary -
    
    // === Arithmetic (Type-Specialized for JIT) ===
    OP_ADD_INT,         // + for integers only (faster)
    OP_ADD_FLOAT,       // + for floats only
    OP_SUB_INT,         // - for integers only
    OP_MUL_INT,         // * for integers only
    OP_DIV_INT,         // / for integers only
    
    // === Comparison ===
    OP_EQUAL,           // ==
    OP_NOT_EQUAL,       // !=
    OP_GREATER,         // >
    OP_LESS,            // <
    OP_GREATER_EQUAL,   // >=
    OP_LESS_EQUAL,      // <=
    
    // === Logical ===
    OP_NOT,             // ! (logical not)
    OP_AND,             // && (short-circuit)
    OP_OR,              // || (short-circuit)
    
    // === Control Flow ===
    OP_JUMP,            // Unconditional jump (2 byte offset)
    OP_JUMP_IF_FALSE,   // Jump if top of stack is false (2 byte offset)
    OP_JUMP_IF_TRUE,    // Jump if top of stack is true (2 byte offset)
    OP_LOOP,            // Jump backward (for loops, 2 byte offset)
    
    // === Functions ===
    OP_CALL,            // Call function (1 byte arg count)
    OP_RETURN,          // Return from function
    OP_CLOSURE,         // Create closure (1 byte function index)
    
    // === Arrays ===
    OP_ARRAY_NEW,       // Create new empty array
    OP_ARRAY_PUSH,      // Push value to array
    OP_ARRAY_GET,       // Get array[index] (index and array on stack)
    OP_ARRAY_SET,       // Set array[index] = value
    OP_ARRAY_LENGTH,    // Get array length
    
    // === Maps/Hashes ===
    OP_MAP_NEW,         // Create new empty map
    OP_MAP_GET,         // Get map[key]
    OP_MAP_SET,         // Set map[key] = value
    OP_MAP_HAS,         // Check if key exists
    OP_MAP_DELETE,      // Delete key from map
    OP_MAP_KEYS,        // Get all keys as array
    
    // === Structs ===
    OP_STRUCT_DEF,      // Define struct type (1 byte constant pool index)
    OP_STRUCT_NEW,      // Create struct instance
    OP_GET_PROPERTY,    // obj.property (property name in constant pool)
    OP_SET_PROPERTY,    // obj.property = value
    
    // === Async ===
    OP_AWAIT,           // Await future value
    OP_ASYNC_CALL,      // Call async function (returns future)
    
    // === Built-ins ===
    OP_PRINT,           // Built-in print statement
    
    // === Special ===
    OP_HALT,            // End of program/chunk
    
    // === Metadata (for profiling/debugging) ===
    OP_LINE,            // Line number hint (2 bytes)
    OP_HOTSPOT_CHECK,   // Execution counter increment (for JIT triggering)
} OpCode;

// Bytecode chunk - sequence of instructions with metadata
typedef struct {
    uint8_t* code;          // Bytecode instructions
    int count;              // Number of bytes used
    int capacity;           // Allocated capacity
    
    Value* constants;       // Constant pool (literals, strings, etc)
    int constantCount;      // Number of constants
    int constantCapacity;   // Constant pool capacity
    
    int* lines;             // Line numbers for debugging (parallel to code)
    int lineCount;          // Number of line entries
    int lineCapacity;       // Line array capacity
    
    // For JIT integration
    int* hotspots;          // Execution counters for each instruction
    int hotspotCapacity;    // Hotspot array capacity
} Chunk;

// Initialize a new chunk
void initChunk(Chunk* chunk);

// Free chunk memory
void freeChunk(Chunk* chunk);

// Write a byte to chunk (instruction or operand)
void writeChunk(Chunk* chunk, uint8_t byte, int line);

// Add a constant to the constant pool, return its index
int addConstant(Chunk* chunk, Value value);

// === Disassembler for debugging ===

// Disassemble entire chunk
void disassembleChunk(Chunk* chunk, const char* name);

// Disassemble single instruction at offset, return next offset
int disassembleInstruction(Chunk* chunk, int offset);

#endif // BYTECODE_H
