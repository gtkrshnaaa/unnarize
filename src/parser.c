#include "parser.h"

// Helper to advance parser
static Token advance(Parser* parser) {
    if (parser->current < parser->count) {
        return parser->tokens[parser->current++];
    }
    return (Token){TOKEN_EOF, NULL, 0, 0};
}

// Check current token type
static bool check(Parser* parser, TokenType type) {
    if (parser->current >= parser->count) return false;
    return parser->tokens[parser->current].type == type;
}

// Match and advance if type
static bool match(Parser* parser, TokenType type) {
    if (check(parser, type)) {
        advance(parser);
        return true;
    }
    return false;
}

// Consume token or error
static Token consume(Parser* parser, TokenType type, const char* message) {
    if (check(parser, type)) return advance(parser);
    error(message, parser->tokens[parser->current].line);
    return (Token){TOKEN_EOF, NULL, 0, 0};
}

// Forward declarations for recursive parsing
static Node* expression(Parser* parser);
static Node* statement(Parser* parser);
static Node* declaration(Parser* parser);

// Forward for postfix
static Node* finishPostfix(Parser* parser, Node* expr);

// Parse primary (literals, vars, groups)
static Node* primary(Parser* parser) {
    if (match(parser, TOKEN_NUMBER) || match(parser, TOKEN_STRING) || 
        match(parser, TOKEN_TRUE) || match(parser, TOKEN_FALSE)) {
        Node* node = malloc(sizeof(Node));
        node->next = NULL;
        node->type = NODE_EXPR_LITERAL;
        node->literal.token = parser->tokens[parser->current - 1];
        return node;
    }
    if (match(parser, TOKEN_LEFT_BRACKET)) {
        // Array literal [e1, e2, ...]
        Node* node = malloc(sizeof(Node));
        node->next = NULL;
        node->type = NODE_EXPR_ARRAY_LITERAL;
        node->arrayLiteral.elements = NULL;
        node->arrayLiteral.count = 0;

        if (!check(parser, TOKEN_RIGHT_BRACKET)) {
            Node** currentElem = &node->arrayLiteral.elements;
            do {
                *currentElem = expression(parser);
                node->arrayLiteral.count++;
                currentElem = &(*currentElem)->next;
            } while (match(parser, TOKEN_COMMA));
        }
        consume(parser, TOKEN_RIGHT_BRACKET, "Expect ']' after array literal.");
        return finishPostfix(parser, node);
    }
    if (match(parser, TOKEN_IDENTIFIER)) {
        // Variable reference base
        Node* node = malloc(sizeof(Node));
        node->next = NULL;
        node->type = NODE_EXPR_VAR;
        node->var.name = parser->tokens[parser->current - 1];
        return finishPostfix(parser, node);
    }
    if (match(parser, TOKEN_LEFT_PAREN)) {
        Node* expr = expression(parser);
        consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
        return finishPostfix(parser, expr);
    }
    error("Expect expression.", parser->tokens[parser->current].line);
    return NULL;
}

// Unary
static Node* unary(Parser* parser) {
    if (match(parser, TOKEN_AWAIT)) {
        // await expression
        Node* expr = unary(parser);
        Node* node = malloc(sizeof(Node));
        node->next = NULL;
        node->type = NODE_EXPR_AWAIT;
        node->unary.op = parser->tokens[parser->current - 1]; // store 'await' token
        node->unary.expr = expr;
        return node;
    }
    if (match(parser, TOKEN_MINUS) || match(parser, TOKEN_PLUS)) {
        Token op = parser->tokens[parser->current - 1];
        Node* expr = unary(parser);
        Node* node = malloc(sizeof(Node));
        node->next = NULL;
        node->type = NODE_EXPR_UNARY;
        node->unary.op = op;
        node->unary.expr = expr;
        return node;
    }
    return primary(parser);
}

