# Unnarize Language Interpreter

![C](https://img.shields.io/badge/language-C-blue.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)
![Performance](https://img.shields.io/badge/performance-4M%20ops%2Fsec-brightgreen.svg)


**A simple, dynamic, C-style scripting language interpreter built from scratch in C.**

### Table of Contents

  * [About The Project](https://github.com/gtkrshnaaa/unnarize?tab=readme-ov-file#about-the-project)
  * [Features](https://github.com/gtkrshnaaa/unnarize?tab=readme-ov-file#features)
  * [Performance Benchmarks](https://github.com/gtkrshnaaa/unnarize?tab=readme-ov-file#performance-benchmarks)
  * [Getting Started](https://github.com/gtkrshnaaa/unnarize?tab=readme-ov-file#getting-started)
      * [Prerequisites](https://github.com/gtkrshnaaa/unnarize?tab=readme-ov-file#prerequisites)
      * [Build Instructions](https://github.com/gtkrshnaaa/unnarize?tab=readme-ov-file#build-instructions)
      * [Installation](https://github.com/gtkrshnaaa/unnarize?tab=readme-ov-file#installation)
  * [Usage](https://github.com/gtkrshnaaa/unnarize?tab=readme-ov-file#usage)
  * [Plugin System](https://github.com/gtkrshnaaa/unnarize?tab=readme-ov-file#plugin-system)
  * [Language Syntax Showcase](https://github.com/gtkrshnaaa/unnarize?tab=readme-ov-file#language-syntax-showcase)
  * [Interpreter Architecture](https://github.com/gtkrshnaaa/unnarize?tab=readme-ov-file#interpreter-architecture)
  * [Project Structure](https://github.com/gtkrshnaaa/unnarize?tab=readme-ov-file#project-structure)
  * [Complex Code Demonstration](https://github.com/gtkrshnaaa/unnarize?tab=readme-ov-file#complex-code-unnarize-language-interpreter-can-run)
  * [Author](https://github.com/gtkrshnaaa/unnarize?tab=readme-ov-file#author)

-----

## About The Project

Unnarize is a high-performance tree-walk interpreter for a dynamic, C-style scripting language, written entirely in C with zero external dependencies. The project demonstrates advanced interpreter design with optimized hash functions (FNV-1a), value pooling, and string interning infrastructure for maximum performance.

Every component, from the lexical analyzer (Lexer) to the Abstract Syntax Tree (AST) parser and the Virtual Machine (VM) that executes the code, has been built from the ground up. The interpreter achieves **4+ million operations per second** while maintaining clean architecture and comprehensive feature support.

### Core Philosophy
**"Do something with one way, but perfect"** - Focus on efficiency, predictability, and elegant simplicity.

## Features

This section details the core features of the Unnarize programming language.

**General**

  - **C-style Syntax:** An intuitive and familiar syntax for developers experienced with C, Java, JavaScript, or similar languages.
  - **Dynamic Typing:** Variables are not bound to a specific type. The interpreter handles type management at runtime for Integers, Floats, Strings, and Booleans (derived from comparisons).
  - **Single-Line Comments:** Use `//` to add comments to your code, which will be ignored by the interpreter.

**Variables and Scope**

  - **Variable Declaration:** Declare variables using the `var` keyword (e.g., `var x = 10;`).
  - **Reassignment:** Variables can be reassigned to new values of any type after declaration.
  - **Lexical Scoping:** The language supports both global scope and local (function-level) scope. Variables declared inside a function are not accessible from the outside.
  - **Block Statements:** Use curly braces `{ ... }` to group multiple statements into a single block, creating a new scope context for control flow structures.

**Operators**

  - **Full Arithmetic Operators:** Includes addition (`+`), subtraction (`-`), multiplication (`*`), division (`/`), and modulo (`%`).
  - **Unary Negation:** Supports the unary minus operator to negate numeric values (e.g., `-x`).
  - **Complete Comparison Operators:** Provides `==` (equal), `!=` (not equal), `>` (greater than), `<` (less than), `>=` (greater or equal), and `<=` (less or equal).
  - **String Concatenation:** The `+` operator is overloaded to concatenate strings.
  - **Automatic Type Coercion:** Non-string types are automatically converted to strings during concatenation operations (e.g., `"Age: " + 25`).

**Functions**

  - **Function Definition:** Declare functions using the `function` keyword, followed by a name, parameters, and a body.
  - **Parameters and Arguments:** Functions can accept multiple arguments, which are passed by value.
  - **Return Values:** Functions can return a value using the `return` statement. If no value is returned, a default of `0` (integer) is provided.
  - **Recursion:** The call stack architecture fully supports recursive function calls, allowing for elegant solutions to problems like factorial or Fibonacci sequences.

**Async/Await and Futures**

  - **Async Functions:** Declare with `async function name(...) { ... }` to return a Future immediately.
  - **Await:** Use `await <future>` to synchronously obtain the result when needed.
  - **Truthiness Rule:** Futures are not allowed as conditions in `if`, `while`, or `for`. Use `await` to get a concrete value first.
  - **String Concatenation:** Printing or concatenating a Future renders a compact descriptor like `[future pending]` or `[future done]`.
  - **Equality:** `==`/`!=` for Futures compares identity (same underlying future).

**Modules and Imports**

  - **Module Import:** Import modules using `import <name> as <alias>;`.
  - **Module Cache:** Repeated imports of the same logical module are cached to avoid re-parsing.
  - **UNNARIZE_PATH Resolution:** Set `UNNARIZE_PATH` (colon-separated list of directories) to prioritize module lookup before falling back to the project root.
  - **Examples:** See `examples/modularity/` and run `make modularity-run`.

**Arrays and Maps**

  - **Dynamic Arrays:** Create with `array()`, index with `arr[i]`, and assign with `arr[i] = value`.
  - **Hash Maps:** Create with `map()`, key access via `m[key]` and `m[key] = value` for set/update.
  - **Built-ins:**
    - Arrays: `push(arr, value)`, `pop(arr)`, `length(arr)`
    - Maps: `has(map, key)`, `delete(map, key)`, `keys(map)`, `length(map)`
  - **Truthiness:** Arrays/Maps are truthy when non-empty (length > 0).
  - **Equality:** `==`/`!=` use identity (pointer), not deep equality.
  - **String Concatenation:** Concatenation renders compact descriptors, e.g., array as `[array length=N]` and map as `{map size=N}`.
  - **Futures Rendering:** Similarly, a Future renders as `[future pending]` or `[future done]` when printed or concatenated.

**Plugin System (loadextern)**

  - **Dynamic Loading:** Load native C libraries at runtime using `loadextern("plugin.so")`.
  - **Self-Contained Plugins:** Minimal plugin interface with automatic function registration.
  - **Performance Extensions:** Write performance-critical code in C while keeping logic in Unnarize.
  - **Built-in Plugins:** Math operations, string manipulation, timing functions, and benchmarking tools.

## Performance Benchmarks

Unnarize delivers exceptional performance for a tree-walking interpreter:

| Operation Type | Performance | Description |
|---|---|---|
| **Simple Loops** | 4.8M ops/sec | Basic counting and iteration |
| **Arithmetic** | 4.2M ops/sec | Mathematical operations |
| **Variable Assignment** | 3.7M ops/sec | Variable manipulation |
| **Nested Loops** | 3.2M ops/sec | Complex iteration patterns |
| **String Operations** | 10M ops/sec | String concatenation and manipulation |
| **Function Calls** | 1.1M ops/sec | Function invocation overhead |

**Overall Average: 3.9 million operations per second**

### Performance Features

- **FNV-1a Hash Function:** Optimized hash table lookups (15-20% faster)
- **Value Pooling:** Reduced memory allocation overhead
- **String Interning Infrastructure:** Ready for string deduplication
- **Optimized Memory Management:** Manual allocation with efficient reuse patterns

*Benchmark with timer plugin: `make run-timer-benchmark`*

## Getting Started

Follow these instructions to get a local copy up and running.

### Prerequisites

Ensure you have a standard C development environment. This project relies on:

  * `gcc` (GNU Compiler Collection)
  * `make`
  * `git`

The project has been tested on Linux-based systems.

### Build Instructions

1.  **Clone the repository:**

    ```sh
    git clone https://github.com/gtkrshnaaa/unnarize.git
    ```

2.  **Navigate to the project directory:**

    ```sh
    cd unnarize
    ```

3.  **Compile the project using the Makefile:**

    ```sh
    make all
    ```

    This command will compile all source files, placing object files in the `obj/` directory and the final `unnarize` executable in the `bin/` directory.

### Installation

Install Unnarize system-wide (requires root privileges):

```sh
make install
```

This will install the `unnarize` binary to `/usr/local/bin/` and make it available globally.

**Uninstall:**
```sh
make uninstall
```

## Usage

The interpreter is a command-line application that takes the path to a Unnarize script file as an argument.

**General Syntax:**

```sh
./bin/unnarize [path_to_script.unna]
```

**Example:**
To run the comprehensive demonstration script included in the repository, you can use the `run` target in the Makefile for convenience:

```sh
make run
```

This is equivalent to running:

```sh
./bin/unnarize examples/test.unna
```

**Modularity Example:**

Run the modularity demo:

```sh
make modularity-run
```

Optionally, specify additional module search paths via `UNNARIZE_PATH` (colon-separated):

```sh
export UNNARIZE_PATH=/path/to/modules:/another/path
make modularity-run
```

**Arrays/Maps Example:**

Run the arrays/maps demo:

```sh
make arrays-maps-run
```
Script: `examples/arrays_maps.unna`

**Async/Await Example:**

Run the async/await demo:

```sh
make async-demo-run
```
Script: `examples/async_demo.unna`

**Run All Examples:**

Execute all `.unna` files under `examples/` (recursively) in a sorted order. This automatically builds all plugins first:

```sh
make run-all
```

**Performance Benchmark:**

Run the comprehensive performance benchmark suite:

```sh
make run-timer-benchmark
```

## Plugin System

Unnarize features a powerful plugin system that allows you to extend the language with native C functions.

### Available Plugins

| Plugin | Functions | Description |
|---|---|---|
| **mathLib** | `mathPower(base, exp)` | Mathematical operations |
| **stringLib** | `stringUpper(str)`, `stringLength(str)` | String manipulation |
| **timeLib** | `timeNow()`, `timeSleep(ms)` | Time and sleep functions |
| **timerLib** | `timerStart()`, `timerElapsed()`, `timerElapsedMicros()` | High-precision benchmarking |

### Quick Start

```bash
# Build all plugins
make build-plugins-all

# Run plugin demos
make plugins-demo-run

# Run performance benchmark
make run-timer-benchmark
```

### Plugin Development

`loadextern(path)` dynamically loads a shared library (.so) and registers native functions:

```c
// Minimal plugin example
#include <dlfcn.h>
typedef struct VM VM;
typedef enum { VAL_INT, VAL_FLOAT, VAL_STRING, VAL_BOOL, VAL_MODULE, VAL_ARRAY, VAL_MAP } ValueType;
typedef struct { ValueType type; union { int intVal; double floatVal; char* stringVal; int bool_storage; void* moduleVal; void* arrayVal; void* mapVal; }; } Value;
typedef Value (*NativeFn)(VM*, Value* args, int argc);
typedef void (*RegisterFn)(VM*, const char* name, NativeFn fn);

static Value add1(VM* vm, Value* args, int argc) {
  (void)vm; Value v; v.type = VAL_INT; v.intVal = 0;
  if (argc == 1 && args[0].type == VAL_INT) { v.intVal = args[0].intVal + 1; }
  return v;
}

void registerFunctions(VM* vm) {
  RegisterFn reg = (RegisterFn)dlsym(RTLD_DEFAULT, "registerNativeFunction");
  if (!reg) return;
  reg(vm, "add1", &add1);
}
```

**Build:**
```sh
gcc -shared -fPIC -o plugin.so plugin.c -ldl
```

**Use:**
```c
loadextern("plugin.so");
print(add1(41)); // 42
```

## Language Syntax Showcase

Here are some code snippets demonstrating key Unnarize language features.

#### Variables and Expressions

```c
// Declare a variable and print its value.
var message = "Hello from Unnarize!";
print(message);

// Variables can be reassigned.
var score = 90;
score = score + 10; // score is now 100
print("Final score: " + score); // Automatic type coercion
```

#### Functions and Recursion

```c
// A function to calculate the factorial of a number.
function factorial(n) {
    // Base case for the recursion.
    if (n <= 1) {
        return 1;
    }
    // Recursive step.
    return n * factorial(n - 1);
}

var result = factorial(6);
print("Factorial of 6 is: " + result); // Prints: 720
```

#### Scope Management

```c
var globalVar = "I am global.";

function testScope() {
    var localVar = "I am local.";
    print("Inside function: " + globalVar); // Can access global
    print("Inside function: " + localVar);
}

testScope();
print("Outside function: " + globalVar);
// The following line would cause an error, as localVar is out of scope.
// print(localVar); 
```

#### Async/Await (Quick Snippet)

```c
// Minimal async/await example
async function computeSum(n) {
    var i = 0;
    var s = 0;
    while (i < n) { s = s + i; i = i + 1; }
    return s;
}

async function orchestrate() {
    // Launch tasks in parallel
    var f1 = computeSum(10000);
    var f2 = computeSum(20000);
    // Await results when needed
    var a = await f1;
    var b = await f2;
    print("Combined: " + (a + b));
}

print("-- Async quick demo --");
print(await computeSum(10)); // inline await
await orchestrate();

// Note: Futures cannot be used directly as conditions in if/while/for.
// Always await to get a concrete value first.
```

## Interpreter Architecture

The Unnarize interpreter follows a classic three-stage pipeline:

`Source Code (.unna)` -> `[ Lexer ]` -> `Tokens` -> `[ Parser ]` -> `AST` -> `[ Interpreter ]` -> `Output`

1.  **Lexer (Scanner) - `lexer.c`**
    This stage performs lexical analysis, breaking down the raw source code into a stream of fundamental units called **tokens**.

2.  **Parser - `parser.c`**
    The parser consumes the token stream and constructs a tree-like data structure known as an **Abstract Syntax Tree (AST)**. The AST represents the hierarchical structure and logical flow of the code.

3.  **Interpreter (VM) - `vm.c`**
    This is the execution engine. It is a **tree-walking interpreter** that recursively traverses the AST. At each node, it evaluates expressions, executes statements, manages variable environments (scopes), and handles the function call stack.

## Project Structure

```
.
├── bin/                       # built binary artifacts
│   └── unnarize                 # Unnarize interpreter executable
├── examples/                  # language examples and demos
│   ├── arraysMaps.unna          # Arrays and maps demonstration
│   ├── asyncDemo.unna           # Async/await and futures
│   ├── test.unna                # Comprehensive language feature demo
│   ├── modularity/              # Module system examples
│   │   ├── main.unna
│   │   └── utils/
│   │       ├── geometry.unna
│   │       └── stats.unna
│   ├── plugins/                 # Plugin system demos
│   │   ├── build/               # Generated .so files (auto-built)
│   │   │   ├── mathLib.so
│   │   │   ├── stringLib.so
│   │   │   ├── timeLib.so
│   │   │   └── timerLib.so
│   │   ├── src/                 # C plugin sources
│   │   │   ├── mathLib.c
│   │   │   ├── stringLib.c
│   │   │   ├── timeLib.c
│   │   │   └── timerLib.c       # Performance benchmarking
│   │   ├── demoMathLib.unna
│   │   ├── demoStringLib.unna
│   │   ├── demoTimeLib.unna
│   │   └── demoTimerLib.unna    # Performance benchmark suite
│   └── projectTest/             # Real-world project simulation
│       ├── main.unna
│       └── utils/
│           ├── inventory.unna
│           ├── math.unna
│           └── report.unna
├── include/                     # public headers
│   ├── common.h
│   ├── extern.h                 # Plugin system interface
│   ├── lexer.h
│   ├── parser.h
│   └── vm.h                     # VM with performance optimizations
├── src/                         # interpreter sources (~2500 LOC)
│   ├── lexer.c                  # Lexical analysis
│   ├── main.c                   # Entry point
│   ├── parser.c                 # AST parser
│   └── vm.c                     # Optimized virtual machine
├── obj/                         # intermediate object files
├── Makefile                     # comprehensive build system
├── README.md                    # documentation
├── COREPHILOSOPHY.md           # design principles
└── LICENSE
```


### Key Features:

- **Performance Optimized VM:** FNV-1a hashing, value pooling, string interning infrastructure
- **Comprehensive Plugin System:** 4 built-in plugins with timer-based benchmarking
- **Advanced Examples:** Async/await, module system, real-world project simulation
- **Zero Dependencies:** Pure C implementation with no external libraries
- **Development Ready:** Comprehensive Makefile with auto-building plugins


## Complex Code Unnarize Language Interpreter Can Run

### Project Codebase

`examples/projectTest/main.unna`

```c
// Real-world style demo project for Unnarize
// This project showcases: variables, control flow, functions, recursion,
// arrays, maps, and modularity via imports.

import inventory as inv;
import report as rpt;
import math as m;

print("=== PROJECT TEST: STORE INVENTORY DASHBOARD ===");
print("");

// Basic variables and expressions
var storeName = "Unnarize Mart";
var openHour = 9;
var closeHour = 21;
print("Store: " + storeName);
print("Open hours: " + openHour + " to " + closeHour);
print("");

// Build initial inventory using a map
var stock = inv.createInventory();
inv.addItem(stock, "apple", 10);
inv.addItem(stock, "banana", 5);
inv.addItem(stock, "chocolate", 12);

// Arrays: incoming shipments
var shipments = array();
push(shipments, 3); // apples
push(shipments, 5); // bananas
push(shipments, 2); // chocolates

// Update stock from shipments using a loop
var items = inv.itemKeys(); // ["apple", "banana", "chocolate"]
var idx = 0;
while (idx < length(items)) {
    var name = items[idx];
    var qty = shipments[idx];
    inv.addItem(stock, name, qty);
    idx = idx + 1;
}
print("");

// Use functions and conditionals
var total = inv.totalItems(stock);
if (total >= 30) {
    print("Stock level: healthy");
} else {
    print("Stock level: low");
}
print("");

// Use math module utilities
var dailySales = array();
push(dailySales, 7);
push(dailySales, 11);
push(dailySales, 5);
push(dailySales, 9);

print("Sales sum: " + m.sum(dailySales));
print("Sales avg: " + m.avg(dailySales));
print("");

// Recursion demo
print("factorial(5) = " + m.factorial(5));
print("");

// Reporting via separate module
rpt.printSummary(storeName, stock);
print("");

// Small analytics: build a frequency map from a sales array
print("-- Sales frequency (units sold per value) --");
var freq = inv.frequency(dailySales);
var fkeys = keys(freq);
var i = 0;
while (i < length(fkeys)) {
    var k = fkeys[i];
    print(k + " -> " + freq[k]);
    i = i + 1;
}
print("");

print("=== END OF PROJECT TEST ===");

```

`examples/projectTest/utils/inventory.unna`

```c
// Inventory utilities module

function createInventory() {
    return map();
}

function addItem(inv, name, qty) {
    if (has(inv, name)) {
        inv[name] = inv[name] + qty;
    } else {
        inv[name] = qty;
    }
    return inv[name];
}

function totalItems(inv) {
    var ks = keys(inv);
    var i = 0;
    var total = 0;
    while (i < length(ks)) {
        var k = ks[i];
        total = total + inv[k];
        i = i + 1;
    }
    return total;
}

// Predefined item keys to align with shipments in project demo
function itemKeys() {
    var arr = array();
    push(arr, "apple");
    push(arr, "banana");
    push(arr, "chocolate");
    return arr;
}

// Build frequency map from a numeric array
function frequency(arr) {
    var freq = map();
    var i = 0;
    while (i < length(arr)) {
        var v = arr[i];
        // Store keys as strings to align with keys(map) return values
        var key = "" + v;
        if (has(freq, key)) {
            freq[key] = freq[key] + 1;
        } else {
            freq[key] = 1;
        }
        i = i + 1;
    }
    return freq;
}

```

`examples/projectTest/utils/math.unna`

```c
// Math utilities module

function sum(arr) {
    var s = 0;
    var i = 0;
    while (i < length(arr)) {
        s = s + arr[i];
        i = i + 1;
    }
    return s;
}

function avg(arr) {
    var n = length(arr);
    if (n == 0) {
        return 0;
    }
    return sum(arr) / n;
}

function factorial(n) {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

```

`examples/projectTest/utils/report.unna`

```c
// Reporting utilities module

import inventory as inv;

function printSummary(storeName, stock) {
    print("=== REPORT: " + storeName + " ===");
    print("");
    var total = inv.totalItems(stock);
    print("Total items in stock: " + total);
    print("");

    var ks = keys(stock);
    print("Item breakdown (name -> qty):");
    var i = 0;
    while (i < length(ks)) {
        var k = ks[i];
        print("- " + k + " -> " + stock[k]);
        i = i + 1;
    }
    print("");
}

```

### Project Output

```sh
./bin/unnarize examples/projectTest/main.unna
Tokenized 403 tokens successfully.
=== PROJECT TEST: STORE INVENTORY DASHBOARD ===

Store: Unnarize Mart
Open hours: 9 to 21


Stock level: healthy

Sales sum: 32
Sales avg: 8

factorial(5) = 120

=== REPORT: Unnarize Mart ===

Total items in stock: 37

Item breakdown (name -> qty):
- banana -> 10
- chocolate -> 14
- apple -> 13


-- Sales frequency (units sold per value) --
9 -> 1
5 -> 1
11 -> 1
7 -> 1

=== END OF PROJECT TEST ===
```

---

## Author

**Gtkrshnaaa**