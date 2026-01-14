#include "ucore_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

// ============================================================================
// Helpers
// ============================================================================

static void push(VM* vm, Value val) {
    if (vm->stackTop >= STACK_MAX) {
        printf("Stack overflow in JSON parser\n");
        exit(1);
    }
    vm->stack[vm->stackTop++] = val;
}

static Value pop(VM* vm) {
    vm->stackTop--;
    return vm->stack[vm->stackTop];
}

static ObjString* copyString(VM* vm, const char* chars, int length) {
    return internString(vm, chars, length);
}
// ============================================================================
// JSON Parser
// ============================================================================

typedef struct {
    const char* start;
    const char* current;
    VM* vm;
    char errorMsg[256];
    bool hasError;
} JsonParser;

static void jsonError(JsonParser* parser, const char* msg) {
    if (parser->hasError) return;
    snprintf(parser->errorMsg, sizeof(parser->errorMsg), "%s (at char %ld)", msg, (long)(parser->current - parser->start));
    parser->hasError = true;
}

static void skipWhitespace(JsonParser* parser) {
    while (*parser->current != '\0' && isspace((unsigned char)*parser->current)) {
        parser->current++;
    }
}

static char peek(JsonParser* parser) {
    skipWhitespace(parser);
    return *parser->current;
}

// static char advance(JsonParser* parser) { ... } // Unused

static bool match(JsonParser* parser, char expected) {
    if (peek(parser) == expected) {
        parser->current++;
        return true;
    }
    return false;
}

// Forward declarations of parsing functions
static Value parseValue(JsonParser* parser);

static Value parseString(JsonParser* parser) {
    if (*parser->current != '"') {
        jsonError(parser, "Expected '\"'");
        return NIL_VAL;
    }
    parser->current++; // skip opening quote

    // Simple implementation: find closing quote, handle escapes manually if needed
    // rigorous implementation would allocate a buffer.
    // For now, we will scan to end, handle simple escapes, copy to new buffer.
    
    // First pass: calc length
    const char* strStart = parser->current;
    int len = 0;
    while (*parser->current != '"' && *parser->current != '\0') {
        if (*parser->current == '\\') {
            parser->current++;
            if (*parser->current == '\0') {
                jsonError(parser, "Unterminated string escape");
                return NIL_VAL;
            }
        }
        parser->current++;
        len++;
    }

    if (*parser->current != '"') {
        jsonError(parser, "Unterminated string");
        return NIL_VAL;
    }

    // Allocate buffer
    char* buffer = (char*)malloc(len + 1);
    if (!buffer) {
        jsonError(parser, "Out of memory");
        return NIL_VAL;
    }

    // copy
    const char* scan = strStart;
    char* dest = buffer;
    while (scan < parser->current) {
        if (*scan == '\\') {
            scan++;
            switch (*scan) {
                case '"': *dest++ = '"'; break;
                case '\\': *dest++ = '\\'; break;
                case '/': *dest++ = '/'; break;
                case 'b': *dest++ = '\b'; break;
                case 'f': *dest++ = '\f'; break;
                case 'n': *dest++ = '\n'; break;
                case 'r': *dest++ = '\r'; break;
                case 't': *dest++ = '\t'; break;
                case 'u': 
                    // skipping unicode for now... simply consume uXXXX
                    scan += 4; 
                    *dest++ = '?'; // placeholder
                    break;
                default: *dest++ = *scan; break;
            }
        } else {
            *dest++ = *scan;
        }
        scan++;
    }
    *dest = '\0';
    parser->current++; // skip closing quote

    ObjString* os = copyString(parser->vm, buffer, (int)(dest - buffer));
    free(buffer);
    return OBJ_VAL(os);
}

static Value parseNumber(JsonParser* parser) {
    const char* start = parser->current;
    if (*parser->current == '-') parser->current++;
    while (isdigit((unsigned char)*parser->current)) parser->current++;
    
    // fraction
    if (*parser->current == '.') {
        parser->current++;
        while (isdigit((unsigned char)*parser->current)) parser->current++;
    }

    // exponent
    if (*parser->current == 'e' || *parser->current == 'E') {
        parser->current++;
        if (*parser->current == '+' || *parser->current == '-') parser->current++;
        while (isdigit((unsigned char)*parser->current)) parser->current++;
    }

    size_t len = parser->current - start;
    
    // OPTIMIZATION: Use stack buffer for small numbers (most common case)
    char stackBuf[64];
    char* tmp = stackBuf;
    bool heapAlloc = false;
    if (len >= sizeof(stackBuf)) {
        tmp = (char*)malloc(len + 1);
        heapAlloc = true;
    }
    memcpy(tmp, start, len);
    tmp[len] = '\0';
    
    // OPTIMIZATION: Check for float during scan instead of second loop
    bool isInt = true;
    for (const char* p = start; p < parser->current; p++) {
        if (*p == '.' || *p == 'e' || *p == 'E') {
            isInt = false; 
            break;
        }
    }

    Value val;
    if (isInt) {
        long long v = strtoll(tmp, NULL, 10);
        val = INT_VAL((int)v);
    } else {
        double v = strtod(tmp, NULL);
        val = FLOAT_VAL(v);
    }
    if (heapAlloc) free(tmp);
    return val;
}

