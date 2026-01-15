# Introduction to Unnarize

> A high-performance bytecode interpreter for a dynamic, C-style scripting language with "Gapless" Generational Garbage Collection and native Async/Await support.

---

## What is Unnarize?

**Unnarize** is a modern scripting language designed for **speed**, **reliability**, and **ease of use**. It compiles source code to bytecode and executes it on a high-performance virtual machine.

### Key Characteristics

- **Dynamically typed** - Variables can hold any type
- **C-style syntax** - Familiar to JavaScript, C, Java developers
- **High performance** - ~275 million operations/second
- **Zero dependencies** - Pure C (standard libc + pthreads only)

---

## Core Philosophy

> *"Do something with one way, but perfect."*

Unnarize is designed with one core principle: force the machine to work in the most efficient, repetitive, and predictable manner possible.

### Design Principles

1. **Minimal Abstraction** - Direct interaction with the machine
2. **Cache Locality** - Data and instructions reside in CPU cache
3. **Predictable Execution** - Consistent instruction paths
4. **Zero External Dependencies** - Pure C implementation

---

## Key Features

### High Performance VM

- **Computed Goto Dispatch** - Extremely fast instruction dispatching (GCC/Clang extension)
- **Specialized Opcodes** - Optimized paths for integers, floats, and locals
- **NaN Boxing** - Efficient 64-bit value representation without heap allocation

### "Gapless" Garbage Collection

- **Generational Heap** - Separates short-lived objects (Nursery) from long-lived ones (Old Gen)
- **Concurrent Marking** - Background thread traces object graphs while execution continues
- **Parallel Sweeping** - Memory cleanup without "Stop-The-World" pauses
- **Write Barriers** - Maintains tri-color invariant for correctness

### Native Async/Await

- **async** functions return Future objects
- **await** expressions block until Future resolves
- Native event loop for I/O and background tasks

### Rich Standard Library

| Library | Purpose |
|---------|---------|
| `ucoreString` | Text manipulation (split, join, regex) |
| `ucoreScraper` | HTML parsing with CSS selectors |
| `ucoreJson` | JSON parse/stringify |
| `ucoreHttp` | HTTP client and server |
| `ucoreTimer` | High-precision timing |
| `ucoreSystem` | File I/O, shell execution |
| `ucoreUon` | UON data format parser |

---

## Performance Benchmarks

Tested on **Intel Core i5-1135G7 @ 4.20GHz** with 8GB RAM:

| Benchmark | Result | Notes |
|-----------|--------|-------|
| **Heavy Loop** | ~275 M ops/sec | while loop increment |
| **Fibonacci(30)** | ~70 ms | Recursive calls |
| **GC Stress** | 50,000 arrays | Stable, no leaks |
| **Long Running** | 15,000 requests | <10ms pause |

### Library Performance

| Operation | Data Size | Speed |
|-----------|-----------|-------|
| String `contains` | 14 KB | 24.5 M ops/sec |
| String `toLower` | 14 KB | 42,500 ops/sec |
| String `replace` | 14 KB | 38,800 ops/sec |
| HTML `parseFile` | 370 KB | 339 ops/sec |

---

## Quick Example

```javascript
// Hello World
var name = "Unnarize";
print("Hello, " + name + "!");

// Function
function factorial(n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}
print(factorial(10)); // 3628800

// Async/Await
async function fetchData(url) {
    return "Response from " + url;
}

async function main() {
    var result = await fetchData("api.example.com");
    print(result);
}
main();

// HTTP Server
function handleApi(req) {
    return "{\"status\":\"ok\"}";
}
ucoreHttp.route("GET", "/api", "handleApi");
ucoreHttp.listen(8080);
```

---

## Comparison with Other Languages

| Feature | Unnarize | Node.js (V8) | Go | Lua |
|---------|----------|--------------|-----|-----|
| Generational GC | ✅ | ✅ | ❌ | ❌ |
| Concurrent Marking | ✅ | ✅ | ✅ | ❌ |
| Async/Await | ✅ | ✅ | ❌ | ❌ |
| Zero Dependencies | ✅ | ❌ | ❌ | ✅ |
| Embeddable | ✅ | ❌ | ❌ | ✅ |

---

## Next Steps

- [Installation Guide](installation.md) - Build and run Unnarize
- [Variables](../language/variables.md) - Start learning the language

---

## About

Unnarize is created by **gtkrshnaaa**, a programmer from Indonesia focused on language design, systems programming, and pushing the boundaries of scripting language performance.

- [GitHub Repository](https://github.com/gtkrshnaaa/unnarize)
