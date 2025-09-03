// Example Unnarize external plugin: stringLib (self-contained)
// Provides: strRepeat(s, n), toUpper(s)
// Build: gcc -shared -fPIC -o ../build/stringLib.so stringLib.c -ldl

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <dlfcn.h>

// Minimal mirror of interpreter public types
typedef struct VM VM; // opaque

typedef enum {
    VAL_INT,
    VAL_FLOAT,
    VAL_STRING,
    VAL_BOOL,
    VAL_MODULE,
    VAL_ARRAY,
    VAL_MAP
} ValueType;

typedef struct Value {
    ValueType type;
    union {
        int intVal;
        double floatVal;
        char* stringVal;
        int bool_storage;
        void* moduleVal;
        void* arrayVal;
        void* mapVal;
    };
} Value;

typedef Value (*NativeFn)(VM*, Value* args, int argCount);
typedef void (*RegisterFn)(VM*, const char* name, NativeFn function);

static Value makeString(const char* s) {
    Value v; v.type = VAL_STRING;
    if (!s) { v.stringVal = NULL; return v; }
    size_t len = strlen(s);
    v.stringVal = (char*)malloc(len + 1);
    if (!v.stringVal) { v.stringVal = NULL; return v; }
    memcpy(v.stringVal, s, len + 1);
    return v;
}

static Value strRepeat(VM* vm, Value* args, int argCount) {
    (void)vm;
    if (argCount != 2 || args[0].type != VAL_STRING || args[1].type != VAL_INT) {
        return makeString("");
    }
    const char* s = args[0].stringVal ? args[0].stringVal : "";
    int n = args[1].intVal;
    if (n <= 0) return makeString("");
    size_t slen = strlen(s);
    size_t outlen = slen * (size_t)n;
    char* buf = (char*)malloc(outlen + 1);
    if (!buf) return makeString("");
    char* p = buf;
    for (int i = 0; i < n; i++) {
        memcpy(p, s, slen);
        p += slen;
    }
    buf[outlen] = '\0';
    Value v; v.type = VAL_STRING; v.stringVal = buf; return v;
}

static Value toUpperFn(VM* vm, Value* args, int argCount) {
    (void)vm;
    if (argCount != 1 || args[0].type != VAL_STRING) {
        return makeString("");
    }
    const char* s = args[0].stringVal ? args[0].stringVal : "";
    size_t len = strlen(s);
    char* buf = (char*)malloc(len + 1);
    if (!buf) return makeString("");
    for (size_t i = 0; i < len; i++) buf[i] = (char)toupper((unsigned char)s[i]);
    buf[len] = '\0';
    Value v; v.type = VAL_STRING; v.stringVal = buf; return v;
}

void registerFunctions(VM* vm) {
    RegisterFn reg_fn = (RegisterFn)dlsym(RTLD_DEFAULT, "registerNativeFunction");
    if (!reg_fn) {
        const char* err = dlerror();
        fprintf(stderr, "[plugin stringLib] dlsym(registerNativeFunction) failed: %s\n", err ? err : "unknown error");
        return;
    }
    reg_fn(vm, "strRepeat", &strRepeat);
    reg_fn(vm, "toUpper", &toUpperFn);
}
