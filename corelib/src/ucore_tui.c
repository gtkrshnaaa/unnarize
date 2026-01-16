#include "ucore_tui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <ctype.h>

// ============================================================================
// ANSI Escape Code Constants
// ============================================================================

#define ESC "\033"
#define CSI ESC "["

// Colors (Foreground)
#define FG_BLACK   "30"
#define FG_RED     "31"
#define FG_GREEN   "32"
#define FG_YELLOW  "33"
#define FG_BLUE    "34"
#define FG_MAGENTA "35"
#define FG_CYAN    "36"
#define FG_WHITE   "37"
#define FG_BRIGHT_BLACK   "90"
#define FG_BRIGHT_RED     "91"
#define FG_BRIGHT_GREEN   "92"
#define FG_BRIGHT_YELLOW  "93"
#define FG_BRIGHT_BLUE    "94"
#define FG_BRIGHT_MAGENTA "95"
#define FG_BRIGHT_CYAN    "96"
#define FG_BRIGHT_WHITE   "97"

// Colors (Background)
#define BG_BLACK   "40"
#define BG_RED     "41"
#define BG_GREEN   "42"
#define BG_YELLOW  "43"
#define BG_BLUE    "44"
#define BG_MAGENTA "45"
#define BG_CYAN    "46"
#define BG_WHITE   "47"

// Box-drawing characters (Unicode)
#define BOX_LIGHT_H  "─"
#define BOX_LIGHT_V  "│"
#define BOX_LIGHT_TL "┌"
#define BOX_LIGHT_TR "┐"
#define BOX_LIGHT_BL "└"
#define BOX_LIGHT_BR "┘"
#define BOX_LIGHT_LT "├"
#define BOX_LIGHT_RT "┤"
#define BOX_LIGHT_TT "┬"
#define BOX_LIGHT_BT "┴"
#define BOX_LIGHT_X  "┼"

#define BOX_ROUND_TL "╭"
#define BOX_ROUND_TR "╮"
#define BOX_ROUND_BL "╰"
#define BOX_ROUND_BR "╯"

#define BOX_DOUBLE_H  "═"
#define BOX_DOUBLE_V  "║"
#define BOX_DOUBLE_TL "╔"
#define BOX_DOUBLE_TR "╗"
#define BOX_DOUBLE_BL "╚"
#define BOX_DOUBLE_BR "╝"

// Tree characters
#define TREE_BRANCH "├── "
#define TREE_LAST   "└── "
#define TREE_PIPE   "│   "
#define TREE_SPACE  "    "

// ============================================================================
// Terminal Mode Handling
// ============================================================================

static struct termios orig_termios;
static int raw_mode_active = 0;

static void disableRawMode(void) {
    if (raw_mode_active) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        raw_mode_active = 0;
    }
}

static void enableRawMode(void) {
    if (raw_mode_active) return;
    
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disableRawMode);
    
    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    raw_mode_active = 1;
}

// Helper to read single character with explicit return value handling
static int readChar(char* c) {
    ssize_t n = read(STDIN_FILENO, c, 1);
    return (n == 1) ? 1 : 0;
}

// ============================================================================
// Helper: Color Name to ANSI Code
// ============================================================================

static const char* colorNameToFg(const char* name) {
    if (strcmp(name, "black") == 0) return FG_BLACK;
    if (strcmp(name, "red") == 0) return FG_RED;
    if (strcmp(name, "green") == 0) return FG_GREEN;
    if (strcmp(name, "yellow") == 0) return FG_YELLOW;
    if (strcmp(name, "blue") == 0) return FG_BLUE;
    if (strcmp(name, "magenta") == 0) return FG_MAGENTA;
    if (strcmp(name, "cyan") == 0) return FG_CYAN;
    if (strcmp(name, "white") == 0) return FG_WHITE;
    if (strcmp(name, "brightBlack") == 0 || strcmp(name, "gray") == 0) return FG_BRIGHT_BLACK;
    if (strcmp(name, "brightRed") == 0) return FG_BRIGHT_RED;
    if (strcmp(name, "brightGreen") == 0) return FG_BRIGHT_GREEN;
    if (strcmp(name, "brightYellow") == 0) return FG_BRIGHT_YELLOW;
    if (strcmp(name, "brightBlue") == 0) return FG_BRIGHT_BLUE;
    if (strcmp(name, "brightMagenta") == 0) return FG_BRIGHT_MAGENTA;
    if (strcmp(name, "brightCyan") == 0) return FG_BRIGHT_CYAN;
    if (strcmp(name, "brightWhite") == 0) return FG_BRIGHT_WHITE;
    return FG_WHITE;
}

static const char* colorNameToBg(const char* name) {
    if (strcmp(name, "black") == 0) return BG_BLACK;
    if (strcmp(name, "red") == 0) return BG_RED;
    if (strcmp(name, "green") == 0) return BG_GREEN;
    if (strcmp(name, "yellow") == 0) return BG_YELLOW;
    if (strcmp(name, "blue") == 0) return BG_BLUE;
    if (strcmp(name, "magenta") == 0) return BG_MAGENTA;
    if (strcmp(name, "cyan") == 0) return BG_CYAN;
    if (strcmp(name, "white") == 0) return BG_WHITE;
    return BG_BLACK;
}

// ============================================================================
// Terminal Primitives
// ============================================================================

// clear() - Clear entire screen
static Value tui_clear(VM* vm, Value* args, int argCount) {
    (void)vm; (void)args; (void)argCount;
    printf(CSI "2J" CSI "H");
    fflush(stdout);
    return NIL_VAL;
}

// clearLine() - Clear current line
static Value tui_clearLine(VM* vm, Value* args, int argCount) {
    (void)vm; (void)args; (void)argCount;
    printf(CSI "2K\r");
    fflush(stdout);
    return NIL_VAL;
}

// moveTo(row, col) - Move cursor to position (1-indexed)
static Value tui_moveTo(VM* vm, Value* args, int argCount) {
    (void)vm;
    if (argCount < 2) return NIL_VAL;
    
    int row = IS_INT(args[0]) ? AS_INT(args[0]) : (int)AS_FLOAT(args[0]);
    int col = IS_INT(args[1]) ? AS_INT(args[1]) : (int)AS_FLOAT(args[1]);
    
    printf(CSI "%d;%dH", row, col);
    fflush(stdout);
    return NIL_VAL;
}

// hideCursor()
static Value tui_hideCursor(VM* vm, Value* args, int argCount) {
    (void)vm; (void)args; (void)argCount;
    printf(CSI "?25l");
    fflush(stdout);
    return NIL_VAL;
}

// showCursor()
static Value tui_showCursor(VM* vm, Value* args, int argCount) {
    (void)vm; (void)args; (void)argCount;
    printf(CSI "?25h");
    fflush(stdout);
    return NIL_VAL;
}

