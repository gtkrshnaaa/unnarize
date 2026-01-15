# VM Architecture

> Internal structure of the Unnarize Virtual Machine.

---

## Execution Pipeline

```
Source Code (.unna)
       │
       ▼
   ┌───────┐
   │ Lexer │  ──── Tokenization
   └───┬───┘
       │
       ▼
   ┌────────┐
   │ Parser │  ──── AST Construction
   └───┬────┘
       │
       ▼
   ┌──────────┐
   │ Compiler │  ──── Bytecode Generation
   └────┬─────┘
       │
       ▼
   ┌──────────────┐
   │ Interpreter  │  ──── Bytecode Execution
   └──────┬───────┘
          │
    ┌─────┴─────┐
    │           │
    ▼           ▼
┌──────┐   ┌──────────┐
│  GC  │   │ Corelib  │
└──────┘   └──────────┘
```

---

## Source Files

| File | Lines | Purpose |
|------|-------|---------|
| `src/lexer.c` | 211 | Tokenizer |
| `src/parser.c` | 625 | AST builder |
| `src/bytecode/compiler.c` | 880 | AST → Bytecode |
| `src/bytecode/interpreter.c` | ~1400 | Bytecode execution |
| `src/vm.c` | 1853 | VM runtime |
| `src/gc.c` | 661 | Garbage collector |

---

## Lexer

The lexer (`lexer.c`) converts source code into tokens.

### Token Types

Keywords:
- `var`, `function`, `if`, `else`, `while`, `for`, `return`
- `struct`, `import`, `as`, `print`
- `async`, `await`, `true`, `false`, `nil`

Operators:
- Arithmetic: `+`, `-`, `*`, `/`, `%`
- Comparison: `==`, `!=`, `<`, `>`, `<=`, `>=`
- Logical: `&&`, `||`, `!`
- Assignment: `=`, `+=`, `-=`, `*=`, `/=`

Delimiters:
- `(`, `)`, `{`, `}`, `[`, `]`
- `;`, `,`, `.`, `:`

### Keyword Detection

Uses first-character switch with suffix matching:

```c
switch (*lexer->start) {
    case 'i': // if, import
    case 'f': // for, function, false
    case 'a': // as, async, await
    // ...
}
```

---

## Parser

The parser (`parser.c`) uses recursive descent to build an AST.

### Expression Precedence (Low to High)

1. Assignment (`=`, `+=`, etc.)
2. Logical OR (`||`)
3. Logical AND (`&&`)
4. Equality (`==`, `!=`)
5. Comparison (`<`, `>`, `<=`, `>=`)
6. Term (`+`, `-`)
7. Factor (`*`, `/`, `%`)
8. Unary (`-`, `!`, `await`)
9. Primary (literals, identifiers, calls)

### AST Node Types

```c
typedef enum {
    // Expressions
    NODE_EXPR_LITERAL,
    NODE_EXPR_VAR,
    NODE_EXPR_BINARY,
    NODE_EXPR_UNARY,
    NODE_EXPR_CALL,
    NODE_EXPR_GET,
    NODE_EXPR_INDEX,
    NODE_EXPR_ARRAY_LITERAL,
    NODE_EXPR_AWAIT,
    
    // Statements
    NODE_STMT_PRINT,
    NODE_STMT_VAR_DECL,
    NODE_STMT_ASSIGN,
    NODE_STMT_IF,
    NODE_STMT_WHILE,
    NODE_STMT_FOR,
    NODE_STMT_FOREACH,
    NODE_STMT_FUNCTION,
    NODE_STMT_RETURN,
    NODE_STMT_BLOCK,
    NODE_STMT_STRUCT_DECL,
    NODE_STMT_IMPORT,
    // ...
} NodeType;
```

---

## Bytecode Compiler

The compiler (`compiler.c`) transforms AST nodes into bytecode.

### Compilation Process

1. **Local Variable Resolution** - Track locals by slot number
2. **Expression Compilation** - Generate value-producing code
3. **Statement Compilation** - Generate control flow
4. **Jump Patching** - Fix forward jump addresses

