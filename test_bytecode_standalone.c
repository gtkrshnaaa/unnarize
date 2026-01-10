#include <stdio.h>
#include "parser.h"
#include "bytecode/compiler.h"
#include "bytecode/chunk.h"
#include "bytecode/interpreter.h"
#include "vm.h"

int main() {
    // Test bytecode compilation and execution
    const char* source = "var x = 0; while (x < 1000000) { x = x + 1; }";
    
    printf("=== BYTECODE VM TEST ===\n");
    printf("Source: %s\n\n", source);
    
    // Lex
    Lexer lexer;
    initLexer(&lexer, (char*)source);
    
    Parser parser;
    initParser(&parser);
    
    while (true) {
        Token tok = scanToken(&lexer);
        addToken(&parser, tok);
        if (tok.type == TOKEN_EOF) break;
    }
    
    // Parse
    Node* ast = parse(&parser);
    
    // Compile to bytecode
    BytecodeChunk chunk;
    initChunk(&chunk);
    
    if (compileToBytecode(ast, &chunk)) {
        printf("✓ Bytecode compilation successful\n");
        printf("  Code size: %d bytes\n", chunk.codeSize);
        printf("  Constants: %d\n\n", chunk.constantCount);
        
        // Disassemble
        printf("=== DISASSEMBLY ===\n");
        disassembleChunk(&chunk, "test");
        
        // Execute
        printf("\n=== EXECUTION ===\n");
        VM vm;
        initVM(&vm);
        executeBytecode(&vm, &chunk);
        
        freeChunk(&chunk);
        freeVM(&vm);
    } else {
        printf("✗ Compilation failed\n");
    }
    
    return 0;
}
