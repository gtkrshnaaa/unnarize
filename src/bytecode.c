#include "bytecode.h"
#include <stdlib.h>
#include <stdio.h>

// Initialize a new bytecode chunk
void initChunk(Chunk* chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    
    chunk->constantCount = 0;
    chunk->constantCapacity = 0;
    chunk->constants = NULL;
    
    chunk->lineCount = 0;
    chunk->lineCapacity = 0;
    chunk->lines = NULL;
    
    chunk->hotspotCapacity = 0;
    chunk->hotspots = NULL;
}

// Free all memory used by chunk
void freeChunk(Chunk* chunk) {
    if (chunk->code != NULL) {
        free(chunk->code);
        chunk->code = NULL;
    }
    
    if (chunk->constants != NULL) {
        free(chunk->constants);
        chunk->constants = NULL;
    }
    
    if (chunk->lines != NULL) {
        free(chunk->lines);
        chunk->lines = NULL;
    }
    
    if (chunk->hotspots != NULL) {
        free(chunk->hotspots);
        chunk->hotspots = NULL;
    }
    
    initChunk(chunk);
}

// Write a byte to the chunk
void writeChunk(Chunk* chunk, uint8_t byte, int line) {
    // Grow code array if needed
    if (chunk->capacity < chunk->count + 1) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code = realloc(chunk->code, chunk->capacity);
        
        // Also grow hotspot array in parallel
        chunk->hotspots = realloc(chunk->hotspots, chunk->capacity * sizeof(int));
        
        // Initialize new hotspot counters to 0
        for (int i = oldCapacity; i < chunk->capacity; i++) {
            chunk->hotspots[i] = 0;
        }
        chunk->hotspotCapacity = chunk->capacity;
    }
    
    chunk->code[chunk->count] = byte;
    chunk->count++;
    
    // Record line number (run-length encoding optimization possible later)
    if (chunk->lineCapacity < chunk->lineCount + 1) {
        int oldCapacity = chunk->lineCapacity;
        chunk->lineCapacity = GROW_CAPACITY(oldCapacity);
        chunk->lines = realloc(chunk->lines, chunk->lineCapacity * sizeof(int));
    }
    
    chunk->lines[chunk->lineCount] = line;
    chunk->lineCount++;
}

// Add a constant to the constant pool
int addConstant(Chunk* chunk, Value value) {
    // Grow constants array if needed
    if (chunk->constantCapacity < chunk->constantCount + 1) {
        int oldCapacity = chunk->constantCapacity;
        chunk->constantCapacity = GROW_CAPACITY(oldCapacity);
        chunk->constants = realloc(chunk->constants, 
                                   chunk->constantCapacity * sizeof(Value));
    }
    
    chunk->constants[chunk->constantCount] = value;
    chunk->constantCount++;
    
    return chunk->constantCount - 1;
}

// === Disassembler Implementation ===