### Local Variables

```c
struct {
    const char* name;
    int depth;      // Scope depth
    int slot;       // Stack slot
} locals[256];
```

---

## Interpreter

The interpreter (`interpreter.c`) executes bytecode using computed goto dispatch.

### Dispatch Table

```c
static void* dispatchTable[] = {
    &&op_load_imm,
    &&op_load_const,
    &&op_load_nil,
    // ... ~100 entries
};

#define DISPATCH() goto *dispatchTable[READ_BYTE()]
```

### Stack Machine

```
Stack Layout:
┌─────────────┐ ← stackTop
│   Value N   │
├─────────────┤
│   Value 2   │
├─────────────┤
│   Value 1   │
├─────────────┤
│   Locals    │ ← fp (frame pointer)
└─────────────┘
```

### Call Frame

```c
struct CallFrame {
    Environment* env;        // Scope
    int fp;                  // Frame pointer
    uint8_t* ip;            // Return address
    BytecodeChunk* chunk;   // Caller's code
    Function* function;     // Current function
};
```

---

## Virtual Machine State

```c
struct VM {
    // Stack
    Value stack[65536];
    int stackTop;
    int fp;
    
    // Call Stack
    CallFrame callStack[1024];
    int callStackTop;
    
    // Environments
    Environment* env;
    Environment* globalEnv;
    
    // GC State
    Obj* objects;      // Old generation
    Obj* nursery;      // Young generation
    int nurseryCount;
    Obj** grayStack;   // Tri-color marking
    
    // Other
    StringPool stringPool;  // String interning
    ModuleEntry* moduleBuckets[1021];  // Module cache
};
```

---

## Memory Management

### Object Types

```c
typedef enum {
    OBJ_STRING,
    OBJ_ARRAY,
    OBJ_MAP,
    OBJ_STRUCT_DEF,
    OBJ_STRUCT_INSTANCE,
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_FUTURE,
    OBJ_ENVIRONMENT
} ObjType;
```

### Object Header

```c
struct Obj {
    ObjType type;
    bool isMarked;      // GC mark
    bool isPermanent;   // Never collect
    uint8_t generation; // 0=young, 1+=old
    Obj* next;          // Linked list
};
```

---

## String Interning

All strings are interned for:
- Fast equality comparison (pointer comparison)
- Memory efficiency (no duplicates)
- Hash table optimization

```c
ObjString* internString(VM* vm, const char* str, int length) {
    unsigned int hash = hashString(str, length);
    // Check pool for existing string
    // If not found, create and add to pool
    return string;
}
```

---

## Native Functions

C functions exposed to Unnarize:

```c
typedef Value (*NativeFn)(VM*, Value* args, int argCount);

void registerNativeFunction(VM* vm, const char* name, NativeFn fn) {
    // Add to global environment
}
```

### Core Library Registration

```c
void registerUCoreTimer(VM* vm);
void registerUCoreUON(VM* vm);
void registerUCoreScraper(VM* vm);
void registerBuiltins(VM* vm);  // push, pop, length, etc.
```

---

## Async Implementation

### Future Structure

```c
struct Future {
    Obj obj;
    bool done;
    Value result;
    pthread_mutex_t mu;
    pthread_cond_t cv;
};
```

### Await Semantics

1. Check if Future is done
2. If not, block on condition variable
3. Return result when complete

---

## Performance Optimizations

| Optimization | Impact |
|--------------|--------|
| Computed Goto | 2-3x faster dispatch |
| NaN Boxing | No heap allocation for primitives |
| String Interning | O(1) string equality |
| Specialized Opcodes | Skip type checks |
| Generational GC | Minimal pause times |

---

## Next Steps

- [Bytecode](bytecode.md) - Complete opcode reference
- [Garbage Collection](garbage-collection.md) - GC algorithm details
- [NaN Boxing](nan-boxing.md) - Value representation
