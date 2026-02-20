#ifndef PARSER_H
#define PARSER_H

#include "common.h"

// AST Node types (simplified)
typedef enum {
    NODE_EXPR_LITERAL,
    NODE_EXPR_BINARY,
    NODE_EXPR_UNARY,
    NODE_EXPR_VAR,
    NODE_EXPR_AWAIT,
    NODE_EXPR_CALL,        // function calls
    NODE_EXPR_GET,         // object.property
    NODE_EXPR_INDEX,       // target[index]
    NODE_STMT_VAR_DECL,
    NODE_STMT_ASSIGN,
    NODE_STMT_INDEX_ASSIGN,
    NODE_STMT_PRINT,
    NODE_STMT_IF,
    NODE_STMT_WHILE,
    NODE_STMT_FOR,
    NODE_STMT_BLOCK,
    NODE_STMT_FUNCTION,
    NODE_STMT_RETURN,
    NODE_STMT_IMPORT,

    NODE_EXPR_ARRAY_LITERAL,
    NODE_STMT_FOREACH,
    NODE_STMT_STRUCT_DECL,
    NODE_STMT_PROP_ASSIGN
} NodeType;

// Forward declaration for AST Node
typedef struct Node Node;

// AST Node structure
struct Node {
    NodeType type;
    union {
        // Literals
        struct {
            Token token;  // For numbers/strings
        } literal;
        // Binary expr
        struct {
            Node* left;
            Token op;
            Node* right;
        } binary;
        // Unary
        struct {
            Token op;
            Node* expr;
        } unary;
        // Var reference
        struct {
            Token name;
            int slot; // Stack slot index, -1 if global
        } var;
        // Function call: callee(arguments)
        struct {
            Node* callee;
            Node* arguments; // linked list via next
            int argumentCount;
        } call;
        // Member access: object.name
        struct {
            Node* object;
            Token name;
        } get;
        // Indexing: target[index]
        struct {
            Node* target;
            Node* index;
        } index;
        // Var decl
        struct {
            Token name;
            Node* initializer;
            int slot; // Stack slot index, -1 if global
        } varDecl;
        // Assign
        struct {
            Token name;
            Token operator; // = or += or -= etc.
            Node* value;
            int slot; // Stack slot index, -1 if global
        } assign;
        // Index assignment: target[index] = value
        struct {
            Node* target;
            Node* index;
            Token operator; // = or += etc.
            Node* value;
        } indexAssign;
        // Print
        struct {
            Node* expr;
        } print;
        // If
        struct {
            Node* condition;
            Node* thenBranch;
            Node* elseBranch;
        } ifStmt;
        // While
        struct {
            Node* condition;
            Node* body;
        } whileStmt;
        // For (init; cond; incr; body)
        struct {
            Node* initializer;
            Node* condition;
            Node* increment;
            Node* body;
        } forStmt;
        // Block { statements }
        struct {
            Node** statements;
            int count;
            int capacity;
        } block;
        // Function
        struct {
            Token name;
            Token* params;
            int paramCount;
            Node* body;
            bool isAsync;
        } function;
        // Return
        struct {
            Node* value;
        } returnStmt;
        // Import statement: import module as alias;
        struct {
            Token module;
            Token alias;
        } importStmt;
        // importStmt...

        // Array literal [e1, e2, ...]
        struct {
            Node* elements; // Linked list
            int count;
        } arrayLiteral;
        // Foreach (var item : collection)
        struct {
            Token iterator;
            Node* collection;
            Node* body;
            int slot; // Stack slot index for iterator
        } foreachStmt;
        // Struct Declaration
        struct {
            Token name;
            Token* fields;
            int fieldCount;
        } structDecl;
        // Property Assignment (obj.prop = val)
        struct {
            Node* object;
            Token name;
            Token operator;
            Node* value;
        } propAssign;
    };
    Node* next; // For linked list (function arguments)
};

// Parser structure with dynamic token array
typedef struct {
    Token* tokens;      // Dynamic array of tokens
    int current;
    int count;
    int capacity;       // Current capacity of the array
} Parser;

// Initialize parser
void initParser(Parser* parser);

// Free parser resources
void freeParser(Parser* parser);

// Add token to parser (grows array as needed)
void addToken(Parser* parser, Token token);

// Parse tokens into AST
Node* parse(Parser* parser);

#endif // PARSER_H