static Value parseArray(JsonParser* parser) {
    parser->current++; // skip '['
    Array* arr = newArray(parser->vm);
    
    // GC Protection: While we fill this array, if GC triggers, 'arr' must be reachable.
    // In unnarize, 'newArray' puts it on heap, but it's not rooted yet unless passed to push() or something deeply?
    // Actually, 'newArray' links it into vm->objects.
    // If GC runs, it will sweep 'arr' unless it's marked.
    // We should push 'arr' to stack temporarily using `push(vm, val)`.
    
    push(parser->vm, OBJ_VAL(arr)); // PROTECT

    skipWhitespace(parser);
    if (*parser->current == ']') {
        parser->current++;
        pop(parser->vm); // UNPROTECT
        return OBJ_VAL(arr);
    }

    while (true) {
        Value val = parseValue(parser);
        if (parser->hasError) break;
        
        push(parser->vm, val); // Protect item
        arrayPush(parser->vm, arr, val);
        pop(parser->vm);       // Unprotect item

        skipWhitespace(parser);
        if (*parser->current == ']') {
            parser->current++;
            break;
        }

        if (!match(parser, ',')) {
            jsonError(parser, "Expected ',' in array");
            break;
        }
    }

    pop(parser->vm); // UNPROTECT arr
    return OBJ_VAL(arr);
}

static Value parseObject(JsonParser* parser) {
    parser->current++; // skip '{'
    Map* map = newMap(parser->vm);
    
    push(parser->vm, OBJ_VAL(map)); // PROTECT

    skipWhitespace(parser);
    if (*parser->current == '}') {
        parser->current++;
        pop(parser->vm); // UNPROTECT
        return OBJ_VAL(map);
    }

    while (true) {
        skipWhitespace(parser);
        if (*parser->current != '"') {
            jsonError(parser, "Expected string key");
            break;
        }

        Value keyVal = parseString(parser);
        if (parser->hasError) break;
        
        // key must be string
        ObjString* keyStr = AS_STRING(keyVal);
        push(parser->vm, keyVal); // Protect Key

        skipWhitespace(parser);
        if (!match(parser, ':')) {
            jsonError(parser, "Expected ':' after key");
            pop(parser->vm); // pop key
            break;
        }

        Value val = parseValue(parser);
        if (parser->hasError) {
             pop(parser->vm);
             break;
        }

        push(parser->vm, val); // Protect Val
        mapSetStr(map, keyStr->chars, keyStr->length, val);
        pop(parser->vm); // pop val
        pop(parser->vm); // pop key

        skipWhitespace(parser);
        if (*parser->current == '}') {
            parser->current++;
            break;
        }

        if (!match(parser, ',')) {
            jsonError(parser, "Expected ',' in object");
            break;
        }
    }

    pop(parser->vm); // UNPROTECT map
    return OBJ_VAL(map);
}

static Value parseValue(JsonParser* parser) {
    skipWhitespace(parser);
    if (parser->hasError) return NIL_VAL;

    char c = *parser->current;
    if (c == '"') return parseString(parser);
    if (c == '[') return parseArray(parser);
    if (c == '{') return parseObject(parser);
    if (isdigit((unsigned char)c) || c == '-') return parseNumber(parser);
    
    if (strncmp(parser->current, "true", 4) == 0) {
        parser->current += 4;
        return BOOL_VAL(true);
    }
    if (strncmp(parser->current, "false", 5) == 0) {
        parser->current += 5;
        return BOOL_VAL(false);
    }
    if (strncmp(parser->current, "null", 4) == 0) {
        parser->current += 4;
        return NIL_VAL;
    }

    jsonError(parser, "Unexpected character");
    return NIL_VAL;
}

static Value jsonParse(VM* vm, Value* args, int argCount) {
    if (argCount != 1 || !IS_STRING(args[0])) {
        return OBJ_VAL(copyString(vm, "Error: parse() expects a JSON string", 36)); 
        // Or return NIL and print error? Standard pattern is probably to error out or return nil
        // Let's print runtime error for now?
        // Actually, returning a result object or throwing would be better.
        // For simplicity, let's just return NIL if bad args.
    }

    ObjString* jsonStr = AS_STRING(args[0]);
    JsonParser parser;
    parser.vm = vm;
    parser.start = jsonStr->chars;
    parser.current = jsonStr->chars;
    parser.hasError = false;
    parser.errorMsg[0] = '\0';

    Value result = parseValue(&parser);
    
    if (parser.hasError) {
        // Return error as string? Or just print?
        // Let's just print to stderr and return NIL
        printf("JSON Parse Error: %s\n", parser.errorMsg);
        return NIL_VAL;
    }
    
    // Check if we consumed everything? 
    skipWhitespace(&parser);
    if (*parser.current != '\0') {
         printf("JSON Parse Error: Extra data at end of JSON\n");
         return NIL_VAL;
    }

    return result;
}

