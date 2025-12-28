#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

// Unnarize interpreter version
// Update this string when making a release.
#ifndef UNNARIZE_VERSION
#define UNNARIZE_VERSION "0.1.0-beta"
#endif

// Define token types for lexer
typedef enum {
    TOKEN_EOF,
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_IMPORT,
    TOKEN_AS,
    TOKEN_ASYNC,
    TOKEN_AWAIT,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_PERCENT,
    TOKEN_EQUAL,
    TOKEN_EQUAL_EQUAL,
    TOKEN_BANG_EQUAL,
    TOKEN_GREATER,
    TOKEN_GREATER_EQUAL,
    TOKEN_LESS,
    TOKEN_LESS_EQUAL,
    TOKEN_LEFT_PAREN,
    TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_BRACE,
    TOKEN_RIGHT_BRACE,
    TOKEN_LEFT_BRACKET,
    TOKEN_RIGHT_BRACKET,
    TOKEN_SEMICOLON,
    TOKEN_DOT,
    TOKEN_VAR,
    TOKEN_PRINT,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_WHILE,
    TOKEN_FOR,
    TOKEN_FUNCTION,
    TOKEN_RETURN,
    TOKEN_COMMA,
    TOKEN_LOADEXTERN,
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_AND, // &&
    TOKEN_OR,  // ||
    TOKEN_PLUS_EQUAL,  // +=
    TOKEN_MINUS_EQUAL, // -=
    TOKEN_STAR_EQUAL,  // *=
    TOKEN_SLASH_EQUAL, // /=
    TOKEN_COLON,       // :
    TOKEN_STRUCT       // struct
} TokenType;

// Token structure
typedef struct {
    TokenType type;
    const char* start;
    int length;
    int line;
} Token;

// Error reporting
// Error reporting
extern const char* g_source;
void error(const char* message, int line);
void errorAtToken(Token token, const char* message);

#endif // COMMON_H