// size() - Returns struct {rows, cols}
static Value tui_size(VM* vm, Value* args, int argCount) {
    (void)args; (void)argCount;
    
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    
    // Create a map with rows and cols
    Map* m = newMap(vm);
    
    // Add rows and cols using available API
    mapSetStr(m, "rows", 4, INT_VAL(w.ws_row));
    mapSetStr(m, "cols", 4, INT_VAL(w.ws_col));
    
    return OBJ_VAL(m);
}

// ============================================================================
// Colors & Styling
// ============================================================================

// fg(color, text) - Apply foreground color
static Value tui_fg(VM* vm, Value* args, int argCount) {
    if (argCount < 2 || !IS_STRING(args[0]) || !IS_STRING(args[1])) {
        return argCount >= 2 ? args[1] : NIL_VAL;
    }
    
    const char* colorName = AS_CSTRING(args[0]);
    const char* text = AS_CSTRING(args[1]);
    const char* code = colorNameToFg(colorName);
    
    char buffer[4096];
    snprintf(buffer, sizeof(buffer), CSI "%sm%s" CSI "0m", code, text);
    
    ObjString* result = internString(vm, buffer, (int)strlen(buffer));
    return OBJ_VAL(result);
}

// bg(color, text) - Apply background color
static Value tui_bg(VM* vm, Value* args, int argCount) {
    if (argCount < 2 || !IS_STRING(args[0]) || !IS_STRING(args[1])) {
        return argCount >= 2 ? args[1] : NIL_VAL;
    }
    
    const char* colorName = AS_CSTRING(args[0]);
    const char* text = AS_CSTRING(args[1]);
    const char* code = colorNameToBg(colorName);
    
    char buffer[4096];
    snprintf(buffer, sizeof(buffer), CSI "%sm%s" CSI "0m", code, text);
    
    ObjString* result = internString(vm, buffer, (int)strlen(buffer));
    return OBJ_VAL(result);
}

// bold(text)
static Value tui_bold(VM* vm, Value* args, int argCount) {
    if (argCount < 1 || !IS_STRING(args[0])) return NIL_VAL;
    
    const char* text = AS_CSTRING(args[0]);
    char buffer[4096];
    snprintf(buffer, sizeof(buffer), CSI "1m%s" CSI "0m", text);
    
    ObjString* result = internString(vm, buffer, (int)strlen(buffer));
    return OBJ_VAL(result);
}

// dim(text)
static Value tui_dim(VM* vm, Value* args, int argCount) {
    if (argCount < 1 || !IS_STRING(args[0])) return NIL_VAL;
    
    const char* text = AS_CSTRING(args[0]);
    char buffer[4096];
    snprintf(buffer, sizeof(buffer), CSI "2m%s" CSI "0m", text);
    
    ObjString* result = internString(vm, buffer, (int)strlen(buffer));
    return OBJ_VAL(result);
}

// italic(text)
static Value tui_italic(VM* vm, Value* args, int argCount) {
    if (argCount < 1 || !IS_STRING(args[0])) return NIL_VAL;
    
    const char* text = AS_CSTRING(args[0]);
    char buffer[4096];
    snprintf(buffer, sizeof(buffer), CSI "3m%s" CSI "0m", text);
    
    ObjString* result = internString(vm, buffer, (int)strlen(buffer));
    return OBJ_VAL(result);
}

// underline(text)
static Value tui_underline(VM* vm, Value* args, int argCount) {
    if (argCount < 1 || !IS_STRING(args[0])) return NIL_VAL;
    
    const char* text = AS_CSTRING(args[0]);
    char buffer[4096];
    snprintf(buffer, sizeof(buffer), CSI "4m%s" CSI "0m", text);
    
    ObjString* result = internString(vm, buffer, (int)strlen(buffer));
    return OBJ_VAL(result);
}

// style(text, styles) - Apply multiple styles
static Value tui_style(VM* vm, Value* args, int argCount) {
    if (argCount < 2 || !IS_STRING(args[0]) || !IS_STRING(args[1])) {
        return argCount >= 1 ? args[0] : NIL_VAL;
    }
    
    const char* text = AS_CSTRING(args[0]);
    const char* styles = AS_CSTRING(args[1]);
    
    char codes[256] = "";
    char* stylesCopy = strdup(styles);
    char* token = strtok(stylesCopy, " ");
    
    while (token) {
        if (strcmp(token, "bold") == 0) strcat(codes, "1;");
        else if (strcmp(token, "dim") == 0) strcat(codes, "2;");
        else if (strcmp(token, "italic") == 0) strcat(codes, "3;");
        else if (strcmp(token, "underline") == 0) strcat(codes, "4;");
        else if (strncmp(token, "bg:", 3) == 0) {
            strcat(codes, colorNameToBg(token + 3));
            strcat(codes, ";");
        }
        else {
            strcat(codes, colorNameToFg(token));
            strcat(codes, ";");
        }
        token = strtok(NULL, " ");
    }
    free(stylesCopy);
    
    // Remove trailing semicolon
    size_t len = strlen(codes);
    if (len > 0 && codes[len - 1] == ';') codes[len - 1] = '\0';
    
    char buffer[4096];
    snprintf(buffer, sizeof(buffer), CSI "%sm%s" CSI "0m", codes, text);
    
    ObjString* result = internString(vm, buffer, (int)strlen(buffer));
    return OBJ_VAL(result);
}

// ============================================================================
// Input Handling
// ============================================================================

// keypress() - Read single keypress, returns key name
static Value tui_keypress(VM* vm, Value* args, int argCount) {
    (void)args; (void)argCount;
    
    enableRawMode();
    
    char c;
    while (!readChar(&c)) {
        // Wait for input
    }
    
    char keyName[32] = "";
    
    if (c == '\033') {
        // Escape sequence
        char seq[3] = {0};
        if (readChar(&seq[0]) && readChar(&seq[1])) {
            if (seq[0] == '[') {
                switch (seq[1]) {
                    case 'A': strcpy(keyName, "up"); break;
                    case 'B': strcpy(keyName, "down"); break;
                    case 'C': strcpy(keyName, "right"); break;
                    case 'D': strcpy(keyName, "left"); break;
                    case 'H': strcpy(keyName, "home"); break;
                    case 'F': strcpy(keyName, "end"); break;
                    case '3':
                        (void)readChar(&seq[2]);
                        strcpy(keyName, "delete");
                        break;
                    default: strcpy(keyName, "escape"); break;
                }
            }
        } else {
            strcpy(keyName, "escape");
        }
    } else if (c == '\r' || c == '\n') {
        strcpy(keyName, "enter");
    } else if (c == 127 || c == 8) {
        strcpy(keyName, "backspace");
    } else if (c == '\t') {
        strcpy(keyName, "tab");
    } else if (c >= 1 && c <= 26) {
        snprintf(keyName, sizeof(keyName), "ctrl+%c", 'a' + c - 1);
    } else if (isprint((unsigned char)c)) {
        keyName[0] = c;
        keyName[1] = '\0';
    } else {
        snprintf(keyName, sizeof(keyName), "unknown:%d", (int)(unsigned char)c);
    }
    
    disableRawMode();
    
    ObjString* result = internString(vm, keyName, (int)strlen(keyName));
    return OBJ_VAL(result);
}

