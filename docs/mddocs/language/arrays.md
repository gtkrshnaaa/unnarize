# Arrays

> Ordered collections of values.

---

## Creating Arrays

### Array Literals

```javascript
var empty = [];
var numbers = [1, 2, 3, 4, 5];
var mixed = ["hello", 42, true, nil];
var nested = [[1, 2], [3, 4], [5, 6]];
```

### Dynamic Arrays

```javascript
var items = [];
push(items, "apple");
push(items, "banana");
push(items, "cherry");
print(items);  // ["apple", "banana", "cherry"]
```

---

## Accessing Elements

Use square brackets with a zero-based index:

```javascript
var fruits = ["apple", "banana", "cherry"];

print(fruits[0]);  // "apple"
print(fruits[1]);  // "banana"
print(fruits[2]);  // "cherry"

// Negative index behavior is undefined
// Out of bounds returns nil
print(fruits[99]); // nil
```

---

## Modifying Elements

```javascript
var colors = ["red", "green", "blue"];

colors[0] = "crimson";
colors[1] = "lime";

print(colors);  // ["crimson", "lime", "blue"]
```

---

## Built-in Array Functions

| Function | Description | Returns |
|----------|-------------|---------|
| `length(arr)` | Get array length | Integer |
| `push(arr, value)` | Add to end | nil |
| `pop(arr)` | Remove from end | Removed value |

### length(arr)

```javascript
var items = [1, 2, 3, 4, 5];
print(length(items));  // 5

var empty = [];
print(length(empty));  // 0
```

### push(arr, value)

```javascript
var stack = [];

push(stack, "first");
push(stack, "second");
push(stack, "third");

print(stack);         // ["first", "second", "third"]
print(length(stack)); // 3
```

### pop(arr)

```javascript
var stack = ["a", "b", "c"];

var last = pop(stack);
print(last);    // "c"
print(stack);   // ["a", "b"]

var next = pop(stack);
print(next);    // "b"
print(stack);   // ["a"]
```

---

## Iterating Arrays

### Using for loop

```javascript
var items = ["apple", "banana", "cherry"];

for (var i = 0; i < length(items); i = i + 1) {
    print(items[i]);
}
// Output:
// apple
// banana
// cherry
```

### Using foreach

```javascript
var items = ["apple", "banana", "cherry"];

for (var item : items) {
    print(item);
}
// Output:
// apple
// banana
// cherry
```

### With index in foreach

```javascript
var items = ["a", "b", "c"];
var i = 0;

for (var item : items) {
    print(i + ": " + item);
    i = i + 1;
}
// Output:
// 0: a
// 1: b
// 2: c
```

---

## Nested Arrays (2D Arrays)

```javascript
var matrix = [
    [1, 2, 3],
    [4, 5, 6],
    [7, 8, 9]
];

// Access element at row 1, column 2
print(matrix[1][2]);  // 6

// Iterate 2D array
for (var i = 0; i < length(matrix); i = i + 1) {
    for (var j = 0; j < length(matrix[i]); j = j + 1) {
        print(matrix[i][j]);
    }
}
```

---

## Common Patterns

### Stack (LIFO)

```javascript
var stack = [];

// Push items
push(stack, 1);
push(stack, 2);
push(stack, 3);

// Pop items (Last In, First Out)
print(pop(stack));  // 3
print(pop(stack));  // 2
print(pop(stack));  // 1
```

### Finding an Element

```javascript
function find(arr, target) {
    for (var i = 0; i < length(arr); i = i + 1) {
        if (arr[i] == target) {
            return i;
        }
    }
    return -1;  // Not found
}

var items = ["apple", "banana", "cherry"];
print(find(items, "banana"));  // 1
print(find(items, "grape"));   // -1
```

### Sum of Array

```javascript
function sum(arr) {
    var total = 0;
    for (var item : arr) {
        total = total + item;
    }
    return total;
}

var numbers = [1, 2, 3, 4, 5];
print(sum(numbers));  // 15
```

### Maximum Value

```javascript
function max(arr) {
    if (length(arr) == 0) return nil;
    var maximum = arr[0];
    for (var i = 1; i < length(arr); i = i + 1) {
        if (arr[i] > maximum) {
            maximum = arr[i];
        }
    }
    return maximum;
}

var numbers = [3, 1, 4, 1, 5, 9, 2, 6];
print(max(numbers));  // 9
```

### Reverse Array

```javascript
function reverse(arr) {
    var result = [];
    for (var i = length(arr) - 1; i >= 0; i = i - 1) {
        push(result, arr[i]);
    }
    return result;
}

var items = [1, 2, 3];
print(reverse(items));  // [3, 2, 1]
```

---

## Array of Structs

```javascript
struct Person { name; age; }

var people = [
    Person("Alice", 30),
    Person("Bob", 25),
    Person("Charlie", 35)
];

for (var person : people) {
    print(person.name + " is " + person.age);
}
// Output:
// Alice is 30
// Bob is 25
// Charlie is 35
```

---

## Examples

```javascript
// examples/basics/03_arrays.unna
var fruits = ["apple", "banana", "cherry"];

print("First: " + fruits[0]);
print("Length: " + length(fruits));

push(fruits, "date");
print("After push: " + length(fruits));

var last = pop(fruits);
print("Popped: " + last);

for (var fruit : fruits) {
    print("- " + fruit);
}
```

---

## Next Steps

- [Structs](structs.md) - Custom data structures
- [Control Flow](control-flow.md) - Loops for array iteration
- [Functions](functions.md) - Array processing functions
