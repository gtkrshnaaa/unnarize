# Unnarize

![C](https://img.shields.io/badge/language-C-blue.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)
![Build](https://img.shields.io/badge/build-passing-brightgreen)
![GC](https://img.shields.io/badge/GC-Gapless%20Generational-purple)

**Unnarize** is a high-performance, embedded scripting language built for speed and reliability. It features a modern **"Gapless" Generational Garbage Collector**, native **Async/Await** support, and a lightweight bytecode VM, making it ideal for long-running embedded services or rapid prototyping.

---

## Key Features

### "Gapless" Garbage Collector
Unnarize implements a production-grade GC architecture comparable to V8 (Node.js) and Go:
*   **Generational Heap**: Separates new objects (Nursery) from long-lived ones (Old Gen) for cache efficiency.
*   **Concurrent Marking**: Traces object graphs in a background thread without freezing the app.
*   **Parallel Sweeping**: Cleans up memory in the background, eliminating "Stop-The-World" pauses.
*   **Thread Safe**: Fully synchronized with Mutexes and Atomic Write Barriers.

### Native Async/Await
First-class support for non-blocking concurrency:
*   **`async` / `await`**: Syntactic sugar for handling `Future` objects.
*   **Event Loop**: Native implementation for handling I/O and background tasks.

### Optimization
*   **Computed Goto Dispatch**: Extremely fast instruction dispatching (GCC/Clang extension).
*   **Specialized Opcodes**: Optimized paths for integers, floats, and local variable access.
*   **Zero Dependency**: Pure C implementation (standard libc + pthreads).

---

## Performance Benchmarks

Tests performed on **Intel Core i5-1135G7 @ 4.20GHz** (TigerLake-LP) with 8GB RAM.

### Compute Throughput
| Benchmark | Result | Ops/Sec | Note |
|-----------|--------|---------|------|
| **Heavy Loop** | 10M iterations | **~275 M** | `while` loop increment |
| **Fibonacci** | fib(30) | **~70 ms** | Recursive function calls |

### GC Stress Test
| Scenario | Load | Result |
|----------|------|--------|
| **Massive Allocation** | 50,000 Arrays/Objects | **Stable** (No leaks) |
| **Long Running** | 15,000 Requests | **<10ms Pause** |
| **Throughput** | 70,000 Allocations | **100% Success** |

---

## Quick Start

### Build from Source
```bash
git clone https://github.com/gtkrshnaaa/unnarize.git
cd unnarize
make
# sudo make install  (Optional)
```

### Run 'SME Store' Demo
A complete modular application simulating a store management system:
```bash
./bin/unnarize examples/testcase/main.unna
```

### Run Benchmarks
```bash
./bin/unnarize examples/benchmark/benchmark.unna
```

---

## Language Tour

### 1. Generational GC in Action
The GC works silently. You create objects, and they are managed automatically.

```javascript
// This typically creates extensive pressure on the nursery
for (var i = 0; i < 10000; i = i + 1) {
    var temp = ["data", i]; // Allocated in Nursery
    // Immediately discarded -> Swept instantly
}
print("Done without leaks!");
```

### 2. Async/Await
Native concurrency allows non-blocking I/O.

```javascript
async function fetchData(url) {
    print("Fetching " + url + "...");
    // Simulate network delay
    return "Response from " + url;
}

async function main() {
    print("Start");
    var task = fetchData("api.google.com");
    var result = await task; // Pauses here non-blocking
    print(result);
}

main();
```

### 3. Structs & Objects
```javascript
struct User {
    id;
    name;
    email;
}

var u = User(1, "Alice", "alice@example.com");
print(u.name); // "Alice"
```

---

## Core Libraries

Unnarize comes with a powerful standard library:
*   **`ucoreSystem`**: File I/O, Environment Variables.
*   **`ucoreTimer`**: High-precision timing.
*   **`ucoreHttp`**: Built-in HTTP Server/Client (`listen`, `get`, `post`).
*   **`ucoreUon`**: Parser for UON (Unnarize Object Notation) data format.

---

## Architecture

```
[Source Code] -> [Lexer] -> [AST] -> [Compiler] -> [Bytecode Chunk]
                                                         |
                                                         v
                                                 [Virtual Machine]
                                                /        |        \
                                     [Interpreter]    [Heap GC]   [Native Interface]
```

## License
MIT License. Created by **gtkrshnaaa**.