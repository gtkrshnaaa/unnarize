#ifndef VM_H
#define VM_H

#include "common.h"
#include "parser.h"
#include <pthread.h>
#include <stdint.h>

// Object types
typedef enum {
    OBJ_STRING,
    OBJ_MODULE,
    OBJ_ARRAY,
    OBJ_MAP,
    OBJ_STRUCT_DEF,
    OBJ_STRUCT_INSTANCE,
    OBJ_RESOURCE,
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_FUTURE,
    OBJ_UPVALUE,
    OBJ_ENVIRONMENT
} ObjType;

typedef struct Obj Obj;

struct Obj {
    ObjType type;
    bool isMarked;
    Obj* next;
};

// Value types for VM
typedef enum {
    VAL_BOOL,
    VAL_INT,
    VAL_FLOAT,
    VAL_OBJ,
    VAL_NIL
} ValueType;

// Value structure for runtime values
typedef struct {
    ValueType type;
    union {
        bool boolVal;
        int64_t intVal;
        double floatVal;
        Obj* obj;
    };
} Value;

void printValue(Value val);

// Helpers
#define AS_OBJ(value)     ((value).obj)
#define AS_STRING(value)  ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString*)AS_OBJ(value))->chars)
#define AS_INT(value)     ((value).intVal)
#define AS_FLOAT(value)   ((value).floatVal)
#define AS_BOOL(value)    ((value).boolVal)

#define INT_VAL(value)    ((Value){VAL_INT, .intVal = (value)})
#define FLOAT_VAL(value)  ((Value){VAL_FLOAT, .floatVal = (value)})
#define BOOL_VAL(value)   ((Value){VAL_BOOL, .boolVal = (value)})
#define NIL_VAL           ((Value){VAL_NIL, .intVal = 0})
#define OBJ_VAL(object)   ((Value){VAL_OBJ, .obj = (Obj*)(object)})

#define IS_OBJ(value)     ((value).type == VAL_OBJ)
#define IS_INT(value)     ((value).type == VAL_INT)
#define IS_FLOAT(value)   ((value).type == VAL_FLOAT)
#define IS_BOOL(value)    ((value).type == VAL_BOOL)
#define IS_NIL(value)     ((value).type == VAL_NIL)

#define IS_STRING(value)  (IS_OBJ(value) && AS_OBJ(value)->type == OBJ_STRING)
#define IS_ARRAY(value)   (IS_OBJ(value) && AS_OBJ(value)->type == OBJ_ARRAY)
#define IS_MAP(value)     (IS_OBJ(value) && AS_OBJ(value)->type == OBJ_MAP)

typedef struct ObjString {
    Obj obj;
    char* chars;
    int length;
    unsigned int hash;
} ObjString;

// Forward declarations
typedef struct VarEntry VarEntry;
typedef struct FuncEntry FuncEntry;
typedef struct Function Function;
typedef struct Environment Environment;
typedef struct CallFrame CallFrame;
typedef struct VM VM;
typedef struct ModuleEntry ModuleEntry;

// Typedefs for object structs
typedef struct Module Module;
typedef struct Array Array;
typedef struct MapEntry MapEntry;
typedef struct Map Map;
typedef struct StructDef StructDef;
typedef struct StructInstance StructInstance;
typedef struct Future Future;

// Native function type for external C functions
typedef Value (*NativeFn)(VM*, Value* args, int argCount);

// Variable entry structure for hash table
struct VarEntry {
    char* key;              // Variable name
    int keyLength;          // Length of key
    bool ownsKey; 
    Value value;            // Variable value
    struct VarEntry* next;  // Next entry in hash bucket
};

// Function entry structure for hash table
struct FuncEntry {
    char* key;              // Function name
    Function* function;     // Function definition
    struct FuncEntry* next; // Next entry in hash bucket
};

// Initial hash table size for environments
#define TABLE_SIZE 61

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
    Obj obj;                           // Header for GC
    struct Environment* enclosing;     // Parent environment
    VarEntry* buckets[TABLE_SIZE];     // Variable hash table
    FuncEntry* funcBuckets[TABLE_SIZE]; // Function hash table
};

// Module wrapper
struct Module {
    Obj obj;
    char* name;
    Environment* env;
    char* source;
};

// Minimal dynamic array implementation
struct Array {
    Obj obj;
    Value* items;
    int count;
    int capacity;
};

// Map entry
struct MapEntry {
    bool isIntKey;
    char* key; // Or ObjString*? For now char* interned
    int intKey;
    Value value;
    MapEntry* next;
};

struct Map {
    Obj obj;
    MapEntry* buckets[TABLE_SIZE];
};

struct StructDef {
    Obj obj;
    char* name;
    char** fields;
    int fieldCount;
};

