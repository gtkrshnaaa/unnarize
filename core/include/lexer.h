#ifndef LEXER_H
#define LEXER_H

#include "common.h"

// Lexer structure
typedef struct {
    const char* start;
    const char* current;
    int line;
} Lexer;

// Initialize lexer
void initLexer(Lexer* lexer, const char* source);

// Scan next token
Token scanToken(Lexer* lexer);

#endif // LEXER_H