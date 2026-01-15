#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <regex.h>
#include "ucore_string.h"
#include "vm.h"

// Helper macros/functions
#ifndef AS_ARRAY
#define AS_ARRAY(v) ((Array*)AS_OBJ(v))
#endif

static ObjString* copyString(VM* vm, const char* chars, int length) {
    return internString(vm, chars, length);
}

// ============================================================================
// Core String Manipulation
// ============================================================================

// ucoreString.split(str, delimiter) -> List<String>
static Value str_split(VM* vm, Value* args, int argCount) {
    if (argCount != 2 || !IS_STRING(args[0]) || !IS_STRING(args[1])) {
        return NIL_VAL;
    }
    
    ObjString* strObj = AS_STRING(args[0]);
    ObjString* delimObj = AS_STRING(args[1]);
    
    const char* str = strObj->chars;
    const char* delim = delimObj->chars;
    int delimLen = strlen(delim);
    
    // Create result array
    Array* array = newArray(vm);
    
    if (delimLen == 0) {
        // Split by character
        for (int i = 0; i < strObj->length; i++) {
            char temp[2] = { str[i], '\0' };
            Value val = OBJ_VAL(internString(vm, temp, 1));
            arrayPush(vm, array, val);
        }
        return OBJ_VAL(array);
    }
    
    const char* start = str;
    const char* p = strstr(start, delim);
    
    while (p) {
        int len = p - start;
        Value val = OBJ_VAL(copyString(vm, start, len)); // efficient copy
        arrayPush(vm, array, val);
        
        start = p + delimLen;
        p = strstr(start, delim);
    }
    
    // Last segment
    Value val = OBJ_VAL(copyString(vm, start, strlen(start)));
    arrayPush(vm, array, val);
    
    return OBJ_VAL(array);
}

// ucoreString.join(list, delimiter) -> String
static Value str_join(VM* vm, Value* args, int argCount) {
    if (argCount != 2 || !IS_ARRAY(args[0]) || !IS_STRING(args[1])) {
        return NIL_VAL;
    }
    
    Array* list = AS_ARRAY(args[0]);
    ObjString* delim = AS_STRING(args[1]);
    
    if (list->count == 0) {
        return OBJ_VAL(internString(vm, "", 0));
    }
    
    // Calculate total length first to allocate once (Performance improvement)
    size_t totalLen = 0;
    int delimLen = delim->length;
    
    for (int i = 0; i < list->count; i++) {
        Value item = list->items[i];
        if (IS_STRING(item)) {
            totalLen += AS_STRING(item)->length;
        } else {
            // Non-string items? For now ignore or simple error. 
            // Or assume toString logic exists. Let's just handle strings for speed.
        }
        if (i < list->count - 1) totalLen += delimLen;
    }
    
    char* result = malloc(totalLen + 1);
    char* ptr = result;
    
    for (int i = 0; i < list->count; i++) {
        Value item = list->items[i];
        if (IS_STRING(item)) {
            ObjString* s = AS_STRING(item);
            memcpy(ptr, s->chars, s->length);
            ptr += s->length;
        }
        
        if (i < list->count - 1) {
            memcpy(ptr, delim->chars, delimLen);
            ptr += delimLen;
        }
    }
    *ptr = '\0';
    
    // takeString assumes ownership of 'result' pointer
    // We need to implement takeString or use internString and free result.
    // 'takeString' usually exists in VM to avoid copy.
    // Checking vm.h: only internString is exposed.
    // So we use internString, then free result.
    ObjString* resObj = internString(vm, result, totalLen);
    free(result);
    return OBJ_VAL(resObj);
}

// ucoreString.replace(str, search, replace) -> String
static Value str_replace(VM* vm, Value* args, int argCount) {
    if (argCount != 3 || !IS_STRING(args[0]) || !IS_STRING(args[1]) || !IS_STRING(args[2])) {
        return NIL_VAL;
    }

    ObjString* strObj = AS_STRING(args[0]);
    ObjString* searchObj = AS_STRING(args[1]);
    ObjString* repObj = AS_STRING(args[2]);

    if (searchObj->length == 0) return args[0]; // No change

    const char* str = strObj->chars;
    const char* search = searchObj->chars;
    const char* rep = repObj->chars;
    
    // Count occurrences
    int count = 0;
    const char* tmp = str;
    while((tmp = strstr(tmp, search))) {
        count++;
        tmp += searchObj->length;
    }
    
    if (count == 0) return args[0];
    
    // Allocate new string
    size_t newLen = strObj->length + count * (repObj->length - searchObj->length);
    char* result = malloc(newLen + 1);
    
    char* dst = result;
    const char* src = str;
    tmp = strstr(src, search);
    
    while (tmp) {
        int len = tmp - src;
        memcpy(dst, src, len);
        dst += len;
        
        memcpy(dst, rep, repObj->length);
        dst += repObj->length;
        
        src = tmp + searchObj->length;
        tmp = strstr(src, search);
    }
    strcpy(dst, src);
    
    ObjString* resObj = internString(vm, result, newLen);
    free(result);
    return OBJ_VAL(resObj);
}

// ucoreString.trim(str) -> String
static Value str_trim(VM* vm, Value* args, int argCount) {
    if (argCount != 1 || !IS_STRING(args[0])) return NIL_VAL;
    
    ObjString* strObj = AS_STRING(args[0]);
    if (strObj->length == 0) return args[0];
    
    const char* start = strObj->chars;
    const char* end = start + strObj->length - 1;
    
    while (*start && isspace((unsigned char)*start)) start++;
    while (end > start && isspace((unsigned char)*end)) end--;
    
    // New length
    int len = (int)(end - start) + 1;
    if (len < 0) len = 0;
    
    return OBJ_VAL(copyString(vm, start, len));
}

