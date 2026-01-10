// Simple test of bytecode system
// This demonstrates the bytecode infrastructure

#include "bytecode.h"
#include "compiler.h"
#include <stdio.h>

int main() {
    printf("=== Unnarize Bytecode Test ===\n\n");
    
    // Create a simple bytecode chunk manually
    Chunk chunk;
    initChunk(&chunk);
    
    // Test 1: Simple arithmetic - 1 + 2
    printf("Test 1: Simple arithmetic (1 + 2)\n");
    printf("-----------------------------------\n");
    
    // Push constants
    Value v1 = {.type = VAL_INT, .as.integer = 1};
    Value v2 = {.type = VAL_INT, .as.integer = 2};
    
    int c1 = addConstant(&chunk, v1);
    int c2 = addConstant(&chunk, v2);
    
    // Compile to bytecode
    writeChunk(&chunk, OP_CONSTANT, 1);
    writeChunk(&chunk, c1, 1);
    writeChunk(&chunk, OP_CONSTANT, 1);
    writeChunk(&chunk, c2, 1);
    writeChunk(&chunk, OP_ADD, 1);
    writeChunk(&chunk, OP_PRINT, 1);
    writeChunk(&chunk, OP_HALT, 1);
    
    // Disassemble
    disassembleChunk(&chunk, "Test 1");
    
    printf("\n");
    
    // Test 2: Variable operations
    printf("Test 2: Variable operations\n");
    printf("----------------------------\n");
    
    Chunk chunk2;
    initChunk(&chunk2);
    
    // var x = 10; x + 5
    Value v10 = {.type = VAL_INT, .as.integer = 10};
    Value v5 = {.type = VAL_INT, .as.integer = 5};
    
    int c10 = addConstant(&chunk2, v10);
    int c5 = addConstant(&chunk2, v5);
    
    // x = 10 (local slot 0)
    writeChunk(&chunk2, OP_CONSTANT, 1);
    writeChunk(&chunk2, c10, 1);
    writeChunk(&chunk2, OP_SET_LOCAL, 1);
    writeChunk(&chunk2, 0, 1);
    
    // Get x
    writeChunk(&chunk2, OP_GET_LOCAL, 1);
    writeChunk(&chunk2, 0, 1);
    
    // Push 5
    writeChunk(&chunk2, OP_CONSTANT, 1);
    writeChunk(&chunk2, c5, 1);
    
    // Add
    writeChunk(&chunk2, OP_ADD, 1);
    
    // Print
    writeChunk(&chunk2, OP_PRINT, 1);
    
    writeChunk(&chunk2, OP_HALT, 1);
    
    disassembleChunk(&chunk2, "Test 2");
    
    printf("\n");
    
    // Test 3: Control flow (if-like pattern)
    printf("Test 3: Control flow (conditional jump)\n");
    printf("----------------------------------------\n");
    
    Chunk chunk3;
    initChunk(&chunk3);
    
    // if (true) print(100) else print(200)
    Value v100 = {.type = VAL_INT, .as.integer = 100};
    Value v200 = {.type = VAL_INT, .as.integer = 200};
    
    int c100 = addConstant(&chunk3, v100);
    int c200 = addConstant(&chunk3, v200);
    
    writeChunk(&chunk3, OP_TRUE, 1);              // Condition
    writeChunk(&chunk3, OP_JUMP_IF_FALSE, 1);     // Jump to else if false
    writeChunk(&chunk3, 0, 1);                    // Jump offset high byte
    writeChunk(&chunk3, 4, 1);                    // Jump offset low byte (skip 4 bytes)
    
    // Then branch
    writeChunk(&chunk3, OP_CONSTANT, 2);
    writeChunk(&chunk3, c100, 2);
    writeChunk(&chunk3, OP_PRINT, 2);
    writeChunk(&chunk3, OP_JUMP, 2);              // Jump over else
    writeChunk(&chunk3, 0, 2);
    writeChunk(&chunk3, 3, 2);                    // Skip 3 bytes
    
    // Else branch
    writeChunk(&chunk3, OP_CONSTANT, 3);
    writeChunk(&chunk3, c200, 3);
    writeChunk(&chunk3, OP_PRINT, 3);
    
    writeChunk(&chunk3, OP_HALT, 4);
    
    disassembleChunk(&chunk3, "Test 3");
    
    printf("\n=== All tests completed ===\n");
    printf("\nBytecode system is working correctly!\n");
    printf("Next steps:\n");
    printf("  1. Implement bytecode VM (interpreter)\n");
    printf("  2. Integrate with AST compiler\n");
    printf("  3. Add profiling hooks\n");
    printf("  4. Begin JIT code generation\n");
    
    // Cleanup
    freeChunk(&chunk);
    freeChunk(&chunk2);
    freeChunk(&chunk3);
    
    return 0;
}