// Postfix: calls, member access, indexing (left-assoc, chainable)
static Node* finishPostfix(Parser* parser, Node* expr) {
    for (;;) {
        if (match(parser, TOKEN_LEFT_PAREN)) {
            Node* call = malloc(sizeof(Node));
            call->next = NULL;
            call->type = NODE_EXPR_CALL;
            call->call.callee = expr;
            call->call.arguments = NULL;
            call->call.argumentCount = 0;

            if (!check(parser, TOKEN_RIGHT_PAREN)) {
                Node** currentArg = &call->call.arguments;
                do {
                    *currentArg = expression(parser);
                    call->call.argumentCount++;
                    currentArg = &(*currentArg)->next;
                } while (match(parser, TOKEN_COMMA));
            }
            consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
            expr = call;
        } else if (match(parser, TOKEN_DOT)) {
            Token name = consume(parser, TOKEN_IDENTIFIER, "Expect property name after '.'.");
            Node* get = malloc(sizeof(Node));
            get->next = NULL;
            get->type = NODE_EXPR_GET;
            get->get.object = expr;
            get->get.name = name;
            expr = get;
        } else if (match(parser, TOKEN_LEFT_BRACKET)) {
            Node* indexExpr = expression(parser);
            consume(parser, TOKEN_RIGHT_BRACKET, "Expect ']' after index expression.");
            Node* idx = malloc(sizeof(Node));
            idx->next = NULL;
            idx->type = NODE_EXPR_INDEX;
            idx->index.target = expr;
            idx->index.index = indexExpr;
            expr = idx;
        } else {
            break;
        }
    }
    return expr;
}

// Factor (* / %)
static Node* factor(Parser* parser) {
    Node* expr = unary(parser);
    while (match(parser, TOKEN_STAR) || match(parser, TOKEN_SLASH) || match(parser, TOKEN_PERCENT)) {
        Token op = parser->tokens[parser->current - 1];
        Node* right = unary(parser);
        Node* node = malloc(sizeof(Node));
        node->next = NULL;
        node->type = NODE_EXPR_BINARY;
        node->binary.left = expr;
        node->binary.op = op;
        node->binary.right = right;
        expr = node;
    }
    return expr;
}

// Term (+ -)
static Node* term(Parser* parser) {
    Node* expr = factor(parser);
    while (match(parser, TOKEN_PLUS) || match(parser, TOKEN_MINUS)) {
        Token op = parser->tokens[parser->current - 1];
        Node* right = factor(parser);
        Node* node = malloc(sizeof(Node));
        node->type = NODE_EXPR_BINARY;
        node->binary.left = expr;
        node->binary.op = op;
        node->binary.right = right;
        expr = node;
    }
    return expr;
}

// Comparison
static Node* comparison(Parser* parser) {
    Node* expr = term(parser);
    while (match(parser, TOKEN_GREATER) || match(parser, TOKEN_GREATER_EQUAL) ||
           match(parser, TOKEN_LESS) || match(parser, TOKEN_LESS_EQUAL)) {
        Token op = parser->tokens[parser->current - 1];
        Node* right = term(parser);
        Node* node = malloc(sizeof(Node));
        node->type = NODE_EXPR_BINARY;
        node->binary.left = expr;
        node->binary.op = op;
        node->binary.right = right;
        expr = node;
    }
    return expr;
}

// Equality
static Node* equality(Parser* parser) {
    Node* expr = comparison(parser);
    while (match(parser, TOKEN_EQUAL_EQUAL) || match(parser, TOKEN_BANG_EQUAL)) {
        Token op = parser->tokens[parser->current - 1];
        Node* right = comparison(parser);
        Node* node = malloc(sizeof(Node));
        node->type = NODE_EXPR_BINARY;
        node->binary.left = expr;
        node->binary.op = op;
        node->binary.right = right;
        expr = node;
    }
    return expr;
}

// Assignment (for var = expr)
static Node* assignment(Parser* parser) {
    Node* expr = equality(parser);
    if (match(parser, TOKEN_EQUAL)) {
        Token equals = parser->tokens[parser->current - 1];
        Node* value = assignment(parser); // Right-assoc
        if (expr->type == NODE_EXPR_VAR) {
            Node* node = malloc(sizeof(Node));
            node->next = NULL;
            node->type = NODE_STMT_ASSIGN;
            node->assign.name = expr->var.name;
            node->assign.value = value;
            return node;
        } else if (expr->type == NODE_EXPR_INDEX) {
            Node* node = malloc(sizeof(Node));
            node->next = NULL;
            node->type = NODE_STMT_INDEX_ASSIGN;
            node->indexAssign.target = expr->index.target;
            node->indexAssign.index = expr->index.index;
            node->indexAssign.value = value;
            return node;
        }
        error("Invalid assignment target.", equals.line);
    }
    return expr;
}

// Expression (top level)
static Node* expression(Parser* parser) {
    return assignment(parser);
}