// ============================================================================
// JSON Stringifier
// ============================================================================

typedef struct {
    char* buffer;
    size_t capacity;
    size_t length;
    VM* vm;
} JsonHeader;



static void jsonAppend(JsonHeader* header, const char* str, size_t len) {
    if (header->length + len >= header->capacity) {
        header->capacity = (header->capacity < 8) ? 8 : header->capacity * 2;
        while (header->length + len >= header->capacity) header->capacity *= 2;
        header->buffer = (char*)realloc(header->buffer, header->capacity);
        if (!header->buffer) exit(1);
    }
    memcpy(header->buffer + header->length, str, len);
    header->length += len;
    header->buffer[header->length] = '\0';
}

static void stringifyValue(JsonHeader* header, Value val);

static void stringifyArray(JsonHeader* header, Array* arr) {
    jsonAppend(header, "[", 1);
    for (int i = 0; i < arr->count; i++) {
        stringifyValue(header, arr->items[i]);
        if (i < arr->count - 1) jsonAppend(header, ",", 1);
    }
    jsonAppend(header, "]", 1);
}

static void stringifyMap(JsonHeader* header, Map* map) {
    jsonAppend(header, "{", 1);
    bool first = true;
    for (int i = 0; i < TABLE_SIZE; i++) {
        MapEntry* e = map->buckets[i];
        while (e) {
            if (!first) jsonAppend(header, ",", 1);
            first = false;
            
            // Key
            jsonAppend(header, "\"", 1);
            jsonAppend(header, e->key, strlen(e->key)); // key is char*
            jsonAppend(header, "\":", 2);
            
            // Value
            stringifyValue(header, e->value);
            
            e = e->next;
        }
    }
    jsonAppend(header, "}", 1);
}

static void stringifyValue(JsonHeader* header, Value val) {
    if (IS_NIL(val)) {
        jsonAppend(header, "null", 4);
    } else if (IS_BOOL(val)) {
        if (AS_BOOL(val)) jsonAppend(header, "true", 4);
        else jsonAppend(header, "false", 5);
    } else if (IS_INT(val)) {
        char buf[32];
        int len = snprintf(buf, sizeof(buf), "%d", AS_INT(val));
        jsonAppend(header, buf, len);
    } else if (IS_FLOAT(val)) {
        char buf[32];
        int len = snprintf(buf, sizeof(buf), "%g", AS_FLOAT(val));
        jsonAppend(header, buf, len);
    } else if (IS_STRING(val)) {
        ObjString* s = AS_STRING(val);
        // OPTIMIZATION: Pre-calculate escaped length and batch write
        int escapeCount = 0;
        for (int i = 0; i < s->length; i++) {
            char c = s->chars[i];
            if (c == '"' || c == '\\' || c == '\n' || c == '\r' || c == '\t') escapeCount++;
        }
        
        // Reserve space: quotes(2) + length + escapes
        size_t needed = 2 + s->length + escapeCount;
        if (header->length + needed >= header->capacity) {
            while (header->length + needed >= header->capacity) header->capacity *= 2;
            header->buffer = (char*)realloc(header->buffer, header->capacity);
        }
        
        char* dest = header->buffer + header->length;
        *dest++ = '"';
        for (int i = 0; i < s->length; i++) {
            char c = s->chars[i];
            if (c == '"') { *dest++ = '\\'; *dest++ = '"'; }
            else if (c == '\\') { *dest++ = '\\'; *dest++ = '\\'; }
            else if (c == '\n') { *dest++ = '\\'; *dest++ = 'n'; }
            else if (c == '\r') { *dest++ = '\\'; *dest++ = 'r'; }
            else if (c == '\t') { *dest++ = '\\'; *dest++ = 't'; }
            else { *dest++ = c; }
        }
        *dest++ = '"';
        header->length = dest - header->buffer;
        header->buffer[header->length] = '\0';
    } else if (IS_ARRAY(val)) {
        stringifyArray(header, (Array*)AS_OBJ(val));
    } else if (IS_MAP(val)) {
        stringifyMap(header, (Map*)AS_OBJ(val));
    } else {
        jsonAppend(header, "\"UnsupportedType\"", 17);
    }
}

