#include "common.h"
#include <unistd.h>
#include "lexer.h"
#include "parser.h"
#include "vm.h"
#include "ucore_uon.h"
#include "ucore_http.h"
#include "ucore_timer.h"
#include "ucore_system.h"
#include "ucore_json.h"

#include "bytecode/chunk.h"
#include "bytecode/compiler.h"
#include "bytecode/interpreter.h"

const char* g_source = NULL;
const char* g_filename = NULL;

// Helper to print error line with context
static void printErrorLine(int line, const char* highlightStart, int highlightLen) {
    if (!g_source || line <= 0) return;

    // Find the start of the line
    const char* start = g_source;
    int currentLine = 1;
    while (currentLine < line && *start != '\0') {
        if (*start == '\n') currentLine++;
        start++;
    }

    if (*start == '\0') return; // Line not found

    // Find end of line
    const char* end = start;
    while (*end != '\0' && *end != '\n') {
        end++;
    }

    // Print the code frame
    // Line number margin
    fprintf(stderr, "\n");
    fprintf(stderr, "   %4d | ", line);
    
    // Print the line content
    fprintf(stderr, "%.*s\n", (int)(end - start), start);

    // Print highlight arrow
    if (highlightStart && highlightLen > 0) {
        fprintf(stderr, "          "); // Match "   %4d | " length (approximately 10 chars: 3+4+3)
        // Adjust for indentation to match the code line
        // Calculate offset from line start
        int offset = (int)(highlightStart - start);
        if (offset >= 0 && offset < (end - start)) {
            for (int i = 0; i < offset; i++) {
                if (start[i] == '\t') fprintf(stderr, "\t");
                else fprintf(stderr, " ");
            }
            // Print caret(s)
            for (int i = 0; i < highlightLen; i++) fprintf(stderr, "^");
            fprintf(stderr, "\n");
        }
    }
    fprintf(stderr, "\n");
}

// Generic error
void error(const char* message, int line) {
    fprintf(stderr, "Error in %s at line %d:\n", g_filename ? g_filename : "<unknown>", line);
    fprintf(stderr, "  %s\n", message);
    printErrorLine(line, NULL, 0);
    exit(1);
}

// Error at specific token
void errorAtToken(Token token, const char* message) {
    fprintf(stderr, "Error in %s at line %d:\n", g_filename ? g_filename : "<unknown>", token.line);
    fprintf(stderr, "  %s\n", message);
    printErrorLine(token.line, token.start, token.length);
    exit(1);
}

// Read file
static char* readFile(const char* path) {
    FILE* file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(1);
    }

    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    char* buffer = malloc(fileSize + 1);
    if (!buffer) {
        fclose(file);
        fprintf(stderr, "Memory allocation failed.\n");
        exit(1);
    }
    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    if (bytesRead < fileSize) {
        free(buffer);
        fclose(file);
        fprintf(stderr, "Could not read file \"%s\".\n", path);
        exit(1);
    }

    buffer[bytesRead] = '\0';
    fclose(file);
    return buffer;
}

// Initialize parser
void initParser(Parser* parser) {
    parser->tokens = malloc(64 * sizeof(Token));  // Start with 64 tokens
    if (!parser->tokens) {
        error("Memory allocation failed.", 0);
    }
    parser->current = 0;
    parser->count = 0;
    parser->capacity = 64;
}

// Free parser resources
void freeParser(Parser* parser) {
    if (parser->tokens) {
        free(parser->tokens);
        parser->tokens = NULL;
    }
}

// Add token to parser (grows array as needed)
void addToken(Parser* parser, Token token) {
    // Grow array if needed
    if (parser->count >= parser->capacity) {
        parser->capacity *= 2;
        parser->tokens = realloc(parser->tokens, parser->capacity * sizeof(Token));
        if (!parser->tokens) {
            error("Memory allocation failed.", token.line);
        }
    }
    
    parser->tokens[parser->count++] = token;
}