// input(prompt) - Line input with arrow key navigation
static Value tui_input(VM* vm, Value* args, int argCount) {
    const char* prompt = "";
    if (argCount >= 1 && IS_STRING(args[0])) {
        prompt = AS_CSTRING(args[0]);
    }
    
    printf("%s", prompt);
    fflush(stdout);
    
    enableRawMode();
    
    char buffer[4096] = "";
    int len = 0;
    int cursor = 0;
    
    while (1) {
        char c;
        if (!readChar(&c)) continue;
        
        if (c == '\r' || c == '\n') {
            printf("\r\n");
            break;
        } else if (c == 127 || c == 8) { // Backspace
            if (cursor > 0) {
                memmove(&buffer[cursor - 1], &buffer[cursor], (size_t)(len - cursor + 1));
                cursor--;
                len--;
                
                // Redraw line
                printf("\r%s%s \r", prompt, buffer);
                printf(CSI "%dG", (int)(strlen(prompt) + (size_t)cursor + 1));
            }
        } else if (c == '\033') { // Escape sequence
            char seq[3] = {0};
            if (readChar(&seq[0]) && readChar(&seq[1])) {
                if (seq[0] == '[') {
                    if (seq[1] == 'C' && cursor < len) { // Right
                        cursor++;
                        printf(CSI "C");
                    } else if (seq[1] == 'D' && cursor > 0) { // Left
                        cursor--;
                        printf(CSI "D");
                    } else if (seq[1] == 'H') { // Home
                        cursor = 0;
                        printf("\r" CSI "%dG", (int)(strlen(prompt) + 1));
                    } else if (seq[1] == 'F') { // End
                        cursor = len;
                        printf("\r" CSI "%dG", (int)(strlen(prompt) + (size_t)len + 1));
                    } else if (seq[1] == '3') { // Delete
                        (void)readChar(&seq[2]);
                        if (cursor < len) {
                            memmove(&buffer[cursor], &buffer[cursor + 1], (size_t)(len - cursor));
                            len--;
                            printf("\r%s%s \r", prompt, buffer);
                            printf(CSI "%dG", (int)(strlen(prompt) + (size_t)cursor + 1));
                        }
                    }
                }
            }
        } else if (c >= 32 && c < 127) { // Printable
            if (len < (int)sizeof(buffer) - 1) {
                memmove(&buffer[cursor + 1], &buffer[cursor], (size_t)(len - cursor + 1));
                buffer[cursor] = c;
                cursor++;
                len++;
                
                printf("\r%s%s\r", prompt, buffer);
                printf(CSI "%dG", (int)(strlen(prompt) + (size_t)cursor + 1));
            }
        } else if (c == 3) { // Ctrl+C
            buffer[0] = '\0';
            len = 0;
            printf("\r\n");
            break;
        }
        fflush(stdout);
    }
    
    disableRawMode();
    
    ObjString* result = internString(vm, buffer, len);
    return OBJ_VAL(result);
}

// inputPassword(prompt) - Hidden input
static Value tui_inputPassword(VM* vm, Value* args, int argCount) {
    const char* prompt = "";
    if (argCount >= 1 && IS_STRING(args[0])) {
        prompt = AS_CSTRING(args[0]);
    }
    
    printf("%s", prompt);
    fflush(stdout);
    
    enableRawMode();
    
    char buffer[256] = "";
    int len = 0;
    
    while (1) {
        char c;
        if (!readChar(&c)) continue;
        
        if (c == '\r' || c == '\n') {
            printf("\r\n");
            break;
        } else if (c == 127 || c == 8) { // Backspace
            if (len > 0) {
                len--;
                printf("\b \b");
            }
        } else if (c >= 32 && c < 127 && len < (int)sizeof(buffer) - 1) {
            buffer[len++] = c;
            printf("*");
        } else if (c == 3) { // Ctrl+C
            buffer[0] = '\0';
            len = 0;
            printf("\r\n");
            break;
        }
        fflush(stdout);
    }
    
    disableRawMode();
    buffer[len] = '\0';
    
    ObjString* result = internString(vm, buffer, len);
    return OBJ_VAL(result);
}

// confirm(prompt) - Yes/No prompt
static Value tui_confirm(VM* vm, Value* args, int argCount) {
    (void)vm;
    const char* prompt = "Confirm?";
    if (argCount >= 1 && IS_STRING(args[0])) {
        prompt = AS_CSTRING(args[0]);
    }
    
    printf("%s [y/n] ", prompt);
    fflush(stdout);
    
    enableRawMode();
    
    bool result = false;
    while (1) {
        char c;
        if (!readChar(&c)) continue;
        
        if (c == 'y' || c == 'Y') {
            result = true;
            printf("y\r\n");
            break;
        } else if (c == 'n' || c == 'N' || c == '\r' || c == '\n') {
            result = false;
            printf("n\r\n");
            break;
        }
    }
    
    disableRawMode();
    return BOOL_VAL(result);
}

// select(prompt, options) - Arrow-key selection menu
static Value tui_select(VM* vm, Value* args, int argCount) {
    (void)vm;
    if (argCount < 2 || !IS_STRING(args[0]) || !IS_ARRAY(args[1])) {
        return INT_VAL(-1);
    }
    
    const char* prompt = AS_CSTRING(args[0]);
    Array* options = (Array*)AS_OBJ(args[1]);
    
    if (options->count == 0) return INT_VAL(-1);
    
    int selected = 0;
    
    printf("%s\r\n", prompt);
    
    enableRawMode();
    printf(CSI "?25l"); // Hide cursor
    
    // Print initial empty lines for options
    for (int i = 0; i < options->count; i++) {
        printf("\r\n");
    }
    // Save position at bottom
    printf(CSI "s");
    fflush(stdout);
    
    while (1) {
        // Move up to start of options
        printf(CSI "%dA", options->count);
        
        // Draw options with proper clearing
        for (int i = 0; i < options->count; i++) {
            printf("\r" CSI "2K"); // Clear entire line
            
            if (i == selected) {
                printf(CSI "7m"); // Inverse
                printf("  > ");
            } else {
                printf("    ");
            }
            
            if (IS_STRING(options->items[i])) {
                printf("%s", AS_CSTRING(options->items[i]));
            }
            
            if (i == selected) {
                printf(CSI "0m");
            }
            printf("\r\n");
        }
        fflush(stdout);
        
        char c;
        if (!readChar(&c)) continue;
        
        if (c == '\033') {
            char seq[2] = {0};
            if (readChar(&seq[0]) && readChar(&seq[1])) {
                if (seq[0] == '[') {
                    if (seq[1] == 'A' && selected > 0) selected--;
                    if (seq[1] == 'B' && selected < options->count - 1) selected++;
                }
            } else {
                // Pure escape
                selected = -1;
                break;
            }
        } else if (c == '\r' || c == '\n') {
            break;
        } else if (c == 3) { // Ctrl+C
            selected = -1;
            break;
        }
    }
    
    printf(CSI "?25h"); // Show cursor
    disableRawMode();
    
    return INT_VAL(selected);
}

