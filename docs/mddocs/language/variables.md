# Variables

> Declare and use variables in Unnarize.

---

## Declaration

Use the `var` keyword to declare variables:

```javascript
var name = "Unnarize";
var count = 42;
var price = 19.99;
var active = true;
var empty = nil;
```

Variables must be initialized when declared.

---

## Data Types

Unnarize is dynamically typed. A variable can hold any of these types:

| Type | Examples | Description |
|------|----------|-------------|
| **Integer** | `42`, `-10`, `0` | Whole numbers (32-bit signed) |
| **Float** | `3.14`, `-0.5`, `1.0` | Decimal numbers (64-bit double) |
| **String** | `"hello"`, `'world'` | Text values (single or double quotes) |
| **Boolean** | `true`, `false` | Logical values |
| **nil** | `nil` | Represents "no value" |
| **Array** | `[1, 2, 3]` | Ordered collection |
| **Map** | `map()` | Key-value collection |
| **Struct** | `User(1, "Alice")` | Custom data structure |

---

## Type Checking

Check a value's type at runtime:

```javascript
var x = 42;
var y = "hello";
var z = [1, 2, 3];

// Type is inferred automatically
print(x + 1);        // 43 (integer arithmetic)
print(y + " world"); // "hello world" (string concat)
print(z[0]);         // 1 (array access)
```

---

## Reassignment

Variables can be reassigned to new values of any type:

```javascript
var x = 10;
x = 20;              // x is now 20
x = x + 5;           // x is now 25
x = "text";          // Dynamic typing: x is now a string
```

### Compound Assignment

```javascript
var n = 10;
n += 5;   // n = n + 5  → 15
n -= 3;   // n = n - 3  → 12
n *= 2;   // n = n * 2  → 24
n /= 4;   // n = n / 4  → 6
```

---

## String Concatenation

Use `+` to concatenate strings and other values:

```javascript
var name = "Alice";
var age = 25;

print("Name: " + name);         // Name: Alice
print("Age: " + age);           // Age: 25
print("User: " + name + ", " + age); // User: Alice, 25
```

Numbers and booleans are automatically converted to strings when concatenated.

---

## Scope

Variables have **lexical scope** (block scope):

```javascript
var global = "I am global";

function test() {
    var local = "I am local";
    print(global);  // Works - accessing global
    print(local);   // Works - accessing local
    
    if (true) {
        var inner = "I am inner";
        print(inner); // Works
    }
    // print(inner); // Error if uncommented - out of scope
}

test();
print(global);      // Works
// print(local);    // Error - local is out of scope
```

### Scope Rules

1. Variables declared at top level are **global**
2. Variables declared inside `{ }` are **local** to that block
3. Inner scopes can access variables from outer scopes
4. Outer scopes cannot access variables from inner scopes

---

## Shadowing

An inner variable can "shadow" an outer variable with the same name:

```javascript
var x = "outer";

function test() {
    var x = "inner";
    print(x);  // "inner"
}

test();
print(x);  // "outer" (unchanged)
```

---

## nil Value

`nil` represents the absence of a value:

```javascript
var empty = nil;

if (empty == nil) {
    print("Variable is nil");
}

// Functions without return statement return nil
function noReturn() {
    print("No explicit return");
}

var result = noReturn();
print(result == nil);  // true
```

---

## Best Practices

1. **Use descriptive names**: `userName` not `x`
2. **Initialize with meaningful values**: Avoid leaving variables undefined
3. **Keep scope minimal**: Declare variables close to where they're used
4. **Use nil explicitly**: When a variable intentionally has no value

---

## Examples

### Basic Usage

```javascript
// examples/basics/01_variables.unna
var age = 25;
var price = 19.99;
var negative = -42;

print("Integer: " + age);
print("Float: " + price);
print("Negative: " + negative);

var greeting = "Hello";
var name = "Unnarize";
var combined = greeting + ", " + name + "!";

print("Concatenation: " + combined);

var isActive = true;
var isComplete = false;

print("Boolean true: " + isActive);
print("Boolean false: " + isComplete);
```

---

## Next Steps

- [Operators](operators.md) - Learn about arithmetic and comparison operators
- [Arrays](arrays.md) - Work with collections
- [Control Flow](control-flow.md) - Conditional logic and loops
