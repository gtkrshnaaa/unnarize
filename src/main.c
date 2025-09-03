#include "common.h"
#include "lexer.h"
#include "parser.h"
#include "vm.h"

// Error function
void error(const char* message, int line) {
    fprintf(stderr, "[line %d] Error: %s\n", line, message);
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
        case NODE_STMT_LOADEXTERN:
            // free the expression used as path
            freeAST(node->loadexternStmt.pathExpr);
            break;
    }
    free(node);
}

int main(int argc, char* argv[]) {
    // Support version flags
    if (argc == 2 && (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0)) {
        printf("unnarize %s\n", UNNARIZE_VERSION);
        return 0;
    }

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file.unna>\n", argv[0]);
        fprintf(stderr, "       %s -v | --version\n", argv[0]);
        return 1;
    }

    char* source = readFile(argv[1]);

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
    interpret(&vm, ast);

    // Cleanup
    freeAST(ast);
    freeParser(&parser);
    freeVM(&vm);
    free(source);

    return 0;
}