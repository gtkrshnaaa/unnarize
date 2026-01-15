# Basic Examples

> Walkthrough of the 11 fundamental example scripts.

---

## Overview

The `examples/basics/` directory contains scripts demonstrating core language features:

| File | Topic |
|------|-------|
| `01_variables.unna` | Variable declaration and types |
| `02_operators.unna` | Arithmetic and comparison |
| `03_arrays.unna` | Array operations |
| `04_structs.unna` | Custom data structures |
| `05_control_if.unna` | If/else conditionals |
| `06_control_while.unna` | While loops |
| `07_control_for.unna` | For loops |
| `08_control_foreach.unna` | Foreach iteration |
| `09_functions.unna` | Function definition |
| `10_recursion.unna` | Recursive functions |
| `11_async.unna` | Async/await |

---

## 01_variables.unna

Learn variable declaration with different types:

```javascript
// Numbers
var age = 25;
var price = 19.99;
var negative = -42;

print("Integer: " + age);
print("Float: " + price);
print("Negative: " + negative);

// Strings
var greeting = "Hello";
var name = "Unnarize";
var combined = greeting + ", " + name + "!";

print("Concatenation: " + combined);

// Booleans
var isActive = true;
var isComplete = false;

print("Boolean true: " + isActive);
print("Boolean false: " + isComplete);
```

**Run:**
```bash
./bin/unnarize examples/basics/01_variables.unna
```

---

## 02_operators.unna

Arithmetic and comparison operations:

```javascript
var a = 10;
var b = 3;

// Arithmetic
print("a + b = " + (a + b));  // 13
print("a - b = " + (a - b));  // 7
print("a * b = " + (a * b));  // 30
print("a / b = " + (a / b));  // 3.33...
print("a % b = " + (a % b));  // 1

// Comparison
print("a > b: " + (a > b));   // true
print("a == 10: " + (a == 10)); // true

// Compound assignment
var x = 5;
x += 3;  // 8
x *= 2;  // 16
print("Result: " + x);
```

---

## 03_arrays.unna

Working with arrays:

```javascript
var fruits = ["apple", "banana", "cherry"];

// Access
print("First: " + fruits[0]);

// Length
print("Count: " + length(fruits));

// Push
push(fruits, "date");
print("After push: " + length(fruits));

// Pop
var last = pop(fruits);
print("Popped: " + last);

// Iterate
for (var fruit : fruits) {
    print("- " + fruit);
}
```

---

## 04_structs.unna

Defining and using structs:

```javascript
struct User {
    id;
    name;
    email;
}

var u = User(1, "Alice", "alice@example.com");

print("ID: " + u.id);
print("Name: " + u.name);
print("Email: " + u.email);

// Modify
u.email = "alice.new@example.com";
print("Updated: " + u.email);

// Nested
struct Department {
    name;
    manager;
}

var dev = Department("Engineering", u);
print("Dept: " + dev.name);
print("Manager: " + dev.manager.name);
```

---

## 05_control_if.unna

Conditional statements:

```javascript
var x = 10;

if (x > 5) {
    print("x is greater than 5");
}

if (x == 10) {
    print("x equals 10");
} else {
    print("x does not equal 10");
}

// Chained
var grade = 85;
if (grade >= 90) {
    print("A");
} else if (grade >= 80) {
    print("B");
} else if (grade >= 70) {
    print("C");
} else {
    print("F");
}
```

---

## 06_control_while.unna

While loops:

```javascript
var count = 0;

while (count < 5) {
    print("Count: " + count);
    count = count + 1;
}

print("Final: " + count);

// Sentinel pattern
var sum = 0;
var n = 1;
while (n <= 100) {
    sum = sum + n;
    n = n + 1;
}
print("Sum 1-100: " + sum);
```

---

## 07_control_for.unna

C-style for loops:

```javascript
// Basic for loop
for (var i = 0; i < 5; i = i + 1) {
    print("Iteration: " + i);
}

// Countdown
for (var i = 10; i > 0; i = i - 1) {
    print(i);
}
print("Go!");

// Nested
for (var i = 1; i <= 3; i = i + 1) {
    for (var j = 1; j <= 3; j = j + 1) {
        print(i + " x " + j + " = " + (i * j));
    }
}
```

---

## 08_control_foreach.unna

Iterate over collections:

```javascript
var items = ["apple", "banana", "cherry"];

for (var item : items) {
    print("Item: " + item);
}

// With index
var index = 0;
for (var item : items) {
    print(index + ": " + item);
    index = index + 1;
}

// Structs in array
struct Point { x; y; }
var points = [Point(0, 0), Point(1, 1), Point(2, 4)];

for (var p : points) {
    print("(" + p.x + ", " + p.y + ")");
}
```

---

## 09_functions.unna

Defining and calling functions:

```javascript
function greet(name) {
    print("Hello, " + name + "!");
}

greet("World");

// Return value
function add(a, b) {
    return a + b;
}

var sum = add(5, 3);
print("Sum: " + sum);

// Multiple parameters
function clamp(value, min, max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

print(clamp(5, 0, 10));   // 5
print(clamp(-3, 0, 10));  // 0
print(clamp(15, 0, 10));  // 10
```

---

## 10_recursion.unna

Recursive function calls:

```javascript
// Factorial
function factorial(n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

print("5! = " + factorial(5));   // 120
print("10! = " + factorial(10)); // 3628800

// Fibonacci
function fib(n) {
    if (n <= 1) return n;
    return fib(n - 1) + fib(n - 2);
}

print("Fibonacci sequence:");
for (var i = 0; i < 10; i = i + 1) {
    print(fib(i));
}
```

---

## 11_async.unna

Asynchronous programming:

```javascript
print("=== Async/Await ===");

async function fetchData(source) {
    print("  Fetching from " + source + "...");
    return "Data:" + source;
}

async function processTask(id) {
    print("  Processing task " + id + "...");
    return "Task" + id + " complete";
}

async function runWorkflow() {
    print("Workflow started");
    
    // Start tasks in parallel
    var task1 = fetchData("API");
    var task2 = fetchData("Database");
    
    // Await results
    var result1 = await task1;
    var result2 = await task2;
    
    print("  Got: " + result1);
    print("  Got: " + result2);
    
    print("Workflow finished");
}

runWorkflow();

print("=== Complete ===");
```

**Expected Output:**
```
=== Async/Await ===
Workflow started
  Fetching from API...
  Fetching from Database...
  Got: Data:API
  Got: Data:Database
Workflow finished
=== Complete ===
```

---

## Running All Examples

```bash
for file in examples/basics/*.unna; do
    echo "=== $file ==="
    ./bin/unnarize "$file"
    echo
done
```

---

## Next Steps

- [SME System](sme-system.md) - Complete application example
- [Language Reference](../language/variables.md) - Detailed documentation
- [Core Libraries](../core-libraries/overview.md) - Built-in libraries
