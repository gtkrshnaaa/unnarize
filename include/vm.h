#ifndef VM_H
#define VM_H

#include "common.h"
#include "parser.h"
#include <pthread.h>

// Value types for VM
typedef enum {
    VAL_BOOL,
    VAL_INT,
    VAL_FLOAT,
    VAL_STRING,
    VAL_MODULE,
    VAL_ARRAY,
    VAL_MAP,
    VAL_FUTURE,
    VAL_STRUCT_DEF,
    VAL_STRUCT_INSTANCE
} ValueType;

// Value structure for runtime values
typedef struct Module Module;
typedef struct Array Array;
typedef struct Map Map;
typedef struct Future Future;
typedef struct StructDef StructDef;
typedef struct StructInstance StructInstance;

// Value structure for runtime values
typedef struct {
    ValueType type;
    union {
        bool boolVal;
        int intVal;
        double floatVal;
        char* stringVal;
        Module* moduleVal; // This should remain Module* as per the original code, not ModuleEntry*
        Array* arrayVal;
        Map* mapVal;
        Future* futureVal;
        StructDef* structDef;
        StructInstance* structInstance;
    };
} Value;

// Forward declarations
typedef struct VarEntry VarEntry;
typedef struct FuncEntry FuncEntry;
typedef struct Function Function;
typedef struct Environment Environment;
typedef struct CallFrame CallFrame;
typedef struct VM VM;
typedef struct ModuleEntry ModuleEntry;

// Native function type for external C functions
typedef Value (*NativeFn)(VM*, Value* args, int argCount);

// Variable entry structure for hash table
struct VarEntry {
    char* key;              // Variable name
    Value value;            // Variable value
    struct VarEntry* next;  // Next entry in hash bucket
};

// Function entry structure for hash table
struct FuncEntry {
    char* key;              // Function name
    Function* function;     // Function definition
    struct FuncEntry* next; // Next entry in hash bucket
};

// Hash table size for environments
#define TABLE_SIZE 256

// String interning pool for performance optimization
typedef struct StringPool {
    char** strings;
    unsigned int* hashes;
    int count;
    int capacity;
} StringPool;

// Value pool for basic types to reduce malloc/free overhead
typedef struct ValuePool {
    Value* values;
    int* free_list;
    int next_free;
    int capacity;
} ValuePool;

// Environment structure (scope)
struct Environment {
    VarEntry* buckets[TABLE_SIZE];     // Variable hash table
    FuncEntry* funcBuckets[TABLE_SIZE]; // Function hash table
};

// Module wrapper
struct Module {
    char* name;
    Environment* env;
    // Keep the module source buffer alive for the lifetime of the module,
    // because AST Tokens point into this buffer.
    char* source;
};

// Minimal dynamic array implementation
struct Array {
    Value* items;
    int count;
    int capacity;
};

// Minimal map (hash table) implementation supporting string and int keys
typedef struct MapEntry MapEntry;
struct MapEntry {
    bool isIntKey;          // true: use intKey; false: use key (string)
    char* key;              // string key (owned)
    int intKey;             // int key
    Value value;            // stored value
    MapEntry* next;         // chaining in bucket
};

struct Map {
    MapEntry* buckets[TABLE_SIZE];
};

struct StructDef {
    char* name;
    char** fields; // Array of field names
    int fieldCount;
};

struct StructInstance {
    StructDef* def;
    Value* fields; // Array of values
};

// Module cache entry structure
struct ModuleEntry {
    char* key;                 // Module name (logical name from import, not alias)
    Module* module;            // Loaded module object
    struct ModuleEntry* next;  // Next entry in hash bucket
};

// Function structure
struct Function {
    Token name;             // Function name
    Token* params;          // Parameter names
    int paramCount;         // Number of parameters
    Node* body;             // Function body (AST node)
    Environment* closure;   // Closure environment (for lexical scoping)
    bool isNative;          // true if this is a native (C) function
    NativeFn native;        // pointer to native function (if isNative)
    bool isAsync;           // async function flag
};

// VM constants
#define STACK_MAX 256           // Maximum stack size
#define CALL_STACK_MAX 64       // Maximum call stack depth

// Call frame structure for function calls
struct CallFrame {
    Environment* env;       // Previous environment
    Value returnValue;      // Function return value
    bool hasReturned;       // Whether function has returned
};

// Virtual Machine structure
struct VM {
    Value stack[STACK_MAX];         // Value stack
    int stackTop;                   // Stack pointer
    Environment* env;               // Current environment
    Environment* globalEnv;         // Global environment
    Environment* defEnv;            // Target environment for function definitions
    CallFrame callStack[CALL_STACK_MAX]; // Call stack
    int callStackTop;               // Call stack pointer
    char projectRoot[1024];         // Project root directory for module search
    ModuleEntry* moduleBuckets[TABLE_SIZE]; // Module cache by name
    void* externHandles[TABLE_SIZE]; // Handles for dlopen() libraries
    int externHandleCount;          // Count of loaded extern libraries
    StringPool stringPool;          // String interning pool for performance
    ValuePool valuePool;            // Value pool for basic types
};

// VM function prototypes

/**
 * Initialize the virtual machine
 * @param vm Pointer to VM structure
 */
void initVM(VM* vm);

/**
 * Free all VM resources
 * @param vm Pointer to VM structure
 */
void freeVM(VM* vm);

/**
 * Interpret and execute AST
 * @param vm Pointer to VM structure
 * @param ast Root AST node to execute
 */
void interpret(VM* vm, Node* ast);

// Exposed internal API for external libraries to register native functions
void registerNativeFunction(VM* vm, const char* name, NativeFn function);

// Core Helpers exposed for corelib
unsigned int hash(const char* key, int length);
Map* newMap(void);
Array* newArray(void);
void mapSetStr(Map* m, const char* key, int len, Value v);
void mapSetInt(Map* m, int ikey, Value v);
MapEntry* mapFindEntry(Map* m, const char* skey, int slen, int* bucketOut);
void arrayPush(Array* a, Value v);
char* readFileAll(const char* path);

#endif // VM_H