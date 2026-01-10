# Unnarize Language Interpreter

![C](https://img.shields.io/badge/language-C-blue.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)
![Performance](https://img.shields.io/badge/performance-17M%20ops%2Fsec-brightgreen.svg)


**A simple, dynamic, C-style scripting language interpreter built from scratch in C.**

### Table of Contents

  * [About The Project](https://github.com/gtkrshnaaa/unnarize?tab=readme-ov-file#about-the-project)
  * [Features](https://github.com/gtkrshnaaa/unnarize?tab=readme-ov-file#features)
  * [Performance Benchmarks](https://github.com/gtkrshnaaa/unnarize?tab=readme-ov-file#performance-benchmarks)
  * [Getting Started](https://github.com/gtkrshnaaa/unnarize?tab=readme-ov-file#getting-started)
      * [Prerequisites](https://github.com/gtkrshnaaa/unnarize?tab=readme-ov-file#prerequisites)
      * [Build Instructions](https://github.com/gtkrshnaaa/unnarize?tab=readme-ov-file#build-instructions)
      * [Installation](https://github.com/gtkrshnaaa/unnarize?tab=readme-ov-file#installation)
  * [📚 Learning Unnarize (Tutorial)](examples/tutorial/TUTORIAL.md)
  * [Usage](https://github.com/gtkrshnaaa/unnarize?tab=readme-ov-file#usage)
  * [Plugin System](https://github.com/gtkrshnaaa/unnarize?tab=readme-ov-file#plugin-system)
  * [Language Syntax Showcase](https://github.com/gtkrshnaaa/unnarize?tab=readme-ov-file#language-syntax-showcase)
  * [Interpreter Architecture](https://github.com/gtkrshnaaa/unnarize?tab=readme-ov-file#interpreter-architecture)
  * [Project Structure](https://github.com/gtkrshnaaa/unnarize?tab=readme-ov-file#project-structure)
  * [Complex Code Demonstration](https://github.com/gtkrshnaaa/unnarize?tab=readme-ov-file#complex-code-unnarize-language-interpreter-can-run)
  * [Author](https://github.com/gtkrshnaaa/unnarize?tab=readme-ov-file#author)
  * [Documentation](https://github.com/gtkrshnaaa/unnarize?tab=readme-ov-file#documentation)

-----

## About The Project

Unnarize is a high-performance tree-walk interpreter for a dynamic, C-style scripting language, written entirely in C with zero external dependencies. The project demonstrates advanced interpreter design with optimized hash functions (FNV-1a), value pooling, and string interning infrastructure for maximum performance.

Every component, from the lexical analyzer (Lexer) to the Abstract Syntax Tree (AST) parser and the Virtual Machine (VM) that executes the code, has been built from the ground up. The interpreter achieves **~17.8 million operations per second** on simple loops while maintaining clean architecture and comprehensive feature support.

## Features

- **Clean Syntax**: Familiar C-like syntax with modern touches
- **High Performance**: ~17.8M ops/sec with stack-based local variable resolution
- **Built-in Async**: First-class `async`/`await` support with `ucoreTimer`
- **Structural Typing**: Flexible Structs and UON (Unnarize Object Notation) data format
- **Core Libraries**: Built-in modules for HTTP, Timer, System operations, and UON database
- **Zero Dependencies**: Built using only the C standard library

## Installation

```bash
git clone https://github.com/gtkrshnaaa/unnarize.git
cd unnarize
make
```

Optional system-wide installation:
```bash
sudo make install
```

```

## 📚 Learning Unnarize (Zero to Hero)

We provide a comprehensive, step-by-step tutorial series to take you from writing your first "Hello World" to building complex modular projects and web servers.

[**👉 Start the Tutorial Here**](examples/tutorial/TUTORIAL.md)

1.  **Basics**: Variables, Conditionals, Loops, Functions.
2.  **Data Structures**: Arrays, Maps, Structs.
3.  **Real Projects**: To-Do List, Banking System.
4.  **Advanced**: Core Libraries (Timer, System, UON, HTTP) and Modularity.

## Quick Start

```javascript
// 1. Simple, Clean Syntax
print("Hello, Unnarize!");

// Variables
var name = "Developer";
print("Welcome, " + name);

// Functions
function greet(who) {
    return "Hi, " + who + "!";
}

print(greet("World"));
```

## Examples

### 1. Hello World
```javascript
// 1. Simple, Clean Syntax
print("Hello, Unnarize!");

// Variables
var name = "Developer";
print("Welcome, " + name);

// Functions
function greet(who) {
    return "Hi, " + who + "!";
}

print(greet("World"));
```

### 2. Structures
```javascript
struct User {
    id;
    name;
    roles;
}

// Create instance
var admin = User(1, "Admin", ["admin", "editor"]);

// Print whole object
print(admin); 

// Access
print("User ID: " + admin.id);
print("Role: " + admin.roles[0]);
```

### 3. Async & Timer
```javascript
// 3. Asynchronous Programming
// ucoreTimer is globally available

async function fetchData() {
    print("Fetching data...");
    ucoreTimer.sleep(1000); // Simulate network delay
    return "Data loaded!";
}

print("Start");
var result = await fetchData();
print(result);
print("End");
```

### 4. HTTP Server
```javascript
// 4. Built-in HTTP Server
// ucoreHttp is globally available

function handleRequest(req) {
    print("Received " + req.method + " " + req.path);
    return "Hello from Unnarize Server!";
}

print("Starting server on port 8080...");
ucoreHttp.listen(8080, "handleRequest");
```

### 5. Streaming File I/O (UON)
```javascript
// 5. Streaming File I/O (Lazy Loading)
print("Loading UON file (Streaming)...");
var success = ucoreUon.load("app_data.uon");
if (!success) {
    print("Failed to load app_data.uon");
}

print("Querying settings table...");
var cursor = ucoreUon.get("settings");
if (cursor) {
    print("Iterating rows:");
    var row;
    row = ucoreUon.next(cursor);
    while (row) {
        print("  Row: " + row.key + " = " + row.val);
        row = ucoreUon.next(cursor);
    }
    print("Done.");
} else {
    print("Table not found or error.");
}
```

## About The Project

Unnarize is a high-performance tree-walk interpreter for a dynamic, C-style scripting language, written entirely in C with zero external dependencies. The project demonstrates advanced interpreter design with optimized hash functions (FNV-1a), value pooling, and string interning infrastructure for maximum performance.

Every component, from the lexical analyzer (Lexer) to the Abstract Syntax Tree (AST) parser and the Virtual Machine (VM) that executes the code, has been built from the ground up. The interpreter achieves **~17.8 million operations per second** on simple loops while maintaining clean architecture and comprehensive feature support.

### Core Philosophy
**"Do something with one way, but perfect"** - Focus on efficiency, predictability, and elegant simplicity.

## Documentation

Official documentation: https://gtkrshnaaa.github.io/unnarize/

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

Unnarize delivers exceptional performance with its optimized bytecode VM:

| Operation Type | Performance | Description |
|---|---|---|
| **Simple Loops** | **~800M ops/sec** | Bytecode VM with optimized stack operations |
| **Arithmetic** | ~600M ops/sec | Fast integer math with specialized opcodes |
| **Comparisons** | ~700M ops/sec | Optimized comparison operations |
| **Function Calls** | ~100M ops/sec | Efficient stack frame management |
| **Global Access** | ~50M ops/sec | Hash-based global lookups (fallback) |

**Peak Performance: ~803 million operations per second** (measured with `ucoreTimer`)

> **Note**: Benchmarks measured on modern x86-64 processor. Performance varies by CPU:
> - **Intel Core i5/i7**: 700-800M ops/sec
> - **Intel Celeron**: 200-300M ops/sec (lower clock speed)
> - See `examples/benchmarks/` for detailed benchmarks

### Performance Features

- **Bytecode VM:** Optimized stack-based virtual machine with 100+ specialized opcodes
- **Specialized Operations:** Type-specific opcodes (e.g., `ADD_II` for integer addition)
- **Hotspot Detection:** Automatic profiling for future JIT compilation
- **Stack-Based Locals:** O(1) array access for local variables
- **Mark-and-Sweep GC:** Efficient automatic memory management
- **String Interning:** Request-scoped string deduplication

### 🚀 JIT Compiler (Experimental)

**Status**: Infrastructure complete (90%), compilation working, execution debugging in progress

Unnarize includes a **Full Native JIT compiler** targeting 1.2B ops/sec:
- ✅ **x86-64 code generation** - Manual instruction encoding, zero dependencies
- ✅ **50+ opcodes implemented** - Arithmetic, comparisons, control flow
- ✅ **Jump patching** - Bytecode-to-native offset mapping
- ✅ **Compilation working** - Successfully generates native code (45 bytes → 183 bytes)
- ⚠️ **Execution debugging** - Generated code needs validation (1 bug remaining)

The JIT infrastructure is complete and ready for community contributions. See `z_docs/jit_status_final.md` for details.

*Benchmark: `bin/unnarize examples/benchmarks/simple_loop.unna`*

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

## Core Libraries

Unnarize includes built-in core libraries that provide essential functionality without requiring any external dependencies.

### Available Core Libraries

| Library | Functions | Description |
|---|---|---|
| **ucoreUon** | `load(file)`, `get(table)`, `next(cursor)`, `parse(schema)`, `insert(table, data)`, `save(file)` | Built-in database with streaming file I/O |
| **ucoreHttp** | `listen(port, handler)` | HTTP server for creating web services |
| **ucoreTimer** | `now()`, `sleep(ms)` | Timing operations and sleep functionality |
| **ucoreSystem** | `fileExists(path)` | System-level operations |

### Usage Examples

#### ucoreUon - Database Operations

```javascript
// Load UON database
var success = ucoreUon.load("database.uon");

// Query a table
var cursor = ucoreUon.get("users");

// Iterate through results
var user = ucoreUon.next(cursor);
while (user) {
    print("User: " + user.name);
    user = ucoreUon.next(cursor);
}
```

#### ucoreHttp - Web Server

```javascript
function handleRequest(req) {
    print("Received " + req.method + " " + req.path);
    return "Hello from Unnarize!";
}

ucoreHttp.listen(8080, "handleRequest");
```

#### ucoreTimer - Timing Operations

```javascript
var start = ucoreTimer.now();
// ... do some work ...
ucoreTimer.sleep(1000);  // Sleep 1 second
var elapsed = ucoreTimer.now() - start;
print("Elapsed: " + elapsed + "ms");
```

#### ucoreSystem - System Operations

```javascript
if (ucoreSystem.fileExists("config.uon")) {
    print("Config file found");
} else {
    print("Config file not found");
}
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