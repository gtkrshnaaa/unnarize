# Functions

> Define reusable blocks of code.

---

## Basic Syntax

```javascript
function greet(name) {
    print("Hello, " + name + "!");
}

greet("World");  // Hello, World!
```

---

## Parameters

Functions can accept zero or more parameters:

```javascript
// No parameters
function sayHello() {
    print("Hello!");
}

// One parameter
function square(x) {
    return x * x;
}

// Multiple parameters
function add(a, b) {
    return a + b;
}

// Many parameters
function createUser(id, name, email, role) {
    // ...
}
```

---

## Return Values

Use `return` to send a value back to the caller:

```javascript
function add(a, b) {
    return a + b;
}

var result = add(5, 3);
print(result);  // 8
```

### Early Return

```javascript
function isPositive(n) {
    if (n > 0) {
        return true;
    }
    return false;
}

// Shorter version
function isPositive2(n) {
    return n > 0;
}
```

### No Return (Returns nil)

```javascript
function logMessage(msg) {
    print("[LOG] " + msg);
    // No return statement
}

var result = logMessage("Test");
print(result == nil);  // true
```

---

## Recursion

Functions can call themselves:

```javascript
function factorial(n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

print(factorial(5));   // 120
print(factorial(10));  // 3628800
```

### Fibonacci

```javascript
function fibonacci(n) {
    if (n <= 1) return n;
    return fibonacci(n - 1) + fibonacci(n - 2);
}

for (var i = 0; i < 10; i = i + 1) {
    print(fibonacci(i));
}
// 0 1 1 2 3 5 8 13 21 34
```

---

## Closures

Functions capture variables from their enclosing scope:

```javascript
function makeCounter() {
    var count = 0;
    
    function increment() {
        count = count + 1;
        return count;
    }
    
    return increment;
}

var counter = makeCounter();
print(counter());  // 1
print(counter());  // 2
print(counter());  // 3
```

---

## Built-in Functions

| Function | Description | Example |
|----------|-------------|---------|
| `print(value)` | Output to console | `print("Hello")` |
| `length(arr)` | Array/string length | `length([1,2,3])` → 3 |
| `push(arr, val)` | Add to array end | `push(arr, 42)` |
| `pop(arr)` | Remove from array end | `pop(arr)` |
| `sqrt(n)` | Square root | `sqrt(16)` → 4 |
| `floor(n)` | Round down | `floor(3.7)` → 3 |
| `ceil(n)` | Round up | `ceil(3.2)` → 4 |
| `abs(n)` | Absolute value | `abs(-5)` → 5 |
| `keys(map)` | Get map keys | `keys(m)` |
| `has(map, key)` | Check key exists | `has(m, "x")` |
| `map()` | Create empty map | `var m = map()` |

### Math Functions

```javascript
print(sqrt(16));    // 4
print(floor(3.7));  // 3
print(ceil(3.2));   // 4
print(abs(-10));    // 10
```

### Map Functions

```javascript
var m = map();
m["name"] = "Alice";
m["age"] = 30;

print(has(m, "name"));  // true
print(has(m, "email")); // false
print(keys(m));         // ["name", "age"]
```

---

## First-Class Functions

Functions are values and can be:

### Stored in Variables

```javascript
function greet(name) {
    print("Hello, " + name);
}

var sayHi = greet;
sayHi("World");  // Hello, World
```

### Passed to Other Functions

```javascript
function apply(fn, value) {
    return fn(value);
}

function double(x) {
    return x * 2;
}

print(apply(double, 5));  // 10
```

### Returned from Functions

```javascript
function makeMultiplier(factor) {
    function multiply(x) {
        return x * factor;
    }
    return multiply;
}

var double = makeMultiplier(2);
var triple = makeMultiplier(3);

print(double(5));  // 10
print(triple(5));  // 15
```

---

## Common Patterns

### Higher-Order Functions

```javascript
function forEach(arr, fn) {
    for (var i = 0; i < length(arr); i = i + 1) {
        fn(arr[i]);
    }
}

function mapArray(arr, fn) {
    var result = [];
    for (var item : arr) {
        push(result, fn(item));
    }
    return result;
}

function filterArray(arr, fn) {
    var result = [];
    for (var item : arr) {
        if (fn(item)) {
            push(result, item);
        }
    }
    return result;
}

// Usage
var numbers = [1, 2, 3, 4, 5];

function isEven(n) { return n % 2 == 0; }
function square(n) { return n * n; }

var evens = filterArray(numbers, isEven);
var squares = mapArray(numbers, square);

print(evens);    // [2, 4]
print(squares);  // [1, 4, 9, 16, 25]
```

### Function with Struct Return

```javascript
struct Result {
    success;
    value;
    error;
}

function divide(a, b) {
    if (b == 0) {
        return Result(false, nil, "Division by zero");
    }
    return Result(true, a / b, nil);
}

var r = divide(10, 2);
if (r.success) {
    print(r.value);  // 5
} else {
    print(r.error);
}
```

---

## Examples

```javascript
// examples/basics/09_functions.unna
function greet(name) {
    print("Hello, " + name + "!");
}

greet("World");

function add(a, b) {
    return a + b;
}

print("Sum: " + add(5, 3));

function factorial(n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

print("Factorial of 5: " + factorial(5));

// examples/basics/10_recursion.unna
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

## Next Steps

- [Modules](modules.md) - Organize functions across files
- [Async/Await](async-await.md) - Async functions
- [Core Libraries](../core-libraries/overview.md) - Built-in library functions