// ============================================================================
// Comprehensive Input Features
// ============================================================================

// inputBox(title, width, initialValue) - Input inside a styled box
// Returns: string (user input)
static Value tui_inputBox(VM* vm, Value* args, int argCount) {
    const char* title = "Input";
    int width = 40;
    const char* initial = "";
    
    if (argCount >= 1 && IS_STRING(args[0])) {
        title = AS_CSTRING(args[0]);
    }
    if (argCount >= 2 && IS_INT(args[1])) {
        width = AS_INT(args[1]);
        if (width < 10) width = 10;
        if (width > 120) width = 120;
    }
    if (argCount >= 3 && IS_STRING(args[2])) {
        initial = AS_CSTRING(args[2]);
    }
    
    int titleLen = (int)strlen(title);
    if (width < titleLen + 6) width = titleLen + 6;
    
    char buffer[4096];
    int len = (int)strlen(initial);
    if (len >= (int)sizeof(buffer)) len = (int)sizeof(buffer) - 1;
    strncpy(buffer, initial, sizeof(buffer) - 1);
    buffer[len] = '\0';
    
    int cursor = len;
    int scrollOffset = 0;
    int inputWidth = width - 4;
    
    // Draw box once (static)
    // Top border: ╭─ Title ─...─╮
    // Chars: TL(1) + dash(1) + space(1) + title + space(1) + dashes(remaining) + TR(1) = width
    printf(BOX_ROUND_TL BOX_LIGHT_H " " CSI "1m%s" CSI "0m " , title);
    int remaining = width - titleLen - 5;  // 5 = TL + dash + space + space + TR
    for (int i = 0; i < remaining; i++) printf(BOX_LIGHT_H);
    printf(BOX_ROUND_TR "\r\n");
    
    printf(BOX_LIGHT_V " ");
    int initialShow = len > inputWidth ? inputWidth : len;
    for (int i = 0; i < initialShow; i++) printf("%c", buffer[i]);
    for (int i = initialShow; i < inputWidth; i++) printf(" ");
    printf(" " BOX_LIGHT_V "\r\n");
    
    printf(BOX_ROUND_BL);
    for (int i = 0; i < width - 2; i++) printf(BOX_LIGHT_H);
    printf(BOX_ROUND_BR "\r\n");
    
    // Move back to input line
    printf(CSI "2A\r" CSI "2C");
    fflush(stdout);
    
    enableRawMode();
    
    while (1) {
        // Calculate scroll
        if (cursor - scrollOffset >= inputWidth) {
            scrollOffset = cursor - inputWidth + 1;
        }
        if (cursor < scrollOffset) {
            scrollOffset = cursor;
        }
        
        // Redraw input content only (stay on same line)
        printf("\r" CSI "2C"); // Go to column 3 (after "│ ")
        int visibleLen = len - scrollOffset;
        if (visibleLen > inputWidth) visibleLen = inputWidth;
        for (int i = 0; i < visibleLen; i++) {
            printf("%c", buffer[scrollOffset + i]);
        }
        for (int i = visibleLen; i < inputWidth; i++) printf(" ");
        printf("\r" CSI "%dC", 2 + (cursor - scrollOffset)); // Position cursor
        fflush(stdout);
        
        char c;
        if (!readChar(&c)) continue;
        
        if (c == '\r' || c == '\n') {
            printf("\r\n" CSI "1B"); // Move past bottom border
            break;
        } else if (c == 127 || c == 8) {
            if (cursor > 0) {
                memmove(&buffer[cursor - 1], &buffer[cursor], (size_t)(len - cursor + 1));
                cursor--;
                len--;
            }
        } else if (c == '\033') {
            char seq[3] = {0};
            if (readChar(&seq[0]) && readChar(&seq[1])) {
                if (seq[0] == '[') {
                    if (seq[1] == 'C' && cursor < len) cursor++;
                    else if (seq[1] == 'D' && cursor > 0) cursor--;
                    else if (seq[1] == 'H') cursor = 0;
                    else if (seq[1] == 'F') cursor = len;
                    else if (seq[1] == '3') {
                        (void)readChar(&seq[2]);
                        if (cursor < len) {
                            memmove(&buffer[cursor], &buffer[cursor + 1], (size_t)(len - cursor));
                            len--;
                        }
                    }
                }
            } else {
                // Pure escape - cancel
                buffer[0] = '\0'; len = 0;
                printf("\r\n" CSI "1B");
                break;
            }
        } else if (c == 1) { cursor = 0; }
        else if (c == 5) { cursor = len; }
        else if (c == 11) { buffer[cursor] = '\0'; len = cursor; }
        else if (c == 21) {
            memmove(buffer, &buffer[cursor], (size_t)(len - cursor + 1));
            len = len - cursor; cursor = 0;
        } else if (c == 23) {
            while (cursor > 0 && buffer[cursor - 1] == ' ') {
                memmove(&buffer[cursor - 1], &buffer[cursor], (size_t)(len - cursor + 1));
                cursor--; len--;
            }
            while (cursor > 0 && buffer[cursor - 1] != ' ') {
                memmove(&buffer[cursor - 1], &buffer[cursor], (size_t)(len - cursor + 1));
                cursor--; len--;
            }
        } else if (c == 3) {
            buffer[0] = '\0'; len = 0;
            printf("\r\n" CSI "1B");
            break;
        } else if (c >= 32 && c < 127) {
            if (len < (int)sizeof(buffer) - 1) {
                memmove(&buffer[cursor + 1], &buffer[cursor], (size_t)(len - cursor + 1));
                buffer[cursor] = c;
                cursor++;
                len++;
            }
        }
    }
    
    disableRawMode();
    
    ObjString* result = internString(vm, buffer, len);
    return OBJ_VAL(result);
}

