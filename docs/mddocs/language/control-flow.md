# Control Flow

> Conditionals and loops for program logic.

---

## If Statement

Execute code based on a condition:

```javascript
var age = 18;

if (age >= 18) {
    print("Adult");
}
```

### If-Else

```javascript
var score = 75;

if (score >= 60) {
    print("Pass");
} else {
    print("Fail");
}
```

### If-Else If-Else

```javascript
var grade = 85;

if (grade >= 90) {
    print("A");
} else if (grade >= 80) {
    print("B");
} else if (grade >= 70) {
    print("C");
} else if (grade >= 60) {
    print("D");
} else {
    print("F");
}
// Output: B
```

---

## Truthiness

Values are evaluated for truthiness in conditions:

| Value | Truthiness |
|-------|------------|
| `true` | truthy |
| `false` | falsy |
| `nil` | falsy |
| `0` | truthy (unlike some languages!) |
| `""` | truthy (unlike some languages!) |
| Any number | truthy |
| Any string | truthy |
| Any array | truthy |
| Any struct | truthy |

```javascript
if (0) {
    print("0 is truthy in Unnarize!");  // This prints!
}

if (nil) {
    print("This won't print");
} else {
    print("nil is falsy");  // This prints
}
```

---

## While Loop

Execute code repeatedly while a condition is true:

```javascript
var i = 0;

while (i < 5) {
    print(i);
    i = i + 1;
}
// Output: 0 1 2 3 4
```

### Infinite Loop (Controlled)

```javascript
var running = true;
var count = 0;

while (running) {
    count = count + 1;
    if (count >= 10) {
        running = false;
    }
}
print("Final count: " + count);  // 10
```

---

## For Loop (C-Style)

The classic three-part for loop:

```javascript
for (var i = 0; i < 5; i = i + 1) {
    print(i);
}
// Output: 0 1 2 3 4
```

### Parts of For Loop

```
for (initialization; condition; increment) {
    body
}
```

| Part | Description |
|------|-------------|
| `initialization` | Runs once before loop starts |
| `condition` | Checked before each iteration |
| `increment` | Runs after each iteration |

### Variations

```javascript
// Count down
for (var i = 10; i > 0; i = i - 1) {
    print(i);
}

// Step by 2
for (var i = 0; i < 10; i = i + 2) {
    print(i);  // 0, 2, 4, 6, 8
}

// Nested loops
for (var i = 0; i < 3; i = i + 1) {
    for (var j = 0; j < 3; j = j + 1) {
        print(i + "," + j);
    }
}
```

---

## For-Each Loop

Iterate over array elements:

```javascript
var fruits = ["apple", "banana", "cherry"];

for (var fruit : fruits) {
    print(fruit);
}
// Output:
// apple
// banana
// cherry
```

### With Index

```javascript
var items = ["a", "b", "c"];
var index = 0;

for (var item : items) {
    print(index + ": " + item);
    index = index + 1;
}
```

---

## Return as Control Flow

Use `return` to exit a function early:

```javascript
function findFirst(arr, target) {
    for (var i = 0; i < length(arr); i = i + 1) {
        if (arr[i] == target) {
            return i;  // Exit function immediately
        }
    }
    return -1;
}

var items = [10, 20, 30, 40];
print(findFirst(items, 30));  // 2
```

---

## Nested Control Flow

Combine conditionals and loops:

```javascript
var matrix = [
    [1, 2, 3],
    [4, 5, 6],
    [7, 8, 9]
];

var target = 5;
var found = false;

for (var i = 0; i < length(matrix); i = i + 1) {
    for (var j = 0; j < length(matrix[i]); j = j + 1) {
        if (matrix[i][j] == target) {
            print("Found at " + i + "," + j);
            found = true;
        }
    }
}

if (!found) {
    print("Not found");
}
```

---

## Common Patterns

### Accumulator

```javascript
var sum = 0;
for (var i = 1; i <= 100; i = i + 1) {
    sum = sum + i;
}
print(sum);  // 5050
```

### Counter

```javascript
var items = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
var evenCount = 0;

for (var item : items) {
    if (item % 2 == 0) {
        evenCount = evenCount + 1;
    }
}
print("Even numbers: " + evenCount);  // 5
```

### Filtering

```javascript
var numbers = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
var evens = [];

for (var n : numbers) {
    if (n % 2 == 0) {
        push(evens, n);
    }
}
print(evens);  // [2, 4, 6, 8, 10]
```

### Early Exit with Flag

```javascript
var data = [10, 20, 30, -1, 40, 50];
var valid = true;

for (var item : data) {
    if (item < 0) {
        valid = false;
        print("Invalid data: " + item);
    }
}

if (valid) {
    print("All data is valid");
}
```

---

## Examples

```javascript
// examples/basics/05_control_if.unna
var x = 10;

if (x > 5) {
    print("x is greater than 5");
}

if (x == 10) {
    print("x equals 10");
} else {
    print("x does not equal 10");
}

// examples/basics/06_control_while.unna
var count = 0;
while (count < 5) {
    print("Count: " + count);
    count = count + 1;
}

// examples/basics/07_control_for.unna
for (var i = 0; i < 5; i = i + 1) {
    print("Iteration: " + i);
}

// examples/basics/08_control_foreach.unna
var items = ["apple", "banana", "cherry"];
for (var item : items) {
    print("Item: " + item);
}
```

---

## Next Steps

- [Functions](functions.md) - Reusable code blocks
- [Arrays](arrays.md) - Array iteration patterns
- [Async/Await](async-await.md) - Asynchronous control flow
