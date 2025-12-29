# Unnarize Performance Benchmarks

This directory contains comprehensive performance benchmarks for the Unnarize programming language interpreter.

## 📊 Benchmark Suite

### 1. Simple Loop Benchmark (`01_simple_loop_benchmark.unna`)
Tests raw loop iteration performance.

**Tests**:
- 1 Million iterations
- 5 Million iterations  
- 10 Million iterations

**Measures**: Pure loop overhead, counter increments

---

### 2. Arithmetic Operations Benchmark (`02_arithmetic_benchmark.unna`)
Tests mathematical operation performance.

**Tests** (5M operations each):
- Addition (`+`)
- Multiplication (`*`)
- Division (`/`)
- Modulo (`%`)
- Mixed operations

**Measures**: Integer arithmetic performance

---

### 3. Array Operations Benchmark (`03_array_benchmark.unna`)
Tests array manipulation and access performance.

**Tests**:
- Array push (1M operations)
- Array read access (5M operations)
- Array write access (2M operations)
- Array iteration (1M operations)
- Array length checks (10M operations)

**Measures**: Dynamic array performance

---

### 4. Function Call Benchmark (`04_function_call_benchmark.unna`)
Tests function invocation overhead.

**Tests**:
- Simple function calls (5M operations)
- Recursive factorial (100K operations)
- Fibonacci recursion (50K operations)
- Nested function calls (1M operations)

**Measures**: Call stack management, recursion performance

---

### 5. String Operations Benchmark (`05_string_benchmark.unna`)
Tests string manipulation performance.

**Tests**:
- String concatenation (500K operations)
- Number to string conversion (1M operations)
- Multiple concatenations (500K operations)
- String length checks (5M operations)

**Measures**: String handling, GC impact

---

### 6. Struct Operations Benchmark (`06_struct_benchmark.unna`)
Tests struct creation and field access.

**Tests**:
- Struct creation (1M operations)
- Field read access (10M operations)
- Field write access (5M operations)
- Struct in arrays (500K operations)
- Struct array iteration (100K operations)

**Measures**: Object-oriented features performance

---

## 🚀 Running Benchmarks

### Run Individual Benchmark
```bash
./bin/unnarize examples/benchmarks/01_simple_loop_benchmark.unna
```

### Run All Benchmarks
```bash
# Simple loop
./bin/unnarize examples/benchmarks/01_simple_loop_benchmark.unna

# Arithmetic
./bin/unnarize examples/benchmarks/02_arithmetic_benchmark.unna

# Arrays
./bin/unnarize examples/benchmarks/03_array_benchmark.unna

# Functions
./bin/unnarize examples/benchmarks/04_function_call_benchmark.unna

# Strings
./bin/unnarize examples/benchmarks/05_string_benchmark.unna

# Structs
./bin/unnarize examples/benchmarks/06_struct_benchmark.unna
```

### Quick Run All (Bash)
```bash
for bench in examples/benchmarks/0*.unna; do
    echo "Running $bench..."
    ./bin/unnarize "$bench"
    echo ""
done
```

---

## 📈 Understanding Results

Each benchmark outputs:
- **Counter/Result**: Verification that computation completed
- **Time**: Elapsed time in milliseconds
- **Operations/sec**: Throughput (operations per second)

### Example Output
```
Test 1: 10 Million iterations
Counter: 10000000
Time: 562 ms
Operations/sec: 17793594
```

This means: **~17.8 million operations per second**

---

## 🎯 Performance Targets

Based on Unnarize's design goals:

| Operation Type | Target | Typical |
|----------------|--------|---------|
| Simple Loops | 15-20M ops/sec | ~17.8M |
| Arithmetic | 10-15M ops/sec | ~15M |
| Array Access | 5-10M ops/sec | ~8M |
| Function Calls | 1-3M ops/sec | ~2M |
| String Ops | 0.5-2M ops/sec | ~1M |
| Struct Access | 5-10M ops/sec | ~7M |

---

## 🔧 Customizing Benchmarks

To create your own benchmark:

```javascript
// my_benchmark.unna
print("=== My Custom Benchmark ===");

var iterations = 1000000;
var start = ucoreTimer.now();

// Your code to benchmark
var result = 0;
for (var i = 0; i < iterations; i = i + 1) {
    result = result + i;
}

var end = ucoreTimer.now();
var elapsed = end - start;
var opsPerSec = iterations / (elapsed / 1000);

print("Result: " + result);
print("Time: " + elapsed + " ms");
print("Operations/sec: " + opsPerSec);
```

---

## 📝 Notes

- All benchmarks use `ucoreTimer.now()` for timing
- Warm-up iterations are included where appropriate
- Results may vary based on system load
- Run benchmarks multiple times for consistent results
- Larger iteration counts provide more accurate measurements

---

## 🎓 Interpreting Performance

**High operations/sec** (>10M):
- Efficient VM execution
- Good cache locality
- Minimal overhead

**Medium operations/sec** (1-10M):
- Expected for complex operations
- Acceptable for most use cases

**Low operations/sec** (<1M):
- Heavy operations (GC, allocation)
- Still performant for scripting tasks

---

## 🔍 Profiling Tips

1. **Isolate operations**: Test one thing at a time
2. **Use large iterations**: Minimize timing overhead
3. **Warm-up**: Run a few iterations before timing
4. **Consistent environment**: Close other applications
5. **Multiple runs**: Average results for accuracy

---

**Created**: 2025-12-29  
**Unnarize Version**: 0.1.1-beta  
**Author**: gtkrshnaaa