// inputPasswordBox(title, width) - Password input inside a styled box
// Returns: string (user input, hidden with asterisks)
static Value tui_inputPasswordBox(VM* vm, Value* args, int argCount) {
    const char* title = "Password";
    int width = 40;
    
    if (argCount >= 1 && IS_STRING(args[0])) {
        title = AS_CSTRING(args[0]);
    }
    if (argCount >= 2 && IS_INT(args[1])) {
        width = AS_INT(args[1]);
        if (width < 10) width = 10;
        if (width > 120) width = 120;
    }
    
    int titleLen = (int)strlen(title);
    if (width < titleLen + 6) width = titleLen + 6;
    
    char buffer[4096];
    int len = 0;
    buffer[0] = '\0';
    
    int inputWidth = width - 4;
    
    // Draw box once (static)
    printf(BOX_ROUND_TL BOX_LIGHT_H " " CSI "1m%s" CSI "0m " , title);
    int remaining = width - titleLen - 5;
    for (int i = 0; i < remaining; i++) printf(BOX_LIGHT_H);
    printf(BOX_ROUND_TR "\r\n");
    
    printf(BOX_LIGHT_V " ");
    for (int i = 0; i < inputWidth; i++) printf(" ");
    printf(" " BOX_LIGHT_V "\r\n");
    
    printf(BOX_ROUND_BL);
    for (int i = 0; i < width - 2; i++) printf(BOX_LIGHT_H);
    printf(BOX_ROUND_BR "\r\n");
    
    // Move back to input line
    printf(CSI "2A\r" CSI "2C");
    fflush(stdout);
    
    enableRawMode();
    
    while (1) {
        // Redraw asterisks (stay on same line)
        printf("\r" CSI "2C");
        int showLen = len > inputWidth ? inputWidth : len;
        for (int i = 0; i < showLen; i++) printf("*");
        for (int i = showLen; i < inputWidth; i++) printf(" ");
        printf("\r" CSI "%dC", 2 + (len > inputWidth ? inputWidth : len));
        fflush(stdout);
        
        char c;
        if (!readChar(&c)) continue;
        
        if (c == '\r' || c == '\n') {
            printf("\r\n" CSI "1B");
            break;
        } else if (c == 127 || c == 8) { // Backspace
            if (len > 0) {
                len--;
                buffer[len] = '\0';
            }
        } else if (c == '\033') { // Escape - cancel
            char seq[2];
            if (!readChar(&seq[0]) || !readChar(&seq[1])) {
                buffer[0] = '\0'; len = 0;
                printf("\r\n" CSI "1B");
                break;
            }
            // Ignore arrow keys for password
        } else if (c == 3) { // Ctrl+C
            buffer[0] = '\0'; len = 0;
            printf("\r\n" CSI "1B");
            break;
        } else if (c == 21) { // Ctrl+U - clear
            len = 0;
            buffer[0] = '\0';
        } else if (c >= 32 && c < 127) { // Printable
            if (len < (int)sizeof(buffer) - 1) {
                buffer[len] = c;
                len++;
                buffer[len] = '\0';
            }
        }
    }
    
    disableRawMode();
    
    ObjString* result = internString(vm, buffer, len);
    return OBJ_VAL(result);
}


// Tables
// ============================================================================

// Helper: calculate display width (handles ANSI codes)
static int displayWidth(const char* str) {
    int width = 0;
    int inEscape = 0;
    
    for (const char* p = str; *p; p++) {
        if (*p == '\033') {
            inEscape = 1;
        } else if (inEscape) {
            if (*p == 'm') inEscape = 0;
        } else {
            // Handle UTF-8 (simplified: skip continuation bytes)
            if ((*p & 0xC0) != 0x80) width++;
        }
    }
    return width;
}

// table(data) - Render table from 2D array
static Value tui_table(VM* vm, Value* args, int argCount) {
    if (argCount < 1 || !IS_ARRAY(args[0])) {
        return NIL_VAL;
    }
    
    Array* rows = (Array*)AS_OBJ(args[0]);
    if (rows->count == 0) return NIL_VAL;
    
    // Determine column count and widths
    int colCount = 0;
    for (int i = 0; i < rows->count; i++) {
        if (IS_ARRAY(rows->items[i])) {
            Array* row = (Array*)AS_OBJ(rows->items[i]);
            if (row->count > colCount) colCount = row->count;
        }
    }
    
    if (colCount == 0) return NIL_VAL;
    
    int* colWidths = calloc((size_t)colCount, sizeof(int));
    if (!colWidths) return NIL_VAL;
    
    // Calculate max width per column
    for (int i = 0; i < rows->count; i++) {
        if (!IS_ARRAY(rows->items[i])) continue;
        Array* row = (Array*)AS_OBJ(rows->items[i]);
        
        for (int j = 0; j < row->count && j < colCount; j++) {
            char cellBuf[256] = "";
            if (IS_STRING(row->items[j])) {
                strncpy(cellBuf, AS_CSTRING(row->items[j]), sizeof(cellBuf) - 1);
            } else if (IS_INT(row->items[j])) {
                snprintf(cellBuf, sizeof(cellBuf), "%d", AS_INT(row->items[j]));
            } else if (IS_FLOAT(row->items[j])) {
                snprintf(cellBuf, sizeof(cellBuf), "%.2f", AS_FLOAT(row->items[j]));
            }
            
            int w = displayWidth(cellBuf);
            if (w > colWidths[j]) colWidths[j] = w;
        }
    }
    
    // Build table string
    char* output = malloc(32768);
    if (!output) {
        free(colWidths);
        return NIL_VAL;
    }
    output[0] = '\0';
    
    // Top border
    strcat(output, BOX_ROUND_TL);
    for (int j = 0; j < colCount; j++) {
        for (int k = 0; k < colWidths[j] + 2; k++) strcat(output, BOX_LIGHT_H);
        if (j < colCount - 1) strcat(output, BOX_LIGHT_TT);
    }
    strcat(output, BOX_ROUND_TR "\n");
    
    // Rows
    for (int i = 0; i < rows->count; i++) {
        if (!IS_ARRAY(rows->items[i])) continue;
        Array* row = (Array*)AS_OBJ(rows->items[i]);
        
        strcat(output, BOX_LIGHT_V);
        for (int j = 0; j < colCount; j++) {
            char cellBuf[256] = "";
            if (j < row->count) {
                if (IS_STRING(row->items[j])) {
                    strncpy(cellBuf, AS_CSTRING(row->items[j]), sizeof(cellBuf) - 1);
                } else if (IS_INT(row->items[j])) {
                    snprintf(cellBuf, sizeof(cellBuf), "%d", AS_INT(row->items[j]));
                } else if (IS_FLOAT(row->items[j])) {
                    snprintf(cellBuf, sizeof(cellBuf), "%.2f", AS_FLOAT(row->items[j]));
                }
            }
            
            int w = displayWidth(cellBuf);
            int padding = colWidths[j] - w;
            
            strcat(output, " ");
            strcat(output, cellBuf);
            for (int k = 0; k < padding + 1; k++) strcat(output, " ");
            strcat(output, BOX_LIGHT_V);
        }
        strcat(output, "\n");
        
        // Header separator (after first row)
        if (i == 0 && rows->count > 1) {
            strcat(output, BOX_LIGHT_LT);
            for (int j = 0; j < colCount; j++) {
                for (int k = 0; k < colWidths[j] + 2; k++) strcat(output, BOX_LIGHT_H);
                if (j < colCount - 1) strcat(output, BOX_LIGHT_X);
            }
            strcat(output, BOX_LIGHT_RT "\n");
        }
    }
    
    // Bottom border
    strcat(output, BOX_ROUND_BL);
    for (int j = 0; j < colCount; j++) {
        for (int k = 0; k < colWidths[j] + 2; k++) strcat(output, BOX_LIGHT_H);
        if (j < colCount - 1) strcat(output, BOX_LIGHT_BT);
    }
    strcat(output, BOX_ROUND_BR "\n");
    
    free(colWidths);
    
    ObjString* result = internString(vm, output, (int)strlen(output));
    free(output);
    return OBJ_VAL(result);
}