// ucoreString.toLower(str) -> String
static Value str_toLower(VM* vm, Value* args, int argCount) {
    if (argCount != 1 || !IS_STRING(args[0])) return NIL_VAL;
    ObjString* str = AS_STRING(args[0]);
    
    char* buf = malloc(str->length + 1);
    for(int i=0; i<str->length; i++) {
        buf[i] = tolower((unsigned char)str->chars[i]);
    }
    buf[str->length] = '\0';
    ObjString* res = internString(vm, buf, str->length);
    free(buf);
    return OBJ_VAL(res);
}

// ucoreString.toUpper(str) -> String
static Value str_toUpper(VM* vm, Value* args, int argCount) {
    if (argCount != 1 || !IS_STRING(args[0])) return NIL_VAL;
    ObjString* str = AS_STRING(args[0]);
    
    char* buf = malloc(str->length + 1);
    for(int i=0; i<str->length; i++) {
        buf[i] = toupper((unsigned char)str->chars[i]);
    }
    buf[str->length] = '\0';
    ObjString* res = internString(vm, buf, str->length);
    free(buf);
    return OBJ_VAL(res);
}

// ucoreString.contains(str, substr) -> bool
static Value str_contains(VM* vm, Value* args, int argCount) {
    if (argCount != 2 || !IS_STRING(args[0]) || !IS_STRING(args[1])) return BOOL_VAL(false);
    ObjString* haystack = AS_STRING(args[0]);
    ObjString* needle = AS_STRING(args[1]);
    return BOOL_VAL(strstr(haystack->chars, needle->chars) != NULL);
}

// ============================================================================
// Regex Support (POSIX)
// ============================================================================

// ucoreString.match(str, pattern) -> bool
static Value str_match(VM* vm, Value* args, int argCount) {
    if (argCount != 2 || !IS_STRING(args[0]) || !IS_STRING(args[1])) {
        return BOOL_VAL(false);
    }
    
    ObjString* str = AS_STRING(args[0]);
    ObjString* pattern = AS_STRING(args[1]);
    
    regex_t regex;
    int ret;
    
    // Compile regex
    ret = regcomp(&regex, pattern->chars, REG_EXTENDED | REG_NOSUB);
    if (ret) {
        // Could print error or return false/nil
        return NIL_VAL; 
    }
    
    // Execute
    ret = regexec(&regex, str->chars, 0, NULL, 0);
    regfree(&regex);
    
    return BOOL_VAL(ret == 0);
}

// ucoreString.extract(str, pattern) -> List<String> (All matches)
static Value str_extract(VM* vm, Value* args, int argCount) {
    if (argCount != 2 || !IS_STRING(args[0]) || !IS_STRING(args[1])) {
        return NIL_VAL;
    }
    
    ObjString* strObj = AS_STRING(args[0]);
    ObjString* patternObj = AS_STRING(args[1]);
    
    regex_t regex;
    int ret = regcomp(&regex, patternObj->chars, REG_EXTENDED);
    if (ret) return NIL_VAL;
    
    Array* results = newArray(vm);
    
    const char* cursor = strObj->chars;
    regmatch_t pmatch[1];
    
    // Find all non-overlapping matches
    while (1) {
        ret = regexec(&regex, cursor, 1, pmatch, 0);
        if (ret != 0) break; // formatting error or no match
        
        int start = pmatch[0].rm_so;
        int end = pmatch[0].rm_eo;
        int len = end - start;
        
        Value matchVal = OBJ_VAL(copyString(vm, cursor + start, len));
        arrayPush(vm, results, matchVal);
        
        cursor += end; // Advance past match
        if (len == 0 && *cursor) cursor++; // Avoid infinite loop on empty match
    }
    
    regfree(&regex);
    return OBJ_VAL(results);
}


// ============================================================================
// Registration
// ============================================================================

void registerUCoreString(VM* vm) {
    ObjString* modNameObj = internString(vm, "ucoreString", 11);
    
    Module* mod = ALLOCATE_OBJ(vm, Module, OBJ_MODULE);
    mod->name = strdup(modNameObj->chars);
    mod->source = NULL;
    mod->obj.isMarked = true;
    
    Environment* modEnv = ALLOCATE_OBJ(vm, Environment, OBJ_ENVIRONMENT);
    memset(modEnv->buckets, 0, sizeof(modEnv->buckets));
    memset(modEnv->funcBuckets, 0, sizeof(modEnv->funcBuckets));
    modEnv->enclosing = NULL;
    modEnv->obj.isMarked = true; 
    modEnv->obj.isPermanent = true;
    mod->env = modEnv;

    defineNative(vm, mod->env, "split", str_split, 2);
    defineNative(vm, mod->env, "join", str_join, 2);
    defineNative(vm, mod->env, "replace", str_replace, 3);
    defineNative(vm, mod->env, "trim", str_trim, 1);
    defineNative(vm, mod->env, "toLower", str_toLower, 1);
    defineNative(vm, mod->env, "toUpper", str_toUpper, 1);
    defineNative(vm, mod->env, "contains", str_contains, 2);
    defineNative(vm, mod->env, "match", str_match, 2);
    defineNative(vm, mod->env, "extract", str_extract, 2);
    
    Value vMod = OBJ_VAL(mod);
    defineGlobal(vm, "ucoreString", vMod);
}
