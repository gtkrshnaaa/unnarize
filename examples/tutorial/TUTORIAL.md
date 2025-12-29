# 📘 Unnarize Tutorial: Zero to Hero

Welcome to the comprehensive guide for **Unnarize**, a dynamic C-style scripting language built for performance and simplicity. This tutorial will take you from printing "Hello World" to building a web server and banking system.

> **How to use this tutorial**:
> All code examples below are complete and runnable. You can copy them into a file (e.g., `script.unna`) and run them using:
> ```bash
> ./bin/unnarize script.unna
> ```

---

## 1. Basics

### Hello World
The journey begins with a single line.

```javascript
print("Hello, World!");
```

### Variables & Data Types
Unnarize is dynamically typed. Use `var` to declare variables.

```javascript
var age = 25;
var price = 99.99;
var name = "Alice";
var isActive = true;

print("Age: " + age);
print("Price: $" + price);
print("Name: " + name);
print("Active: " + isActive);

// Math operations
var sum = 10 + 5;
print("Sum: " + sum);

// Compound assignment
var a = 10;
a += 5;
print("a after += : " + a);
```

### Conditionals (If/Else)
Control the flow of your program logic.

```javascript
var temperature = 25;

if (temperature > 30) {
    print("It's hot!");
} else if (temperature > 20) {
    print("It's warm!");
} else {
    print("It's cold!");
}

var score = 85;
if (score >= 60) {
    print("Passed!");
} else {
    print("Failed.");
}

// Logical Operators: && (AND), || (OR)
var weekend = false;
var holiday = true;
if (weekend || holiday) {
    print("Time to relax!");
}
```

### Loops
Repeat actions using `while` and `for` loops.

```javascript
print("=== While Loop ===");
var count = 1;
while (count <= 3) {
    print(count);
    count = count + 1;
}

print("=== For Loop ===");
for (var i = 1; i <= 3; i = i + 1) {
    print("i = " + i);
}

print("=== Countdown ===");
for (var n = 3; n > 0; n = n - 1) {
    print(n + "...");
}
print("Liftoff!");
```

### Functions
Encapsulate logic for reuse.

```javascript
function greet(name) {
    return "Hello, " + name + "!";
}

print(greet("Alice"));

// Recursion is fully supported
function factorial(n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

print("Factorial of 5: " + factorial(5));
```

### Async & Await (First-Class Support)
Unnarize treats asynchronous programming as a first-class citizen. You can mark functions as `async` and use `await` to handle results cleanly.

```javascript
async function compute() {
    print("Async task running...");
    return "Done";
}

// Await directly gets the result
var result = await compute();
print("Async Result: " + result);
```

---

## 2. Data Structures

### Arrays
Lists of ordered values.

```javascript
var fruits = ["apple", "banana", "cherry"];
print("First fruit: " + fruits[0]);

// Modifying
fruits[1] = "mango";
push(fruits, "date");
print("Fruits: " + fruits);

// Iteration
print("--- List ---");
for (var f : fruits) {
    print("- " + f);
}

// Length and Pop
print("Total: " + length(fruits));
var last = pop(fruits);
print("Removed: " + last);
```

### Maps
Key-value pairs (dictionaries).

```javascript
var user = map();
user["name"] = "Alice";
user["role"] = "Admin";
user["active"] = true;

print(user["name"] + " is " + user["role"]);

// Check keys
if (has(user, "email")) {
    print("Email found");
} else {
    print("No email");
}

// Iterate keys
var props = keys(user);
for (var prop : props) {
    print(prop + ": " + user[prop]);
}
```

### Structs
Custom data types for structured data.

```javascript
struct Point {
    x;
    y;
}

var p = Point(10, 20);
print("Point: " + p.x + ", " + p.y);

// Nested Structs
struct Circle {
    center;
    radius;
}

var c = Circle(p, 5.5);
print("Circle radius: " + c.radius);
print("Center X: " + c.center.x);
```

---

## 3. Real Projects

### Project 1: To-Do List Manager
A complete command-line task manager.

```javascript
struct Task {
    id;
    title;
    completed;
}

var tasks = [];
var nextId = 1;

function addTask(title) {
    var task = Task(nextId, title, false);
    push(tasks, task);
    nextId = nextId + 1;
    print("Added: " + title);
}

function listTasks() {
    print("\n=== Tasks ===");
    for (var task : tasks) {
        var status = "[ ]";
        if (task.completed) status = "[x]";
        print(status + " " + task.id + ". " + task.title);
    }
}

function complete(id) {
    for (var task : tasks) {
        if (task.id == id) {
            task.completed = true;
            print("Completed: " + task.title);
            return;
        }
    }
}

// Usage
addTask("Learn Unnarize");
addTask("Build App");
listTasks();
complete(1);
listTasks();
```

---

## 4. Core Libraries
Unnarize comes with powerful built-in modules.

### System Module (`ucoreSystem`)
Interact with the OS.

```javascript
// Arguments
var args = ucoreSystem.args();
print("Args: " + args);

// Environment
var user = ucoreSystem.getenv("USER");
print("User: " + user);

// Filesystem
if (ucoreSystem.fileExists("config.txt")) {
    print("Config found.");
} else {
    print("Config missing.");
}
```

### Timer Module (`ucoreTimer`)
High-precision timing.

```javascript
var start = ucoreTimer.now();

// Simulate work
var sum = 0;
for (var i=0; i<100000; i=i+1) sum = sum + i;

var end = ucoreTimer.now();
print("Work done in " + (end - start) + "ms");
```

### HTTP Server (`ucoreHttp`)
Build web APIs in seconds.

**Server Script:**
```javascript
print("Starting server on port 8080...");

function home(req) {
    return "Welcome to Unnarize!";
}

function api(req) {
    var data = map();
    data["status"] = "ok";
    data["time"] = ucoreTimer.now();
    return ucoreHttp.json(data);
}

ucoreHttp.route("GET", "/", "home");
ucoreHttp.route("GET", "/api", "api");

// Blocks indefinitely
ucoreHttp.listen(8080);
```

### UON Database (`ucoreUon`)
Unnarize Object Notation - a streaming database format.

**1. Create a data file (`data.uon`)**:
```uon
@schema {
    users: [id, name, active]
}
@flow {
    users: [
        { id: 1, name: "Alice", active: true },
        { id: 2, name: "Bob", active: false }
    ]
}
```

**2. Script to read it**:
```javascript
ucoreUon.load("data.uon");
var cursor = ucoreUon.get("users");

var user = ucoreUon.next(cursor);
while (user) {
    print("User: " + user["name"]);
    user = ucoreUon.next(cursor);
}
```

---

## 5. Modularity (Syntax)
Organize code into files.

> **Note**: This demonstrates the standard syntax.

**`math.unna`**:
```javascript
function add(a, b) {
    return a + b;
}
```

**`main.unna`**:
```javascript
import math as m;

var res = m.add(10, 5);
print(res);
```

---

## 🚀 Congratulations!
You have completed the Unnarize tutorial. You now understand variables, functions, data structures, and how to use the powerful core libraries to build real applications.