// ============================================================================
// Tree View
// ============================================================================

// Helper: lookup string key in map
static Value mapLookup(Map* m, const char* key, int keyLen) {
    int bucket;
    MapEntry* entry = mapFindEntry(m, key, keyLen, &bucket);
    if (entry) return entry->value;
    return NIL_VAL;
}

// Helper: recursive tree builder
static void buildTree(VM* vm, Value node, char* output, const char* prefix, bool isLast) {
    if (!IS_MAP(node)) return;
    
    Map* m = (Map*)AS_OBJ(node);
    
    // Get name
    Value nameVal = mapLookup(m, "name", 4);
    const char* name = IS_STRING(nameVal) ? AS_CSTRING(nameVal) : "?";
    
    // Print current node
    strcat(output, prefix);
    strcat(output, isLast ? TREE_LAST : TREE_BRANCH);
    strcat(output, name);
    strcat(output, "\n");
    
    // Get children
    Value childVal = mapLookup(m, "children", 8);
    
    if (IS_ARRAY(childVal)) {
        Array* children = (Array*)AS_OBJ(childVal);
        char newPrefix[1024];
        snprintf(newPrefix, sizeof(newPrefix), "%s%s", prefix, isLast ? TREE_SPACE : TREE_PIPE);
        
        for (int i = 0; i < children->count; i++) {
            buildTree(vm, children->items[i], output, newPrefix, i == children->count - 1);
        }
    }
}

// tree(data) - Render tree from nested map structure
static Value tui_tree(VM* vm, Value* args, int argCount) {
    if (argCount < 1 || !IS_MAP(args[0])) {
        return NIL_VAL;
    }
    
    Map* root = (Map*)AS_OBJ(args[0]);
    
    char* output = malloc(32768);
    if (!output) return NIL_VAL;
    output[0] = '\0';
    
    // Get root name
    Value nameVal = mapLookup(root, "name", 4);
    const char* rootName = IS_STRING(nameVal) ? AS_CSTRING(nameVal) : "root";
    
    strcat(output, rootName);
    strcat(output, "\n");
    
    // Get children
    Value childVal = mapLookup(root, "children", 8);
    
    if (IS_ARRAY(childVal)) {
        Array* children = (Array*)AS_OBJ(childVal);
        for (int i = 0; i < children->count; i++) {
            buildTree(vm, children->items[i], output, "", i == children->count - 1);
        }
    }
    
    ObjString* result = internString(vm, output, (int)strlen(output));
    free(output);
    return OBJ_VAL(result);
}

// ============================================================================
// Progress & Spinners
// ============================================================================

// progressBar(current, total, width) - Returns progress bar string
static Value tui_progressBar(VM* vm, Value* args, int argCount) {
    if (argCount < 2) return NIL_VAL;
    
    double current = IS_INT(args[0]) ? AS_INT(args[0]) : AS_FLOAT(args[0]);
    double total = IS_INT(args[1]) ? AS_INT(args[1]) : AS_FLOAT(args[1]);
    int width = argCount >= 3 ? (IS_INT(args[2]) ? AS_INT(args[2]) : (int)AS_FLOAT(args[2])) : 40;
    
    if (total <= 0) total = 1;
    double ratio = current / total;
    if (ratio > 1.0) ratio = 1.0;
    if (ratio < 0.0) ratio = 0.0;
    
    int filled = (int)(ratio * width);
    int percent = (int)(ratio * 100);
    
    char buffer[256];
    char bar[128] = "";
    
    for (int i = 0; i < width; i++) {
        if (i < filled) strcat(bar, "█");
        else strcat(bar, "░");
    }
    
    snprintf(buffer, sizeof(buffer), "[%s] %3d%%", bar, percent);
    
    ObjString* result = internString(vm, buffer, (int)strlen(buffer));
    return OBJ_VAL(result);
}