// Block { ... }
static Node* block(Parser* parser) {
    Node* node = malloc(sizeof(Node));
    node->next = NULL;
    node->type = NODE_STMT_BLOCK;
    node->block.statements = malloc(8 * sizeof(Node*));
    node->block.count = 0;
    node->block.capacity = 8;

    while (!check(parser, TOKEN_RIGHT_BRACE) && !check(parser, TOKEN_EOF)) {
        Node* decl = declaration(parser);
        if (node->block.count == node->block.capacity) {
            node->block.capacity *= 2;
            node->block.statements = realloc(node->block.statements, node->block.capacity * sizeof(Node*));
        }
        node->block.statements[node->block.count++] = decl;
    }

    consume(parser, TOKEN_RIGHT_BRACE, "Expect '}' after block.");
    return node;
}

// Function declaration
static Node* function(Parser* parser) {
    Token name = consume(parser, TOKEN_IDENTIFIER, "Expect function name.");
    consume(parser, TOKEN_LEFT_PAREN, "Expect '(' after function name.");

    Node* node = malloc(sizeof(Node));
    node->next = NULL;
    node->type = NODE_STMT_FUNCTION;
    node->function.name = name;
    node->function.params = malloc(8 * sizeof(Token));
    node->function.paramCount = 0;
    int capacity = 8;

    if (!check(parser, TOKEN_RIGHT_PAREN)) {
        do {
            if (node->function.paramCount == capacity) {
                capacity *= 2;
                node->function.params = realloc(node->function.params, capacity * sizeof(Token));
            }
            node->function.params[node->function.paramCount++] = consume(parser, TOKEN_IDENTIFIER, "Expect parameter name.");
        } while (match(parser, TOKEN_COMMA));
    }

    consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    consume(parser, TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    node->function.body = block(parser);
    // isAsync is set by caller (declaration) when seeing 'async function'
    return node;
}

// Var declaration
static Node* varDeclaration(Parser* parser) {
    Token name = consume(parser, TOKEN_IDENTIFIER, "Expect variable name.");

    Node* initializer = NULL;
    if (match(parser, TOKEN_EQUAL)) {
        initializer = expression(parser);
    }

    consume(parser, TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

    Node* node = malloc(sizeof(Node));
    node->next = NULL;
    node->type = NODE_STMT_VAR_DECL;
    node->varDecl.name = name;
    node->varDecl.initializer = initializer;
    return node;
}

// Print statement
static Node* printStatement(Parser* parser) {
    Node* expr = expression(parser);
    consume(parser, TOKEN_SEMICOLON, "Expect ';' after print value.");

    Node* node = malloc(sizeof(Node));
    node->next = NULL;
    node->type = NODE_STMT_PRINT;
    node->print.expr = expr;
    return node;
}

// Return statement
static Node* returnStatement(Parser* parser) {
    Node* value = NULL;
    if (!check(parser, TOKEN_SEMICOLON)) {
        value = expression(parser);
    }
    consume(parser, TOKEN_SEMICOLON, "Expect ';' after return value.");

    Node* node = malloc(sizeof(Node));
    node->next = NULL;
    node->type = NODE_STMT_RETURN;
    node->returnStmt.value = value;
    return node;
}

// If statement
static Node* ifStatement(Parser* parser) {
    consume(parser, TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    Node* condition = expression(parser);
    consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after if condition.");

    Node* thenBranch = statement(parser);
    Node* elseBranch = NULL;
    if (match(parser, TOKEN_ELSE)) {
        elseBranch = statement(parser);
    }

    Node* node = malloc(sizeof(Node));
    node->next = NULL;
    node->type = NODE_STMT_IF;
    node->ifStmt.condition = condition;
    node->ifStmt.thenBranch = thenBranch;
    node->ifStmt.elseBranch = elseBranch;
    return node;
}

// While statement
static Node* whileStatement(Parser* parser) {
    consume(parser, TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    Node* condition = expression(parser);
    consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
    Node* body = statement(parser);

    Node* node = malloc(sizeof(Node));
    node->next = NULL;
    node->type = NODE_STMT_WHILE;
    node->whileStmt.condition = condition;
    node->whileStmt.body = body;
    return node;
}

// For statement
static Node* forStatement(Parser* parser) {
    consume(parser, TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
    
    Node* initializer = NULL;
    if (match(parser, TOKEN_SEMICOLON)) {
        // No initializer
    } else if (match(parser, TOKEN_VAR)) {
        initializer = varDeclaration(parser);
    } else {
        initializer = expression(parser);
        consume(parser, TOKEN_SEMICOLON, "Expect ';' after loop start.");
    }

    Node* condition = NULL;
    if (!check(parser, TOKEN_SEMICOLON)) {
        condition = expression(parser);
    }
    consume(parser, TOKEN_SEMICOLON, "Expect ';' after loop condition.");

    Node* increment = NULL;
    if (!check(parser, TOKEN_RIGHT_PAREN)) {
        increment = expression(parser);
    }
    consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

    Node* body = statement(parser);

    Node* node = malloc(sizeof(Node));
    node->next = NULL;
    node->type = NODE_STMT_FOR;
    node->forStmt.initializer = initializer;
    node->forStmt.condition = condition;
    node->forStmt.increment = increment;
    node->forStmt.body = body;
    return node;
}

// Statement
static Node* statement(Parser* parser) {
    if (match(parser, TOKEN_PRINT)) return printStatement(parser);
    if (match(parser, TOKEN_IF)) return ifStatement(parser);
    if (match(parser, TOKEN_WHILE)) return whileStatement(parser);
    if (match(parser, TOKEN_FOR)) return forStatement(parser);
    if (match(parser, TOKEN_RETURN)) return returnStatement(parser);
    if (match(parser, TOKEN_LOADEXTERN)) {
        Node* node = malloc(sizeof(Node));
        if (!node) { error("Memory allocation failed.", parser->tokens[parser->current].line); return NULL; }
        node->next = NULL;
        node->type = NODE_STMT_LOADEXTERN;
        consume(parser, TOKEN_LEFT_PAREN, "Expect '(' after 'loadextern'.");
        Node* pathExpr = expression(parser);
        consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after loadextern argument.");
        consume(parser, TOKEN_SEMICOLON, "Expect ';' after loadextern statement.");
        node->loadexternStmt.pathExpr = pathExpr;
        return node;
    }
    if (match(parser, TOKEN_LEFT_BRACE)) return block(parser);

    Node* exprStmt = expression(parser);
    consume(parser, TOKEN_SEMICOLON, "Expect ';' after expression.");
    exprStmt->next = NULL; // Initialize next pointer to NULL
    return exprStmt;
}

// Declaration (var or stmt or function)
static Node* declaration(Parser* parser) {
    if (match(parser, TOKEN_VAR)) return varDeclaration(parser);
    if (match(parser, TOKEN_IMPORT)) {
        Token module = consume(parser, TOKEN_IDENTIFIER, "Expect module name after 'import'.");
        consume(parser, TOKEN_AS, "Expect 'as' in import statement.");
        Token alias = consume(parser, TOKEN_IDENTIFIER, "Expect alias after 'as'.");
        consume(parser, TOKEN_SEMICOLON, "Expect ';' after import statement.");

        Node* node = malloc(sizeof(Node));
        node->next = NULL;
        node->type = NODE_STMT_IMPORT;
        node->importStmt.module = module;
        node->importStmt.alias = alias;
        return node;
    }
    // async function or normal function
    if (match(parser, TOKEN_ASYNC)) {
        // require 'function' after async
        consume(parser, TOKEN_FUNCTION, "Expect 'function' after 'async'.");
        Node* fn = function(parser);
        fn->function.isAsync = true;
        return fn;
    }
    if (match(parser, TOKEN_FUNCTION)) {
        Node* fn = function(parser);
        fn->function.isAsync = false;
        return fn;
    }
    return statement(parser);
}

// Main parse function
Node* parse(Parser* parser) {
    // Parse top-level declarations into a block
    Node* root = malloc(sizeof(Node));
    root->next = NULL;
    root->type = NODE_STMT_BLOCK;
    root->block.statements = malloc(8 * sizeof(Node*));
    root->block.count = 0;
    root->block.capacity = 8;

    while (!match(parser, TOKEN_EOF)) {
        Node* decl = declaration(parser);
        if (root->block.count == root->block.capacity) {
            root->block.capacity *= 2;
            root->block.statements = realloc(root->block.statements, root->block.capacity * sizeof(Node*));
        }
        root->block.statements[root->block.count++] = decl;
    }

    return root;
}