// Free AST
static void freeAST(Node* node) {
    if (!node) return;
    switch (node->type) {
        case NODE_EXPR_LITERAL:
            if (node->literal.token.type == TOKEN_STRING) {
                // No need to free token.start, it's from source
            }
            break;
        case NODE_EXPR_BINARY:
            freeAST(node->binary.left);
            freeAST(node->binary.right);
            break;
        case NODE_EXPR_UNARY:
            freeAST(node->unary.expr);
            break;
        case NODE_EXPR_AWAIT:
            // await <expr> -> free inner expression
            freeAST(node->unary.expr);
            break;
        case NODE_EXPR_VAR:
            break;
        case NODE_EXPR_GET:
            // object.property -> free object
            freeAST(node->get.object);
            break;
        case NODE_EXPR_INDEX:
            // target[index] -> free both
            freeAST(node->index.target);
            freeAST(node->index.index);
            break;
        case NODE_EXPR_CALL:
            freeAST(node->call.arguments);
            break;
        case NODE_STMT_VAR_DECL:
            freeAST(node->varDecl.initializer);
            break;
        case NODE_STMT_ASSIGN:
            freeAST(node->assign.value);
            break;
        case NODE_STMT_INDEX_ASSIGN:
            // target[index] = value
            freeAST(node->indexAssign.target);
            freeAST(node->indexAssign.index);
            freeAST(node->indexAssign.value);
            break;
        case NODE_STMT_PRINT:
            freeAST(node->print.expr);
            break;
        case NODE_STMT_IF:
            freeAST(node->ifStmt.condition);
            freeAST(node->ifStmt.thenBranch);
            freeAST(node->ifStmt.elseBranch);
            break;
        case NODE_STMT_WHILE:
            freeAST(node->whileStmt.condition);
            freeAST(node->whileStmt.body);
            break;
        case NODE_STMT_FOR:
            freeAST(node->forStmt.initializer);
            freeAST(node->forStmt.condition);
            freeAST(node->forStmt.increment);
            freeAST(node->forStmt.body);
            break;
        case NODE_STMT_FOREACH:
            freeAST(node->foreachStmt.collection);
            freeAST(node->foreachStmt.body);
            break;
        case NODE_STMT_STRUCT_DECL:
            if (node->structDecl.fields) {
                free(node->structDecl.fields);
            }
            break;
        case NODE_STMT_PROP_ASSIGN:
            freeAST(node->propAssign.object);
            freeAST(node->propAssign.value);
            break;
        case NODE_STMT_BLOCK:
            for (int i = 0; i < node->block.count; i++) {
                freeAST(node->block.statements[i]);
            }
            free(node->block.statements);
            break;
        case NODE_STMT_FUNCTION:
            freeAST(node->function.body);
            free(node->function.params);
            break;
        case NODE_STMT_RETURN:
            freeAST(node->returnStmt.value);
            break;
        case NODE_STMT_IMPORT:
            // tokens only; nothing to free
            break;

        case NODE_EXPR_ARRAY_LITERAL:
            // Free elements linked list
            {
                Node* current = node->arrayLiteral.elements;
                while (current) {
                    Node* next = current->next;
                    freeAST(current);
                    current = next; // current->next pointer logic is slightly weird here as freeAST frees the node itself.
                }
            }
            break;
    }
    free(node);
}

int main(int argc, char** argv) {
    // Support version flags
    if (argc == 2 && (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0)) {
        printf("unnarize %s\n", UNNARIZE_VERSION);
        return 0;
    }

    if (argc < 2) {
        fprintf(stderr, "Usage: %s [--opt] <file.unna>\n", argv[0]);
        fprintf(stderr, "       %s -v | --version\n", argv[0]);
        return 1;
    }
    
    
    // Parse arguments (keeping for future flags if needed)
    char* filename = NULL;
    
    for (int i = 1; i < argc; i++) {
        if (filename == NULL) {
            filename = argv[i];
        }
    }
    
    if (!filename) {
        fprintf(stderr, "Error: No input file specified\n");
        return 1;
    }

    g_filename = filename;
    char* source = readFile(filename);
    g_source = source;

    // Lexer
    Lexer lexer;
    initLexer(&lexer, source);

    // Initialize parser with dynamic array
    Parser parser;
    initParser(&parser);
    
    // Tokenize - no more token limit!
    while (true) {
        Token token = scanToken(&lexer);
        addToken(&parser, token);
        if (token.type == TOKEN_EOF) break;
    }

    printf("Tokenized %d tokens successfully.\n", parser.count);

    // Parse
    Node* ast = parse(&parser);

    // VM
    VM vm;
    initVM(&vm);
    vm.argc = argc;
    vm.argv = argv;

    char* projectRoot = getenv("UNNARIZE_ROOT");
    if (projectRoot) {
        strncpy(vm.projectRoot, projectRoot, 1023);
    } else {
        if (getcwd(vm.projectRoot, 1024) == NULL) {
            fprintf(stderr, "Check for getcwd failed.");
        }
    }

    registerUCoreUON(&vm); // Register built-in core libraries
    registerUCoreHttp(&vm);
    registerUCoreTimer(&vm); // Register Timer
    registerUCoreJson(&vm);  // Register Json
    registerUCoreScraper(&vm); // Register Scraper

    registerUCoreSystem(&vm); // Register System
    registerBuiltins(&vm);    // Register built-in natives (has, keys)
    

    
    // VM Execution
    BytecodeChunk* chunk = malloc(sizeof(BytecodeChunk));
    initChunk(chunk);
    
    // Allocate script function to root constants during compilation
    Function* script = (Function*)ALLOCATE_OBJ(&vm, Function, OBJ_FUNCTION);
    script->paramCount = 0;
    script->name.start = "<script>";
    script->name.length = 8;
    script->name.line = 0;
    script->body = NULL;
    script->closure = NULL;
    script->isNative = false;
    script->bytecodeChunk = chunk;
    script->modulePath = g_filename ? strdup(g_filename) : NULL;
    
    // Root script on stack
    vm.stack[vm.stackTop++] = OBJ_VAL(script);
    
    if (compileToBytecode(&vm, ast, chunk, g_filename)) {
        // Setup CallFrame
        if (vm.callStackTop < CALL_STACK_MAX) {
            CallFrame* frame = &vm.callStack[vm.callStackTop++];
            frame->function = script;
            frame->chunk = chunk;
            frame->ip = chunk->code;
            frame->env = vm.globalEnv; // Bind global env
            frame->fp = 0;
        }
        
        // Execute VM
        executeBytecode(&vm, chunk, 0);
        
        vm.callStackTop--; // Pop frame
    } else {
        fprintf(stderr, "Bytecode compilation failed.\n");
        exit(1);
    }
    
    vm.stackTop--; // Pop script (script will be freed by freeVM -> freeObject)
    // Don't free chunk here - it will be freed when script Function is freed


    // Cleanup
    freeAST(ast);
    freeParser(&parser);
    freeVM(&vm);
    free(source);

    return 0;
}