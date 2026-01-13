# Unnarize

![C](https://img.shields.io/badge/language-C-blue.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)

A high-performance bytecode interpreter for a dynamic, C-style scripting language.

---

## Features

- **Clean C-Style Syntax** — Familiar and intuitive
- **High Performance** — Bytecode VM with computed goto dispatch
- **Module System** — Hierarchical imports with relative paths
- **Async/Await** — First-class asynchronous programming
- **Structs** — Custom data types with field access
- **Core Libraries** — Built-in HTTP, Timer, System, UON database
- **Zero Dependencies** — Pure C implementation

## Quick Start

```bash
git clone https://github.com/gtkrshnaaa/unnarize.git
cd unnarize
make
./bin/unnarize examples/basics/01_variables.unna
```

## Examples

```
examples/
├── basics/           # Core language features (11 demos)
│   ├── 01_variables.unna
│   ├── 02_operators.unna
│   ├── 03_arrays.unna
│   ├── 04_structs.unna
│   ├── 05_control_if.unna
│   ├── 06_control_while.unna
│   ├── 07_control_for.unna
│   ├── 08_control_foreach.unna
│   ├── 09_functions.unna
│   ├── 10_recursion.unna
│   └── 11_async.unna
├── benchmark/        # Performance testing
│   └── benchmark.unna
└── testcase/         # SME Store System (modular architecture demo)
    ├── main.unna
    ├── models/
    │   ├── product.unna
    │   ├── customer.unna
    │   └── order.unna
    ├── services/
    │   ├── inventory.unna
    │   ├── sales.unna
    │   └── reporting.unna
    └── utils/
        ├── logger.unna
        └── validator.unna
```

### Run Examples

```bash
# Basic demos
./bin/unnarize examples/basics/01_variables.unna

# SME Store System
./bin/unnarize examples/testcase/main.unna

# Benchmark
./bin/unnarize examples/benchmark/benchmark.unna
```

## Language Overview

### Variables & Types

```javascript
var name = "Unnarize";
var count = 42;
var price = 19.99;
var active = true;

print("Hello, " + name);
```

### Functions

```javascript
function greet(who) {
    return "Hello, " + who + "!";
}

print(greet("World"));
```

### Structs

```javascript
struct Point {
    x;
    y;
}

var p = Point(10, 20);
print("Position: (" + p.x + ", " + p.y + ")");
```

### Control Flow

```javascript
// If-Else
if (score >= 90) {
    print("Grade A");
} else if (score >= 70) {
    print("Grade B");
} else {
    print("Grade C");
}

// For Loop
for (var i = 0; i < 5; i = i + 1) {
    print(i);
}

// Foreach
var items = ["apple", "banana", "cherry"];
for (var item : items) {
    print(item);
}
```

### Modules & Imports

```javascript
// main.unna
import "./models/product.unna" as product;
import "./utils/logger.unna" as log;

var p = product.createProduct("Laptop", 999, 50);
log.logInfo("Created: " + p.name);
```

Import paths are relative to the importing file (like PHP `include` or HTML `href`).

### Async/Await

```javascript
async function fetchData(source) {
    print("Fetching " + source + "...");
    return "Data from " + source;
}

async function main() {
    var task1 = fetchData("API");
    var task2 = fetchData("DB");
    
    var r1 = await task1;
    var r2 = await task2;
    
    print("Results: " + r1 + ", " + r2);
}

main();
```

## Core Libraries

| Library | Functions |
|---------|-----------|
| `ucoreTimer` | `now()`, `sleep(ms)` |
| `ucoreHttp` | `listen(port, handler)` |
| `ucoreSystem` | `fileExists(path)` |
| `ucoreUon` | `load()`, `get()`, `next()`, `save()` |

## Build & Install

```bash
# Build
make

# Install system-wide (optional)
sudo make install

# Uninstall
sudo make uninstall
```

## Architecture

```
Source (.unna) → [Lexer] → Tokens → [Parser] → AST → [Compiler] → Bytecode → [VM] → Output
```

- **Lexer** — Tokenizes source code
- **Parser** — Builds Abstract Syntax Tree
- **Compiler** — Generates bytecode
- **VM** — Executes bytecode with computed goto dispatch

## Project Structure

```
.
├── bin/              # Built executable
├── examples/         # Language examples
├── include/          # Header files
├── src/              # Source files
│   ├── lexer.c
│   ├── parser.c
│   ├── vm.c
│   └── bytecode/
│       ├── compiler.c
│       └── interpreter.c
├── Makefile
└── README.md
```

## Author

**gtkrshnaaa**

## License

MIT License