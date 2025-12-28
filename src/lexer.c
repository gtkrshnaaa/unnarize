#include "lexer.h"

// Helper to check if char is digit
static bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

// Helper to check if alpha
static bool isAlpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

// Make token
static Token makeToken(Lexer* lexer, TokenType type) {
    Token token;
    token.type = type;
    token.start = lexer->start;
    token.length = (int)(lexer->current - lexer->start);
    token.line = lexer->line;
    return token;
}

// Error token
static Token errorToken(Lexer* lexer, const char* message) {
    Token token;
    token.type = TOKEN_EOF; // Use EOF for error
    token.start = message;
    token.length = (int)strlen(message);
    token.line = lexer->line;
    return token;
}

// Skip whitespace
static void skipWhitespace(Lexer* lexer) {
    while (true) {
        char c = *lexer->current;
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                lexer->current++;
                break;
            case '\n':
                lexer->line++;
                lexer->current++;
                break;
            case '/':
                if (*(lexer->current + 1) == '/') {
                    // Comment until end of line
                    while (*lexer->current != '\n' && *lexer->current != '\0') {
                        lexer->current++;
                    }
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

// Check keyword
static TokenType checkKeyword(Lexer* lexer, int start, int length, const char* rest, TokenType type) {
    if (lexer->current - lexer->start == start + length &&
        memcmp(lexer->start + start, rest, length) == 0) {
        return type;
    }
    return TOKEN_IDENTIFIER;
}

// Identifier type (keywords or var names)
static TokenType identifierType(Lexer* lexer) {
    switch (*lexer->start) {
        case 'i': {
            // import, if
            if (lexer->current - lexer->start > 1) {
                if (*(lexer->start + 1) == 'm') {
                    return checkKeyword(lexer, 1, 5, "mport", TOKEN_IMPORT);
                } else if (*(lexer->start + 1) == 'f') {
                    return checkKeyword(lexer, 1, 1, "f", TOKEN_IF);
                }
            }
            break;
        }
        case 'l':
            return checkKeyword(lexer, 1, 9, "oadextern", TOKEN_LOADEXTERN);
        case 'v': return checkKeyword(lexer, 1, 2, "ar", TOKEN_VAR);
        case 'p': return checkKeyword(lexer, 1, 4, "rint", TOKEN_PRINT);
        case 'e': return checkKeyword(lexer, 1, 3, "lse", TOKEN_ELSE);
        case 'w': return checkKeyword(lexer, 1, 4, "hile", TOKEN_WHILE);
        case 'f':
            if (lexer->current - lexer->start > 1) {
                switch (*(lexer->start + 1)) {
                    case 'o': return checkKeyword(lexer, 2, 1, "r", TOKEN_FOR);
                    case 'u': return checkKeyword(lexer, 2, 6, "nction", TOKEN_FUNCTION);
                    case 'a': return checkKeyword(lexer, 2, 3, "lse", TOKEN_FALSE);
                }
            }
            break;
        case 'a':
            if (lexer->current - lexer->start > 1) {
                switch (*(lexer->start + 1)) {
                    case 's':
                        // as, async
                        if (lexer->current - lexer->start == 2) {
                            return checkKeyword(lexer, 1, 1, "s", TOKEN_AS);
                        }
                        return checkKeyword(lexer, 1, 4, "sync", TOKEN_ASYNC);
                    case 'w':
                        return checkKeyword(lexer, 1, 4, "wait", TOKEN_AWAIT);
                }
            } else {
                return checkKeyword(lexer, 1, 1, "s", TOKEN_AS);
            }
            break;
        case 'r': return checkKeyword(lexer, 1, 5, "eturn", TOKEN_RETURN);
        case 's': return checkKeyword(lexer, 1, 5, "truct", TOKEN_STRUCT);
        case 't': return checkKeyword(lexer, 1, 3, "rue", TOKEN_TRUE);
    }
    return TOKEN_IDENTIFIER;
}

// Scan identifier
static Token identifier(Lexer* lexer) {
    while (isAlpha(*lexer->current) || isDigit(*lexer->current)) {
        lexer->current++;
    }
    return makeToken(lexer, identifierType(lexer));
}

// Scan number
static Token number(Lexer* lexer) {
    while (isDigit(*lexer->current)) lexer->current++;
    if (*lexer->current == '.' && isDigit(*(lexer->current + 1))) {
        lexer->current++; // Consume '.'
        while (isDigit(*lexer->current)) lexer->current++;
    }
    return makeToken(lexer, TOKEN_NUMBER);
}

// Scan string
static Token string(Lexer* lexer) {
    char quote = *(lexer->current - 1); // opening quote char (' or ")
    while (*lexer->current != quote && *lexer->current != '\0') {
        if (*lexer->current == '\n') lexer->line++;
        lexer->current++;
    }
    if (*lexer->current == '\0') return errorToken(lexer, "Unterminated string.");
    lexer->current++; // Consume closing "
    return makeToken(lexer, TOKEN_STRING);
}

void initLexer(Lexer* lexer, const char* source) {
    lexer->start = source;
    lexer->current = source;
    lexer->line = 1;
}

Token scanToken(Lexer* lexer) {
    skipWhitespace(lexer);
    lexer->start = lexer->current;

    if (*lexer->current == '\0') return makeToken(lexer, TOKEN_EOF);

    char c = *lexer->current++;
    if (isAlpha(c)) return identifier(lexer);
    if (isDigit(c)) return number(lexer);

    switch (c) {
        case '(': return makeToken(lexer, TOKEN_LEFT_PAREN);
        case ')': return makeToken(lexer, TOKEN_RIGHT_PAREN);
        case '{': return makeToken(lexer, TOKEN_LEFT_BRACE);
        case '}': return makeToken(lexer, TOKEN_RIGHT_BRACE);
        case '[': return makeToken(lexer, TOKEN_LEFT_BRACKET);
        case ']': return makeToken(lexer, TOKEN_RIGHT_BRACKET);
        case ':': return makeToken(lexer, TOKEN_COLON);
        case ';': return makeToken(lexer, TOKEN_SEMICOLON);
        case ',': return makeToken(lexer, TOKEN_COMMA);
        case '.': return makeToken(lexer, TOKEN_DOT);
        case '+': 
            return makeToken(lexer, (*lexer->current == '=') ? (lexer->current++, TOKEN_PLUS_EQUAL) : TOKEN_PLUS);
        case '-': 
            return makeToken(lexer, (*lexer->current == '=') ? (lexer->current++, TOKEN_MINUS_EQUAL) : TOKEN_MINUS);
        case '*': 
            return makeToken(lexer, (*lexer->current == '=') ? (lexer->current++, TOKEN_STAR_EQUAL) : TOKEN_STAR);
        case '/': 
            return makeToken(lexer, (*lexer->current == '=') ? (lexer->current++, TOKEN_SLASH_EQUAL) : TOKEN_SLASH);
        case '%': return makeToken(lexer, TOKEN_PERCENT);
        case '&':
            if (*lexer->current == '&') { lexer->current++; return makeToken(lexer, TOKEN_AND); }
            else return errorToken(lexer, "Unexpected character '&'.");
        case '|':
            if (*lexer->current == '|') { lexer->current++; return makeToken(lexer, TOKEN_OR); }
            else return errorToken(lexer, "Unexpected character '|'.");
        case '!':
            return makeToken(lexer, (*lexer->current == '=') ? (lexer->current++, TOKEN_BANG_EQUAL) : TOKEN_BANG); // ! or !=
        case '=':
            return makeToken(lexer, (*lexer->current == '=') ? (lexer->current++, TOKEN_EQUAL_EQUAL) : TOKEN_EQUAL);
        case '>':
            return makeToken(lexer, (*lexer->current == '=') ? (lexer->current++, TOKEN_GREATER_EQUAL) : TOKEN_GREATER);
        case '<':
            return makeToken(lexer, (*lexer->current == '=') ? (lexer->current++, TOKEN_LESS_EQUAL) : TOKEN_LESS);
        case '"':
        case '\'':
            return string(lexer);
    }

    return errorToken(lexer, "Unexpected character.");
}