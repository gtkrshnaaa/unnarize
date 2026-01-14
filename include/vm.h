#ifndef VM_H
#define VM_H

#include "common.h"
#include "parser.h"
#include <pthread.h>
#include <stdint.h>
#include <string.h> // For NanBoxing memcpy

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
    uint8_t generation;  // 0 = young (nursery), 1+ = old
    Obj* next;
};

// Value types for VM (kept for compatibility and helper)
typedef enum {
    VAL_BOOL,
    VAL_INT,
    VAL_FLOAT,
    VAL_OBJ,
    VAL_NIL
} ValueType;

// NanBoxing Implementation
// Value is 64-bit uint64_t
typedef uint64_t Value;

#define SIGN_BIT ((uint64_t)0x8000000000000000)
#define QNAN     ((uint64_t)0x7ffc000000000000)

// Helper inline to bitcast double <-> uint64_t
static inline double valueToNum(Value v) {
    double d;
    memcpy(&d, &v, sizeof(Value));
    return d;
}
static inline Value numToValue(double d) {
    Value v;
    memcpy(&v, &d, sizeof(Value));
    return v;
}

#define TAG_NIL   1
#define TAG_BOOL  2
#define TAG_INT   3
#define TAG_INT_BIT   ((uint64_t)0x0001000000000000)

#define IS_OBJ(v)     (((v) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT)) 
#define AS_OBJ(v)     ((Obj*)(uintptr_t)((v) & ~(SIGN_BIT | QNAN)))
#define OBJ_VAL(obj)  ((Value)(SIGN_BIT | QNAN | (uint64_t)(uintptr_t)(obj)))

#define IS_INT(v)     (((v) & (QNAN | TAG_INT_BIT)) == (QNAN | TAG_INT_BIT))
#define AS_INT(v)     ((int32_t)(v & 0xFFFFFFFF))
#define INT_VAL(num)  ((Value)(QNAN | TAG_INT_BIT | ((uint32_t)(num))))

// Internal Tagged Values
#define TAGGED_NIL    ((Value)(QNAN | 0x0002000000000000))
#define TAGGED_FALSE  ((Value)(QNAN | 0x0003000000000000))
#define TAGGED_TRUE   ((Value)(QNAN | 0x0003000000000001))

#define IS_NIL(v)     ((v) == TAGGED_NIL)
#define NIL_VAL       TAGGED_NIL

#define IS_BOOL(v)    (((v) & 0xFFFFFFFFFFFFFFFE) == TAGGED_FALSE)
#define AS_BOOL(v)    ((v) == TAGGED_TRUE)
#define BOOL_VAL(b)   ((b) ? TAGGED_TRUE : TAGGED_FALSE)

#define IS_NUMBER(v)  (((v) & QNAN) != QNAN)
#define IS_FLOAT(v)   IS_NUMBER(v)
#define AS_FLOAT(v)   valueToNum(v)
#define FLOAT_VAL(v)  numToValue(v)

void printValue(Value val);

// Helper for switch cases
static inline ValueType getValueType(Value v) {
    if (IS_INT(v)) return VAL_INT;
    if (IS_OBJ(v)) return VAL_OBJ;
    if (IS_BOOL(v)) return VAL_BOOL;
    if (IS_NIL(v)) return VAL_NIL;
    return VAL_FLOAT;
}

// Helpers
#define AS_STRING(value)  ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString*)AS_OBJ(value))->chars)

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

// Initial hash table size for environments (increased to next prime ~256)
#define TABLE_SIZE 1021

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
    char* modulePath; // Path of the module/file this function is defined in
    Environment* moduleEnv; // The global environment where this function was defined
};

// Forward declaration
struct BytecodeChunk;

// VM constants
#define STACK_MAX 65536           // Maximum stack size
#define CALL_STACK_MAX 1024       // Maximum call stack depth

// Call frame structure for function calls
struct CallFrame {
    Environment* env;       // Previous environment
    Environment* prevGlobalEnv; // Previous global environment
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



// Performance tracking structure
typedef struct {
    uint64_t instructionsExecuted;   // Total operations executed
    uint64_t loopsExecuted;          // Loop iterations
    uint64_t functionCalls;          // Function invocations
    uint64_t startTimeMicros;        // Execution start time
    uint64_t totalTimeMicros;        // Total execution time
} PerformanceCounters;

// Async Task for event loop
typedef struct {
    Future* future;         // The Future this task will resolve
    Function* func;         // Function to execute
    Value* args;           // Arguments array
    int argCount;          // Number of arguments
    bool started;          // Has execution started?
    bool completed;        // Is execution complete?
} Task;

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
    
    // GC State (Enhanced)
    Obj* objects;                   // Linked list of all objects (old gen)
    Obj* nursery;                   // Young generation objects
    int nurseryCount;               // Count of nursery objects
    int nurseryThreshold;           // Trigger minor GC when exceeded
    int grayCount;
    int grayCapacity;
    Obj** grayStack;
    size_t bytesAllocated;
    size_t nextGC;
    int gcPhase;                    // 0=idle, 1=marking, 2=sweeping
    
    // GC Statistics
    uint64_t gcCollectCount;        // Total GC runs
    uint64_t gcTotalPauseUs;        // Total pause time (microseconds)
    uint64_t gcLastPauseUs;         // Last GC pause time
    uint64_t gcTotalFreed;          // Total bytes freed
    size_t gcPeakMemory;            // Peak memory usage
    uint64_t gcLastCollectTime;     // Timestamp of last GC (for pacing)
    size_t gcBytesAllocSinceGC;     // Bytes allocated since last GC
    
    // CLI Arguments
    int argc;
    char** argv;
    
    // Async Task Queue
    Task* taskQueue;
    int taskCount;
    int taskCapacity;
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
bool collectGarbageIncremental(VM* vm, int workUnits);
void collectGarbageConcurrent(VM* vm, int workUnits);
bool isGCActive(void);
#define ALLOCATE_OBJ(vm, type, objectType) \
    (type*)allocateObject(vm, sizeof(type), objectType)

Obj* allocateObject(VM* vm, size_t size, ObjType type);
void freeObject(VM* vm, Obj* object);
void markObject(VM* vm, Obj* object);
void markValue(VM* vm, Value value);

// Write barrier: call when modifying object references during GC marking phase
// This ensures the tri-color invariant is maintained for incremental GC
#define WRITE_BARRIER(vm, obj) do { \
    if ((vm)->gcPhase == 1 && (obj) != NULL && !((Obj*)(obj))->isMarked) { \
        markObject((vm), (Obj*)(obj)); \
    } \
} while(0)

// Write barrier for Values containing objects
#define WRITE_BARRIER_VALUE(vm, val) do { \
    if ((vm)->gcPhase == 1 && IS_OBJ(val)) { \
        Obj* _obj = AS_OBJ(val); \
        if (_obj != NULL && !_obj->isMarked) { \
            markObject((vm), _obj); \
        } \
    } \
} while(0)

// Register core built-in functions (has, keys, len, etc.)
// String concatenation helper (exposed for VM)
Value vm_concatenate(VM* vm, Value a, Value b);

void registerBuiltins(VM* vm);

#endif // VM_H