struct StructInstance {
    Obj obj;
    StructDef* def;
    Value* fields;
};

struct Future {
    Obj obj;
    bool done;              // completion flag
    Value result;           // resolved value
    pthread_mutex_t mu;     // mutex
    pthread_cond_t cv;      // condition variable
};

typedef void (*ResourceCleanupFn)(void* data);
typedef struct {
    Obj obj;
    void* data;
    ResourceCleanupFn cleanup;
} ObjResource;

// Function structure
struct Function {
    Obj obj; // Function is an object now
    Token name;
    Token* params;
    int paramCount;
    Node* body;
    Environment* closure;
    bool isNative;
    NativeFn native;
    bool isAsync;
    struct BytecodeChunk* bytecodeChunk; // Bytecode for this function
};

// Forward declaration
struct BytecodeChunk;

// VM constants
#define STACK_MAX 65536           // Maximum stack size
#define CALL_STACK_MAX 64       // Maximum call stack depth

// Call frame structure for function calls
struct CallFrame {
    Environment* env;       // Previous environment
    int fp;                 // Previous frame pointer (stack index)
    Value returnValue;      // Function return value
    bool hasReturned;       // Whether function has returned
    
    // Bytecode support
    uint8_t* ip;            // Return address (caller's IP)
    struct BytecodeChunk* chunk; // Caller's chunk
    struct Function* function;   // Executing function (GC Root)
};

// Module cache entry
struct ModuleEntry {
    char* name;
    int nameLen;
    Module* module;
    struct ModuleEntry* next;
};

// Execution mode for performance optimization
typedef enum {
    EXEC_MODE_INTERPRETED,  // Standard tree-walking (current)
    EXEC_MODE_OPTIMIZED     // JIT-compiled (future)
} ExecutionMode;

// Performance tracking structure
typedef struct {
    uint64_t instructionsExecuted;   // Total operations executed
    uint64_t loopsExecuted;          // Loop iterations
    uint64_t functionCalls;          // Function invocations
    uint64_t startTimeMicros;        // Execution start time
    uint64_t totalTimeMicros;        // Total execution time
} PerformanceCounters;

// Virtual Machine structure
struct VM {
    Value stack[STACK_MAX];         // Value stack
    int stackTop;                   // Stack pointer
    int fp;                         // Current frame pointer
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
    // GC State
    Obj* objects;                   // Linked list of all objects
    int grayCount;
    int grayCapacity;
    Obj** grayStack;
    size_t bytesAllocated;
    size_t nextGC;
    
    // CLI Arguments
    int argc;
    char** argv;
    
    // JIT Infrastructure (Phase 2 - Full Native JIT)
    ExecutionMode execMode;              // Current execution mode
    PerformanceCounters perfCounters;    // Performance tracking
    bool enableOptimizations;            // Optimization flag
    
    // JIT Compilation State
    bool jitEnabled;                     // Enable JIT compilation
    int jitThreshold;                    // Hotspot threshold (iterations before compile)
    void** jitCache;                     // Cache of compiled JITFunction pointers
    int jitCacheSize;                    // Number of cached functions
    int jitCacheCapacity;                // Capacity of JIT cache
    uint64_t jitCompilations;            // Stats: number of compilations
    uint64_t jitExecutions;              // Stats: number of JIT executions
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
Map* newMap(VM* vm);
Array* newArray(VM* vm);
void mapSetStr(Map* m, const char* key, int len, Value v);
void mapSetInt(Map* m, int ikey, Value v);
MapEntry* mapFindEntry(Map* m, const char* skey, int slen, int* bucketOut);
MapEntry* mapFindEntryInt(Map* m, int ikey, int* bucketOut);
void arrayPush(VM* vm, Array* a, Value v);
bool arrayPop(Array* array, Value* value);
char* readFileAll(const char* path);
Value callFunction(VM* vm, Function* func, Value* args, int argCount);
Function* findFunctionByName(VM* vm, const char* name);
void defineGlobal(VM* vm, const char* name, Value value);
void defineNative(VM* vm, Environment* env, const char* name, NativeFn fn, int arity);
ObjString* internString(VM* vm, const char* str, int length);

// Memory Management
void* reallocate(VM* vm, void* pointer, size_t oldSize, size_t newSize);
void collectGarbage(VM* vm);
#define ALLOCATE_OBJ(vm, type, objectType) \
    (type*)allocateObject(vm, sizeof(type), objectType)

Obj* allocateObject(VM* vm, size_t size, ObjType type);
void freeObject(VM* vm, Obj* object);

// Register core built-in functions (has, keys, len, etc.)
void registerBuiltins(VM* vm);

#endif // VM_H