static Value jsonStringify(VM* vm, Value* args, int argCount) {
    if (argCount < 1) return NIL_VAL;
    
    JsonHeader header;
    header.vm = vm;
    header.capacity = 1024; // OPTIMIZATION: Larger initial buffer
    header.length = 0;
    header.buffer = (char*)malloc(header.capacity);
    header.buffer[0] = '\0';
    
    stringifyValue(&header, args[0]);
    
    ObjString* res = copyString(vm, header.buffer, (int)header.length);
    free(header.buffer);
    return OBJ_VAL(res);
}

// ============================================================================
// File CRUD
// ============================================================================

static Value jsonRead(VM* vm, Value* args, int argCount) {
    if (argCount != 1 || !IS_STRING(args[0])) {
         return OBJ_VAL(copyString(vm, "Error: read(path) expects a file path string", 44));
    }
    char* path = AS_CSTRING(args[0]);
    
    FILE* file = fopen(path, "rb");
    if (!file) {
        return NIL_VAL; // File not found or err
    }

    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    char* buffer = malloc(fileSize + 1);
    if (!buffer) {
        fclose(file);
        return NIL_VAL;
    }
    
    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    buffer[bytesRead] = '\0';
    fclose(file);
    
    // Create temp string value to pass to internal parse
    ObjString* jsonStr = copyString(vm, buffer, bytesRead);
    free(buffer);
    
    push(vm, OBJ_VAL(jsonStr));
    
    // Reuse parser logic manually
    JsonParser parser;
    parser.vm = vm;
    parser.start = jsonStr->chars;
    parser.current = jsonStr->chars;
    parser.hasError = false;
    parser.errorMsg[0] = '\0';

    Value result = parseValue(&parser);
    
    pop(vm); // jsonStr

    if (parser.hasError) {
        printf("JSON Read Error: %s in %s\n", parser.errorMsg, path);
        return NIL_VAL;
    }
    
    return result;
}

static Value jsonWrite(VM* vm, Value* args, int argCount) {
    if (argCount != 2 || !IS_STRING(args[0])) {
        return BOOL_VAL(false);
    }
    char* path = AS_CSTRING(args[0]);
    Value data = args[1];
    
    // Stringify
    JsonHeader header;
    header.vm = vm;
    header.capacity = 128;
    header.length = 0;
    header.buffer = (char*)malloc(header.capacity);
    header.buffer[0] = '\0';
    
    stringifyValue(&header, data);
    
    FILE* file = fopen(path, "w");
    if (!file) {
        free(header.buffer);
        return BOOL_VAL(false);
    }
    
    // OPTIMIZATION: Use fwrite for better performance than fprintf
    fwrite(header.buffer, 1, header.length, file);
    fclose(file);
    free(header.buffer);
    
    return BOOL_VAL(true);
}

static Value jsonRemove(VM* vm, Value* args, int argCount) {
    (void)vm; // Unused
    (void)argCount;
    if (!IS_STRING(args[0])) {
        return BOOL_VAL(false);
    }
    char* path = AS_CSTRING(args[0]);
    if (unlink(path) == 0) {
        return BOOL_VAL(true);
    }
    return BOOL_VAL(false);
}

// ============================================================================
// Registration
// ============================================================================

void registerUCoreJson(VM* vm) {
    ObjString* modNameObj = internString(vm, "ucoreJson", 9);
    char* modName = modNameObj->chars;
    
    // Create Module manually
    Module* mod = ALLOCATE_OBJ(vm, Module, OBJ_MODULE);
    mod->name = strdup(modName);
    mod->source = NULL;
    mod->obj.isMarked = true; 
    mod->obj.isPermanent = true; // PERMANENT ROOT
    
    // CRITICAL: Use ALLOCATE_OBJ for Environment to enable GC tracking
    Environment* modEnv = ALLOCATE_OBJ(vm, Environment, OBJ_ENVIRONMENT);
    memset(modEnv->buckets, 0, sizeof(modEnv->buckets));
    memset(modEnv->funcBuckets, 0, sizeof(modEnv->funcBuckets));
    modEnv->enclosing = NULL;
    modEnv->obj.isMarked = true; 
    modEnv->obj.isPermanent = true; // PERMANENT ROOT
    mod->env = modEnv;
    
    // Protect module during native registration
    push(vm, OBJ_VAL(mod));
    
    defineNative(vm, mod->env, "parse", jsonParse, 1);
    defineNative(vm, mod->env, "stringify", jsonStringify, 1);
    
    defineNative(vm, mod->env, "read", jsonRead, 1);
    defineNative(vm, mod->env, "write", jsonWrite, 2);
    defineNative(vm, mod->env, "remove", jsonRemove, 1);
    
    pop(vm); // unprotect
    
    // Define global 'ucoreJson'
    Value vMod = OBJ_VAL(mod);
    defineGlobal(vm, "ucoreJson", vMod);
}
