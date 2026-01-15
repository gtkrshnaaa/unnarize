# Operators

> Arithmetic, comparison, logical, and assignment operators.

---

## Operator Precedence

From highest to lowest precedence:

| Precedence | Operators | Description |
|------------|-----------|-------------|
| 1 (highest) | `()` | Grouping |
| 2 | `!` `-` `+` (unary) | Unary operators |
| 3 | `*` `/` `%` | Multiplication, division, modulo |
| 4 | `+` `-` | Addition, subtraction |
| 5 | `<` `<=` `>` `>=` | Comparison |
| 6 | `==` `!=` | Equality |
| 7 | `&&` | Logical AND |
| 8 | `\|\|` | Logical OR |
| 9 (lowest) | `=` `+=` `-=` `*=` `/=` | Assignment |

---

## Arithmetic Operators

| Operator | Description | Example | Result |
|----------|-------------|---------|--------|
| `+` | Addition | `5 + 3` | `8` |
| `-` | Subtraction | `5 - 3` | `2` |
| `*` | Multiplication | `5 * 3` | `15` |
| `/` | Division | `10 / 3` | `3.333...` |
| `%` | Modulo (remainder) | `10 % 3` | `1` |

### Examples

```javascript
var a = 10;
var b = 3;

print(a + b);  // 13
print(a - b);  // 7
print(a * b);  // 30
print(a / b);  // 3.333...
print(a % b);  // 1

// Integer division
print(10 / 4);     // 2.5
print(10 % 4);     // 2

// Negative numbers
print(-5 + 3);     // -2
print(5 * -2);     // -10
```

### Unary Operators

```javascript
var x = 5;
print(-x);   // -5 (negation)
print(+x);   // 5  (identity, rarely used)
```

---

## Comparison Operators

All comparison operators return `true` or `false`.

| Operator | Description | Example | Result |
|----------|-------------|---------|--------|
| `==` | Equal to | `5 == 5` | `true` |
| `!=` | Not equal to | `5 != 3` | `true` |
| `<` | Less than | `3 < 5` | `true` |
| `<=` | Less than or equal | `5 <= 5` | `true` |
| `>` | Greater than | `5 > 3` | `true` |
| `>=` | Greater than or equal | `5 >= 5` | `true` |

### Examples

```javascript
var x = 10;
var y = 5;

print(x == y);   // false
print(x != y);   // true
print(x > y);    // true
print(x < y);    // false
print(x >= 10);  // true
print(y <= 5);   // true

// String comparison
print("abc" == "abc");  // true
print("abc" != "xyz");  // true
```

---

## Logical Operators

| Operator | Description | Example | Result |
|----------|-------------|---------|--------|
| `&&` | Logical AND | `true && false` | `false` |
| `\|\|` | Logical OR | `true \|\| false` | `true` |
| `!` | Logical NOT | `!true` | `false` |

### Truth Tables

**AND (`&&`)**
| A | B | A && B |
|---|---|--------|
| true | true | true |
| true | false | false |
| false | true | false |
| false | false | false |

**OR (`||`)**
| A | B | A \|\| B |
|---|---|----------|
| true | true | true |
| true | false | true |
| false | true | true |
| false | false | false |

**NOT (`!`)**
| A | !A |
|---|-----|
| true | false |
| false | true |

### Examples

```javascript
var a = true;
var b = false;

print(a && b);   // false
print(a || b);   // true
print(!a);       // false
print(!b);       // true

// Combined
print((5 > 3) && (2 < 4));  // true
print((5 < 3) || (2 < 4));  // true
print(!(5 > 3));            // false
```

### Short-Circuit Evaluation

Logical operators use short-circuit evaluation:

```javascript
// && stops at first false
var result = false && expensiveOperation();  // expensiveOperation() not called

// || stops at first true
var result = true || expensiveOperation();   // expensiveOperation() not called
```

---

## Assignment Operators

| Operator | Description | Equivalent |
|----------|-------------|------------|
| `=` | Assign | `x = 5` |
| `+=` | Add and assign | `x = x + 5` |
| `-=` | Subtract and assign | `x = x - 5` |
| `*=` | Multiply and assign | `x = x * 5` |
| `/=` | Divide and assign | `x = x / 5` |

### Examples

```javascript
var x = 10;

x = 5;     // x is 5
x += 3;    // x is 8
x -= 2;    // x is 6
x *= 4;    // x is 24
x /= 3;    // x is 8

print(x);  // 8
```

---

## String Concatenation

The `+` operator concatenates strings:

```javascript
var first = "Hello";
var second = "World";

print(first + " " + second);  // "Hello World"

// Auto-conversion
print("Value: " + 42);        // "Value: 42"
print("Active: " + true);     // "Active: true"
```

---

## Type Coercion

Unnarize performs automatic type conversion in certain cases:

```javascript
// String + Number = String
print("Age: " + 25);        // "Age: 25"

// Number + Number = Number
print(10 + 20);             // 30

// Integer + Float = Float
print(10 + 0.5);            // 10.5
```

---

## Common Patterns

### Incrementing and Decrementing

```javascript
var i = 0;
i = i + 1;  // Increment
i = i - 1;  // Decrement

// Or use compound assignment
i += 1;     // Increment
i -= 1;     // Decrement
```

### Conditional Assignment

```javascript
var status = (score >= 60) && true || false;
// Better approach: use if/else
```

### Checking for nil

```javascript
var value = nil;
if (value == nil) {
    print("No value");
}
```

---

## Next Steps

- [Arrays](arrays.md) - Work with collections
- [Control Flow](control-flow.md) - Use operators in conditionals
- [Functions](functions.md) - Pass values to functions