// Helper to print a simple instruction (no operands)
static int simpleInstruction(const char* name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

// Helper to print instruction with 1-byte operand
static int byteInstruction(const char* name, Chunk* chunk, int offset) {
    uint8_t slot = chunk->code[offset + 1];
    printf("%-16s %4d\n", name, slot);
    return offset + 2;
}

// Helper to print instruction with constant pool reference
static int constantInstruction(const char* name, Chunk* chunk, int offset) {
    uint8_t constantIndex = chunk->code[offset + 1];
    printf("%-16s %4d '", name, constantIndex);
    
    // Print the constant value
    Value constant = chunk->constants[constantIndex];
    switch (constant.type) {
        case VAL_INT:
            printf("%ld", constant.as.integer);
            break;
        case VAL_FLOAT:
            printf("%g", constant.as.floatNum);
            break;
        case VAL_STRING:
            printf("%s", constant.as.string->chars);
            break;
        default:
            printf("<value>");
            break;
    }
    printf("'\n");
    
    return offset + 2;
}

// Helper to print jump instruction
static int jumpInstruction(const char* name, int sign, Chunk* chunk, int offset) {
    uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
    jump |= chunk->code[offset + 2];
    printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
    return offset + 3;
}

// Disassemble a single instruction
int disassembleInstruction(Chunk* chunk, int offset) {
    printf("%04d ", offset);
    
    // Show line number
    if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
        printf("   | ");
    } else {
        printf("%4d ", chunk->lines[offset]);
    }
    
    // Show hotspot count
    printf("[%6d] ", chunk->hotspots[offset]);
    
    uint8_t instruction = chunk->code[offset];
    
    switch (instruction) {
        // Stack operations
        case OP_CONSTANT:
            return constantInstruction("OP_CONSTANT", chunk, offset);
        case OP_NIL:
            return simpleInstruction("OP_NIL", offset);
        case OP_TRUE:
            return simpleInstruction("OP_TRUE", offset);
        case OP_FALSE:
            return simpleInstruction("OP_FALSE", offset);
        case OP_POP:
            return simpleInstruction("OP_POP", offset);
        case OP_DUP:
            return simpleInstruction("OP_DUP", offset);
            
        // Variables
        case OP_GET_LOCAL:
            return byteInstruction("OP_GET_LOCAL", chunk, offset);
        case OP_SET_LOCAL:
            return byteInstruction("OP_SET_LOCAL", chunk, offset);
        case OP_GET_GLOBAL:
            return constantInstruction("OP_GET_GLOBAL", chunk, offset);
        case OP_SET_GLOBAL:
            return constantInstruction("OP_SET_GLOBAL", chunk, offset);
        case OP_DEFINE_GLOBAL:
            return constantInstruction("OP_DEFINE_GLOBAL", chunk, offset);
            
        // Arithmetic
        case OP_ADD:
            return simpleInstruction("OP_ADD", offset);
        case OP_SUBTRACT:
            return simpleInstruction("OP_SUBTRACT", offset);
        case OP_MULTIPLY:
            return simpleInstruction("OP_MULTIPLY", offset);
        case OP_DIVIDE:
            return simpleInstruction("OP_DIVIDE", offset);
        case OP_MODULO:
            return simpleInstruction("OP_MODULO", offset);
        case OP_NEGATE:
            return simpleInstruction("OP_NEGATE", offset);
            
        // Specialized arithmetic
        case OP_ADD_INT:
            return simpleInstruction("OP_ADD_INT", offset);
        case OP_ADD_FLOAT:
            return simpleInstruction("OP_ADD_FLOAT", offset);
        case OP_SUB_INT:
            return simpleInstruction("OP_SUB_INT", offset);
        case OP_MUL_INT:
            return simpleInstruction("OP_MUL_INT", offset);
        case OP_DIV_INT:
            return simpleInstruction("OP_DIV_INT", offset);
            
        // Comparison
        case OP_EQUAL:
            return simpleInstruction("OP_EQUAL", offset);
        case OP_NOT_EQUAL:
            return simpleInstruction("OP_NOT_EQUAL", offset);
        case OP_GREATER:
            return simpleInstruction("OP_GREATER", offset);
        case OP_LESS:
            return simpleInstruction("OP_LESS", offset);
        case OP_GREATER_EQUAL:
            return simpleInstruction("OP_GREATER_EQUAL", offset);
        case OP_LESS_EQUAL:
            return simpleInstruction("OP_LESS_EQUAL", offset);
            
        // Logical
        case OP_NOT:
            return simpleInstruction("OP_NOT", offset);
        case OP_AND:
            return simpleInstruction("OP_AND", offset);
        case OP_OR:
            return simpleInstruction("OP_OR", offset);
            
        // Control flow
        case OP_JUMP:
            return jumpInstruction("OP_JUMP", 1, chunk, offset);
        case OP_JUMP_IF_FALSE:
            return jumpInstruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
        case OP_JUMP_IF_TRUE:
            return jumpInstruction("OP_JUMP_IF_TRUE", 1, chunk, offset);
        case OP_LOOP:
            return jumpInstruction("OP_LOOP", -1, chunk, offset);
            
        // Functions
        case OP_CALL:
            return byteInstruction("OP_CALL", chunk, offset);
        case OP_RETURN:
            return simpleInstruction("OP_RETURN", offset);
        case OP_CLOSURE:
            return byteInstruction("OP_CLOSURE", chunk, offset);
            
        // Arrays
        case OP_ARRAY_NEW:
            return simpleInstruction("OP_ARRAY_NEW", offset);
        case OP_ARRAY_PUSH:
            return simpleInstruction("OP_ARRAY_PUSH", offset);
        case OP_ARRAY_GET:
            return simpleInstruction("OP_ARRAY_GET", offset);
        case OP_ARRAY_SET:
            return simpleInstruction("OP_ARRAY_SET", offset);
        case OP_ARRAY_LENGTH:
            return simpleInstruction("OP_ARRAY_LENGTH", offset);
            
        // Maps
        case OP_MAP_NEW:
            return simpleInstruction("OP_MAP_NEW", offset);
        case OP_MAP_GET:
            return simpleInstruction("OP_MAP_GET", offset);
        case OP_MAP_SET:
            return simpleInstruction("OP_MAP_SET", offset);
        case OP_MAP_HAS:
            return simpleInstruction("OP_MAP_HAS", offset);
        case OP_MAP_DELETE:
            return simpleInstruction("OP_MAP_DELETE", offset);
        case OP_MAP_KEYS:
            return simpleInstruction("OP_MAP_KEYS", offset);
            
        // Structs
        case OP_STRUCT_DEF:
            return constantInstruction("OP_STRUCT_DEF", chunk, offset);
        case OP_STRUCT_NEW:
            return simpleInstruction("OP_STRUCT_NEW", offset);
        case OP_GET_PROPERTY:
            return constantInstruction("OP_GET_PROPERTY", chunk, offset);
        case OP_SET_PROPERTY:
            return constantInstruction("OP_SET_PROPERTY", chunk, offset);
            
        // Async
        case OP_AWAIT:
            return simpleInstruction("OP_AWAIT", offset);
        case OP_ASYNC_CALL:
            return byteInstruction("OP_ASYNC_CALL", chunk, offset);
            
        // Built-ins
        case OP_PRINT:
            return simpleInstruction("OP_PRINT", offset);
            
        // Special
        case OP_HALT:
            return simpleInstruction("OP_HALT", offset);
        case OP_LINE:
            return byteInstruction("OP_LINE", chunk, offset);
        case OP_HOTSPOT_CHECK:
            return simpleInstruction("OP_HOTSPOT_CHECK", offset);
            
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}

// Disassemble entire chunk
void disassembleChunk(Chunk* chunk, const char* name) {
    printf("== %s ==\n", name);
    printf("Offset Line [Hotspot] Instruction\n");
    printf("------ ---- ---------- -----------\n");
    
    for (int offset = 0; offset < chunk->count;) {
        offset = disassembleInstruction(chunk, offset);
    }
    
    printf("\nConstants Pool (%d):\n", chunk->constantCount);
    for (int i = 0; i < chunk->constantCount; i++) {
        printf("  [%d] ", i);
        Value constant = chunk->constants[i];
        switch (constant.type) {
            case VAL_INT:
                printf("%ld\n", constant.as.integer);
                break;
            case VAL_FLOAT:
                printf("%g\n", constant.as.floatNum);
                break;
            case VAL_STRING:
                printf("\"%s\"\n", constant.as.string->chars);
                break;
            default:
                printf("<value>\n");
                break;
        }
    }
}