// spinner(style) - Returns array of spinner frames
static Value tui_spinner(VM* vm, Value* args, int argCount) {
    const char* style = "dots";
    if (argCount >= 1 && IS_STRING(args[0])) {
        style = AS_CSTRING(args[0]);
    }
    
    const char** frames;
    int frameCount;
    
    const char* dots[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
    const char* line[] = {"-", "\\", "|", "/"};
    const char* arc[] = {"◜", "◠", "◝", "◞", "◡", "◟"};
    const char* bounce[] = {"⠁", "⠂", "⠄", "⠂"};
    const char* dots2[] = {"⣾", "⣽", "⣻", "⢿", "⡿", "⣟", "⣯", "⣷"};
    
    if (strcmp(style, "line") == 0) {
        frames = line; frameCount = 4;
    } else if (strcmp(style, "arc") == 0) {
        frames = arc; frameCount = 6;
    } else if (strcmp(style, "bounce") == 0) {
        frames = bounce; frameCount = 4;
    } else if (strcmp(style, "dots2") == 0) {
        frames = dots2; frameCount = 8;
    } else {
        frames = dots; frameCount = 10;
    }
    
    Array* arr = newArray(vm);
    for (int i = 0; i < frameCount; i++) {
        ObjString* s = internString(vm, frames[i], (int)strlen(frames[i]));
        arrayPush(vm, arr, OBJ_VAL(s));
    }
    
    return OBJ_VAL(arr);
}

// ============================================================================
// Boxes & Panels
// ============================================================================

// box(content, style) - Wrap content in a box
static Value tui_box(VM* vm, Value* args, int argCount) {
    if (argCount < 1 || !IS_STRING(args[0])) return NIL_VAL;
    
    const char* content = AS_CSTRING(args[0]);
    const char* style = "rounded";
    if (argCount >= 2 && IS_STRING(args[1])) {
        style = AS_CSTRING(args[1]);
    }
    
    // Box characters based on style
    const char *tl, *tr, *bl, *br, *h, *v;
    if (strcmp(style, "double") == 0) {
        tl = BOX_DOUBLE_TL; tr = BOX_DOUBLE_TR;
        bl = BOX_DOUBLE_BL; br = BOX_DOUBLE_BR;
        h = BOX_DOUBLE_H; v = BOX_DOUBLE_V;
    } else if (strcmp(style, "simple") == 0) {
        tl = BOX_LIGHT_TL; tr = BOX_LIGHT_TR;
        bl = BOX_LIGHT_BL; br = BOX_LIGHT_BR;
        h = BOX_LIGHT_H; v = BOX_LIGHT_V;
    } else {
        tl = BOX_ROUND_TL; tr = BOX_ROUND_TR;
        bl = BOX_ROUND_BL; br = BOX_ROUND_BR;
        h = BOX_LIGHT_H; v = BOX_LIGHT_V;
    }
    
    // Split content by newlines and find max width
    int maxWidth = 0;
    const char* p = content;
    int lineLen = 0;
    
    while (*p) {
        if (*p == '\n') {
            if (lineLen > maxWidth) maxWidth = lineLen;
            lineLen = 0;
        } else {
            lineLen++;
        }
        p++;
    }
    if (lineLen > maxWidth) maxWidth = lineLen;
    
    char* output = malloc(32768);
    if (!output) return NIL_VAL;
    output[0] = '\0';
    
    // Top border
    strcat(output, tl);
    for (int i = 0; i < maxWidth + 2; i++) strcat(output, h);
    strcat(output, tr);
    strcat(output, "\n");
    
    // Content lines
    p = content;
    while (*p) {
        strcat(output, v);
        strcat(output, " ");
        
        int len = 0;
        while (*p && *p != '\n') {
            char c[2] = {*p, '\0'};
            strcat(output, c);
            len++;
            p++;
        }
        
        // Padding
        for (int i = len; i < maxWidth; i++) strcat(output, " ");
        strcat(output, " ");
        strcat(output, v);
        strcat(output, "\n");
        
        if (*p == '\n') p++;
    }
    
    // Bottom border
    strcat(output, bl);
    for (int i = 0; i < maxWidth + 2; i++) strcat(output, h);
    strcat(output, br);
    strcat(output, "\n");
    
    ObjString* result = internString(vm, output, (int)strlen(output));
    free(output);
    return OBJ_VAL(result);
}

// row(columns, spacing?) - Join components horizontally
static Value tui_row(VM* vm, Value* args, int argCount) {
    if (argCount < 1 || !IS_ARRAY(args[0])) {
        return NIL_VAL;
    }
    
    Array* cols = (Array*)AS_OBJ(args[0]);
    int spacing = 1;
    if (argCount >= 2) {
        if (IS_INT(args[1])) spacing = AS_INT(args[1]);
        else if (IS_FLOAT(args[1])) spacing = (int)AS_FLOAT(args[1]);
    }
    
    int count = cols->count;
    if (count == 0) return OBJ_VAL(internString(vm, "", 0));
    
    // 1. Analyze dimensions
    int* colWidths = malloc(sizeof(int) * count);
    int* colHeights = malloc(sizeof(int) * count);
    int maxTotalHeight = 0;
    
    // Helper simple struct to store separated lines could be useful but we can parse on fly or 2-pass
    // 2-pass is safer for calculation. 
    // Let's store split lines for efficiency to avoid re-splitting.
    // We need an array of (array of strings). 
    // Since we are in C, let's just use raw char pointers.
    
    typedef struct {
        char** lines;
        int lineCount;
        int width;
    } ColumnData;
    
    ColumnData* colData = malloc(sizeof(ColumnData) * count);
    
    for (int i = 0; i < count; i++) {
        Value v = cols->items[i];
        const char* str = IS_STRING(v) ? AS_CSTRING(v) : "";
        
        // Count lines and max width
        int lines = 0;
        int maxWidth = 0;
        int currentLen = 0;
        const char* p = str;
        
        // First pass: count lines
        if (*p) lines = 1; 
        while (*p) {
            if (*p == '\n') {
                if (currentLen > maxWidth) maxWidth = currentLen;
                currentLen = 0;
                if (*(p+1)) lines++; 
            } else {
                currentLen++;
            }
            p++;
        }
        if (currentLen > maxWidth) maxWidth = currentLen;
        
        colData[i].width = maxWidth;
        colData[i].lineCount = lines;
        if (lines > maxTotalHeight) maxTotalHeight = lines;
        
        // Store lines
        colData[i].lines = malloc(sizeof(char*) * lines);
        p = str;
        int l = 0;
        while (l < lines) {
            const char* start = p;
            int len = 0;
            while (*p && *p != '\n') {
                p++;
                len++;
            }
            
            char* lineStr = malloc(len + 1);
            memcpy(lineStr, start, len);
            lineStr[len] = '\0';
            colData[i].lines[l] = lineStr;
            
            if (*p == '\n') p++;
            l++;
        }
    }
    
    // 2. Render joined rows
    // Estimate output size: (sum(widths) + spacing*count) * height + margin
    int totalWidth = 0;
    for(int i=0; i<count; i++) totalWidth += colData[i].width;
    totalWidth += spacing * (count - 1);
    
    int estSize = (totalWidth + 2) * maxTotalHeight + 1024;
    char* output = malloc(estSize);
    output[0] = '\0';
    int outLen = 0;
    
    for (int y = 0; y < maxTotalHeight; y++) {
        for (int i = 0; i < count; i++) {
            ColumnData* cd = &colData[i];
            
            // Print line if exists, else padding
            if (y < cd->lineCount) {
                char* line = cd->lines[y];
                int len = (int)strlen(line); // assumes ascii mostly or simple
                // We must handle actual utf8 vs printed width? 
                // For now simple strlen. Advanced TUI needs wcwidth.
                strcat(output, line); // simple strcat for now, optimize later
                outLen += len;
                
                // Pad to col width
                for (int s = len; s < cd->width; s++) {
                    strcat(output, " ");
                    outLen++;
                }
            } else {
                // Empty line padding for this column
                for (int s = 0; s < cd->width; s++) {
                    strcat(output, " "); 
                    outLen++;
                }
            }
            
            // Spacing
            if (i < count - 1) {
                for (int s = 0; s < spacing; s++) {
                    strcat(output, " ");
                    outLen++;
                }
            }
        }
        strcat(output, "\n");
        outLen++;
    }
    
    // Cleanup
    for (int i = 0; i < count; i++) {
        for (int l = 0; l < colData[i].lineCount; l++) {
            free(colData[i].lines[l]);
        }
        free(colData[i].lines);
    }
    free(colData);
    free(colWidths);
    free(colHeights);
    
    ObjString* result = internString(vm, output, (int)strlen(output));
    free(output);
    return OBJ_VAL(result);
}
static Value tui_panel(VM* vm, Value* args, int argCount) {
    if (argCount < 2 || !IS_STRING(args[0]) || !IS_STRING(args[1])) {
        return NIL_VAL;
    }
    
    const char* title = AS_CSTRING(args[0]);
    const char* content = AS_CSTRING(args[1]);
    
    // Get terminal width
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    int termWidth = w.ws_col > 0 ? w.ws_col : 80;
    
    // Optional width parameter: 0 = auto, -1 = full terminal width, positive = fixed
    int requestedWidth = 0;
    if (argCount >= 3) {
        if (IS_INT(args[2])) requestedWidth = AS_INT(args[2]);
        else if (IS_FLOAT(args[2])) requestedWidth = (int)AS_FLOAT(args[2]);
        
        if (requestedWidth == -1) {
            requestedWidth = termWidth;
        }
    }
    
    // Calculate content width
    int titleLen = (int)strlen(title);
    int maxContentWidth = titleLen + 4;
    
    const char* p = content;
    int lineLen = 0;
    while (*p) {
        if (*p == '\n') {
            if (lineLen > maxContentWidth) maxContentWidth = lineLen;
            lineLen = 0;
        } else {
            lineLen++;
        }
        p++;
    }
    if (lineLen > maxContentWidth) maxContentWidth = lineLen;
    
    // Determine final box width
    int boxWidth;
    if (requestedWidth <= 0 || requestedWidth == -1) {
        // Auto width based on content
        boxWidth = maxContentWidth + 4;  // 4 = borders + padding
    } else {
        boxWidth = requestedWidth;
        if (boxWidth < titleLen + 6) boxWidth = titleLen + 6;
    }
    int contentWidth = boxWidth - 4;
    
    char* output = malloc(32768);
    if (!output) return NIL_VAL;
    output[0] = '\0';
    
    // Top border with title
    strcat(output, BOX_ROUND_TL);
    strcat(output, BOX_LIGHT_H);
    strcat(output, " ");
    strcat(output, title);
    strcat(output, " ");
    int remaining = boxWidth - titleLen - 5;  // 5 = TL + H + space + space + TR
    for (int i = 0; i < remaining && i < boxWidth; i++) strcat(output, BOX_LIGHT_H);
    strcat(output, BOX_ROUND_TR);
    strcat(output, "\n");
    
    // Content
    p = content;
    while (*p) {
        strcat(output, BOX_LIGHT_V);
        strcat(output, " ");
        
        int len = 0;
        while (*p && *p != '\n' && len < contentWidth) {
            char c[2] = {*p, '\0'};
            strcat(output, c);
            len++;
            p++;
        }
        // Skip rest of line if too long
        while (*p && *p != '\n') p++;
        
        for (int i = len; i < contentWidth; i++) strcat(output, " ");
        strcat(output, " ");
        strcat(output, BOX_LIGHT_V);
        strcat(output, "\n");
        
        if (*p == '\n') p++;
    }
    
    // Bottom border
    strcat(output, BOX_ROUND_BL);
    for (int i = 0; i < boxWidth - 2; i++) strcat(output, BOX_LIGHT_H);
    strcat(output, BOX_ROUND_BR);
    strcat(output, "\n");
    
    ObjString* result = internString(vm, output, (int)strlen(output));
    free(output);
    return OBJ_VAL(result);
}

// ============================================================================
// Module Registration
// ============================================================================

void registerUCoreTui(VM* vm) {
    ObjString* modNameObj = internString(vm, "ucoreTui", 8);
    char* modName = modNameObj->chars;
    
    Module* mod = ALLOCATE_OBJ(vm, Module, OBJ_MODULE);
    mod->name = strdup(modName);
    mod->source = NULL;
    mod->obj.isMarked = true;
    mod->obj.isPermanent = true;
    
    Environment* modEnv = ALLOCATE_OBJ(vm, Environment, OBJ_ENVIRONMENT);
    memset(modEnv->buckets, 0, sizeof(modEnv->buckets));
    memset(modEnv->funcBuckets, 0, sizeof(modEnv->funcBuckets));
    modEnv->enclosing = NULL;
    modEnv->obj.isMarked = true;
    modEnv->obj.isPermanent = true;
    mod->env = modEnv;

    // Terminal primitives
    defineNative(vm, mod->env, "clear", tui_clear, 0);
    defineNative(vm, mod->env, "clearLine", tui_clearLine, 0);
    defineNative(vm, mod->env, "moveTo", tui_moveTo, 2);
    defineNative(vm, mod->env, "hideCursor", tui_hideCursor, 0);
    defineNative(vm, mod->env, "showCursor", tui_showCursor, 0);
    defineNative(vm, mod->env, "size", tui_size, 0);
    
    // Colors & styling
    defineNative(vm, mod->env, "fg", tui_fg, 2);
    defineNative(vm, mod->env, "bg", tui_bg, 2);
    defineNative(vm, mod->env, "bold", tui_bold, 1);
    defineNative(vm, mod->env, "dim", tui_dim, 1);
    defineNative(vm, mod->env, "italic", tui_italic, 1);
    defineNative(vm, mod->env, "underline", tui_underline, 1);
    defineNative(vm, mod->env, "style", tui_style, 2);
    
    // Input handling
    defineNative(vm, mod->env, "keypress", tui_keypress, 0);
    defineNative(vm, mod->env, "input", tui_input, 1);
    defineNative(vm, mod->env, "inputPassword", tui_inputPassword, 1);
    defineNative(vm, mod->env, "confirm", tui_confirm, 1);
    defineNative(vm, mod->env, "select", tui_select, 2);
    defineNative(vm, mod->env, "inputBox", tui_inputBox, 3);
    defineNative(vm, mod->env, "inputPasswordBox", tui_inputPasswordBox, 2);
    
    // Tables & trees
    defineNative(vm, mod->env, "table", tui_table, 1);
    defineNative(vm, mod->env, "tree", tui_tree, 1);
    
    // Progress & spinners
    defineNative(vm, mod->env, "progressBar", tui_progressBar, 3);
    defineNative(vm, mod->env, "spinner", tui_spinner, 1);
    
    // Boxes & panels
    defineNative(vm, mod->env, "box", tui_box, 2);
    defineNative(vm, mod->env, "panel", tui_panel, 3);
    defineNative(vm, mod->env, "row", tui_row, 2);

    Value vMod = OBJ_VAL(mod);
    defineGlobal(vm, "ucoreTui", vMod);
}
