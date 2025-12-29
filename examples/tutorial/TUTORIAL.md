# Unnarize Tutorial: Zero to Hero

Welcome to the complete Unnarize tutorial! This guide will take you from absolute beginner to building real projects.

## 📚 Table of Contents

1. [Hello World](#1-hello-world)
2. [Variables](#2-variables)
3. [Conditionals](#3-conditionals)
4. [Loops](#4-loops)
5. [Functions](#5-functions)
6. [Arrays](#6-arrays)
7. [Maps](#7-maps)
8. [Structs](#8-structs)
9. [Project: Todo List](#9-project-todo-list)
10. [Project: Banking System](#10-project-banking-system)

---

## Prerequisites

Make sure Unnarize is installed:

```bash
# Build Unnarize
make

# Or install system-wide
sudo make install
```

Run any tutorial file:

```bash
./bin/unnarize examples/tutorial/01_hello_world.unna
```

---

## 1. Hello World

**File**: `examples/tutorial/01_hello_world.unna`

Learn the basics: printing output, creating variables, and string concatenation.

**What you'll learn**:
- Using `print()` to display output
- Creating variables with `var`
- String concatenation with `+`
- Working with numbers and text

**Code**:

```c
print("Hello, Unnarize!");
print("Welcome to your first program!");

var name = "World";
print("Hello, " + name + "!");

var x = 42;
var y = 3.14;
var message = "The answer is";

print(message + " " + x);
print("Pi is approximately " + y);
```

**Run it**:
```bash
./bin/unnarize examples/tutorial/01_hello_world.unna
```

**Expected output**:
```
Hello, Unnarize!
Welcome to your first program!
Hello, World!
The answer is 42
Pi is approximately 3.14
```

---

## 2. Variables

**File**: `examples/tutorial/02_variables.unna`

Master variables and arithmetic operations.

**What you'll learn**:
- Different data types (integers, floats, strings, booleans)
- Arithmetic operators (`+`, `-`, `*`, `/`)
- Variable reassignment
- Compound assignment (`+=`)

**Code**:

```c
var age = 25;
var price = 99.99;
var name = "Alice";
var isActive = true;

print("Age: " + age);
print("Price: $" + price);
print("Name: " + name);
print("Active: " + isActive);

var sum = 10 + 5;
var difference = 20 - 8;
var product = 6 * 7;
var quotient = 100 / 4;

print("Sum: " + sum);
print("Difference: " + difference);
print("Product: " + product);
print("Quotient: " + quotient);

var a = 10;
a = a + 5;
print("a after addition: " + a);

a += 10;
print("a after += : " + a);
```

**Run it**:
```bash
./bin/unnarize examples/tutorial/02_variables.unna
```

---

## 3. Conditionals

**File**: `examples/tutorial/03_conditionals.unna`

Control program flow with if/else statements.

**What you'll learn**:
- `if`, `else if`, `else` statements
- Comparison operators (`>`, `<`, `>=`, `<=`, `==`)
- Logical operators (`and`, `or`, `not`)
- Boolean expressions

**Code**:

```c
var temperature = 25;

if (temperature > 30) {
    print("It's hot!");
} else if (temperature > 20) {
    print("It's warm!");
} else {
    print("It's cold!");
}

var score = 85;
var passed = score >= 60;

if (passed) {
    print("Congratulations! You passed.");
} else {
    print("Sorry, you failed.");
}

var age = 18;
var hasLicense = true;

if (age >= 18 and hasLicense) {
    print("You can drive!");
} else {
    print("You cannot drive yet.");
}

var isWeekend = false;
var isHoliday = true;

if (isWeekend or isHoliday) {
    print("Time to relax!");
} else {
    print("Time to work!");
}
```

**Run it**:
```bash
./bin/unnarize examples/tutorial/03_conditionals.unna
```

---

## 4. Loops

**File**: `examples/tutorial/04_loops.unna`

Repeat code with while and for loops.

**What you'll learn**:
- `while` loops
- `for` loops
- Loop counters and iteration
- Practical loop applications

**Code**:

```c
print("=== While Loop ===");
var count = 1;
while (count <= 5) {
    print("Count: " + count);
    count = count + 1;
}

print("");
print("=== For Loop ===");
for (var i = 1; i <= 5; i = i + 1) {
    print("i = " + i);
}

print("");
print("=== Countdown ===");
for (var num = 10; num >= 1; num = num - 1) {
    print(num);
}
print("Blast off!");

print("");
print("=== Multiplication Table ===");
var number = 7;
for (var i = 1; i <= 10; i = i + 1) {
    var result = number * i;
    print(number + " x " + i + " = " + result);
}

print("");
print("=== Sum of Numbers ===");
var sum = 0;
for (var i = 1; i <= 100; i = i + 1) {
    sum = sum + i;
}
print("Sum of 1 to 100: " + sum);
```

**Run it**:
```bash
./bin/unnarize examples/tutorial/04_loops.unna
```

---

## 5. Functions

**File**: `examples/tutorial/05_functions.unna`

Create reusable code with functions.

**What you'll learn**:
- Defining functions
- Function parameters
- Return values
- Recursion

**Code**:

```c
function greet() {
    print("Hello from a function!");
}

greet();
greet();

function sayHello(name) {
    print("Hello, " + name + "!");
}

sayHello("Alice");
sayHello("Bob");

function add(a, b) {
    return a + b;
}

var result = add(10, 20);
print("10 + 20 = " + result);

function multiply(x, y) {
    return x * y;
}

var product = multiply(6, 7);
print("6 * 7 = " + product);

function isEven(number) {
    return number % 2 == 0;
}

print("Is 4 even? " + isEven(4));
print("Is 7 even? " + isEven(7));

function factorial(n) {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

print("Factorial of 5: " + factorial(5));
print("Factorial of 6: " + factorial(6));

function fibonacci(n) {
    if (n <= 1) {
        return n;
    }
    return fibonacci(n - 1) + fibonacci(n - 2);
}

print("Fibonacci(10): " + fibonacci(10));
```

**Run it**:
```bash
./bin/unnarize examples/tutorial/05_functions.unna
```

---

## 6. Arrays

**File**: `examples/tutorial/06_arrays.unna`

Work with ordered collections of data.

**What you'll learn**:
- Creating arrays with `[]` syntax
- Accessing elements by index
- Modifying array elements
- Array functions: `push()`, `pop()`, `length()`
- Iterating with for loops and for-each
- Nested arrays

**Code**:

```c
print("=== Creating Arrays ===");
var numbers = [1, 2, 3, 4, 5];
var fruits = ["apple", "banana", "orange"];
var mixed = [42, "hello", 3.14, true];

print("Numbers: " + numbers);
print("Fruits: " + fruits);

print("");
print("=== Accessing Elements ===");
print("First number: " + numbers[0]);
print("Second fruit: " + fruits[1]);
print("Last number: " + numbers[4]);

print("");
print("=== Modifying Arrays ===");
fruits[1] = "mango";
print("After change: " + fruits);

print("");
print("=== Array Functions ===");
var items = [];
push(items, "first");
push(items, "second");
push(items, "third");
print("After pushes: " + items);
print("Length: " + length(items));

var last = pop(items);
print("Popped: " + last);
print("Remaining: " + items);

print("");
print("=== Iterating Arrays ===");
var colors = ["red", "green", "blue"];

print("Using for loop:");
for (var i = 0; i < length(colors); i = i + 1) {
    print("  " + i + ": " + colors[i]);
}

print("Using for-each:");
for (var color : colors) {
    print("  Color: " + color);
}

print("");
print("=== Nested Arrays ===");
var matrix = [
    [1, 2, 3],
    [4, 5, 6],
    [7, 8, 9]
];

print("matrix[0][0] = " + matrix[0][0]);
print("matrix[1][2] = " + matrix[1][2]);
print("matrix[2][1] = " + matrix[2][1]);

print("");
print("=== Practical Example ===");
var scores = [85, 92, 78, 95, 88];
var sum = 0;

for (var score : scores) {
    sum = sum + score;
}

var average = sum / length(scores);
print("Average score: " + average);
```

**Run it**:
```bash
./bin/unnarize examples/tutorial/06_arrays.unna
```

---

## 7. Maps

**File**: `examples/tutorial/07_maps.unna`

Store key-value pairs with maps (dictionaries).

**What you'll learn**:
- Creating maps with `map()`
- String and integer keys
- Map functions: `has()`, `keys()`, `delete()`
- Iterating over maps

**Code**:

```c
print("=== Creating Maps ===");
var user = map();
user["name"] = "Alice";
user["age"] = 25;
user["email"] = "alice@example.com";

print("Name: " + user["name"]);
print("Age: " + user["age"]);
print("Email: " + user["email"]);

print("");
print("=== Integer Keys ===");
var data = map();
data[1] = "first";
data[2] = "second";
data[100] = "hundredth";

print("data[1]: " + data[1]);
print("data[100]: " + data[100]);

print("");
print("=== Map Functions ===");
var config = map();
config["debug"] = true;
config["port"] = 8080;
config["host"] = "localhost";

print("Has 'debug': " + has(config, "debug"));
print("Has 'timeout': " + has(config, "timeout"));

var allKeys = keys(config);
print("All keys: " + allKeys);

delete(config, "port");
print("After deleting 'port':");
print("Has 'port': " + has(config, "port"));

print("");
print("=== Iterating Maps ===");
var scores = map();
scores["Alice"] = 95;
scores["Bob"] = 87;
scores["Charlie"] = 92;

var names = keys(scores);
for (var name : names) {
    print(name + ": " + scores[name]);
}

print("");
print("=== Practical Example ===");
var inventory = map();
inventory["apples"] = 50;
inventory["oranges"] = 30;
inventory["bananas"] = 45;

print("=== Inventory ===");
var items = keys(inventory);
for (var item : items) {
    var quantity = inventory[item];
    print(item + ": " + quantity + " units");
}

inventory["apples"] = inventory["apples"] - 10;
print("");
print("After selling 10 apples:");
print("Apples remaining: " + inventory["apples"]);
```

**Run it**:
```bash
./bin/unnarize examples/tutorial/07_maps.unna
```

---

## 8. Structs

**File**: `examples/tutorial/08_structs.unna`

Define custom data types with structs.

**What you'll learn**:
- Defining structs
- Creating struct instances
- Accessing and modifying fields
- Nested structs
- Structs in arrays

**Code**:

```c
print("=== Defining Structs ===");
struct Point {
    x;
    y;
}

var p1 = Point(10, 20);
print("Point: (" + p1.x + ", " + p1.y + ")");

p1.x = 30;
p1.y = 40;
print("Updated: (" + p1.x + ", " + p1.y + ")");

print("");
print("=== Struct with Multiple Fields ===");
struct Person {
    name;
    age;
    email;
}

var alice = Person("Alice", 25, "alice@example.com");
print("Name: " + alice.name);
print("Age: " + alice.age);
print("Email: " + alice.email);

alice.age = alice.age + 1;
print("After birthday: " + alice.age);

print("");
print("=== Nested Structs ===");
struct Address {
    street;
    city;
    zipcode;
}

struct Employee {
    name;
    position;
    address;
}

var addr = Address("123 Main St", "New York", "10001");
var emp = Employee("Bob", "Developer", addr);

print("Employee: " + emp.name);
print("Position: " + emp.position);
print("City: " + emp.address.city);
print("Street: " + emp.address.street);

print("");
print("=== Structs in Arrays ===");
struct Product {
    id;
    name;
    price;
}

var products = [
    Product(1, "Laptop", 999),
    Product(2, "Mouse", 25),
    Product(3, "Keyboard", 75)
];

print("=== Product Catalog ===");
for (var product : products) {
    print("ID: " + product.id);
    print("  Name: " + product.name);
    print("  Price: $" + product.price);
}

print("");
print("=== Calculating Total ===");
var total = 0;
for (var product : products) {
    total = total + product.price;
}
print("Total value: $" + total);
```

**Run it**:
```bash
./bin/unnarize examples/tutorial/08_structs.unna
```

---

## 9. Project: Todo List

**File**: `examples/tutorial/09_project_todo_list.unna`

Build a complete todo list manager combining everything you've learned.

**Features**:
- Add tasks
- Mark tasks as completed
- List all tasks with status
- Count completed tasks

**What you'll practice**:
- Structs for data modeling
- Arrays for storage
- Functions for operations
- Conditionals for logic
- Loops for iteration

**Code**:

```c
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
    print("");
    print("=== Task List ===");
    if (length(tasks) == 0) {
        print("No tasks yet!");
        return;
    }
    
    for (var task : tasks) {
        var status = "[ ]";
        if (task.completed) {
            status = "[x]";
        }
        print(status + " " + task.id + ". " + task.title);
    }
}

function completeTask(taskId) {
    for (var task : tasks) {
        if (task.id == taskId) {
            task.completed = true;
            print("Completed: " + task.title);
            return;
        }
    }
    print("Task not found!");
}

function countCompleted() {
    var count = 0;
    for (var task : tasks) {
        if (task.completed) {
            count = count + 1;
        }
    }
    return count;
}

print("=== Todo List Manager ===");

addTask("Learn Unnarize basics");
addTask("Build a simple program");
addTask("Read documentation");
addTask("Create a project");

listTasks();

print("");
print("Completing some tasks...");
completeTask(1);
completeTask(3);

listTasks();

print("");
var completed = countCompleted();
var total = length(tasks);
var remaining = total - completed;
print("Progress: " + completed + "/" + total + " completed");
print("Remaining: " + remaining + " tasks");
```

**Run it**:
```bash
./bin/unnarize examples/tutorial/09_project_todo_list.unna
```

**Expected output**:
```
=== Todo List Manager ===
Added: Learn Unnarize basics
Added: Build a simple program
Added: Read documentation
Added: Create a project

=== Task List ===
[ ] 1. Learn Unnarize basics
[ ] 2. Build a simple program
[ ] 3. Read documentation
[ ] 4. Create a project

Completing some tasks...
Completed: Learn Unnarize basics
Completed: Read documentation

=== Task List ===
[x] 1. Learn Unnarize basics
[ ] 2. Build a simple program
[x] 3. Read documentation
[ ] 4. Create a project

Progress: 2/4 completed
Remaining: 2 tasks
```

---

## 10. Project: Banking System

**File**: `examples/tutorial/10_project_banking_system.unna`

Build a complete banking system with accounts, deposits, withdrawals, and transfers.

**Features**:
- Create bank accounts
- Deposit money
- Withdraw money
- Transfer between accounts
- Check balance
- Error handling

**What you'll practice**:
- Maps for account storage
- Structs for account data
- Functions for operations
- Conditionals for validation
- Complex business logic

**Code**:

```c
struct BankAccount {
    accountNumber;
    holderName;
    balance;
}

var accounts = map();
var nextAccountNumber = 1000;

function createAccount(name, initialDeposit) {
    var accNum = nextAccountNumber;
    nextAccountNumber = nextAccountNumber + 1;
    
    var account = BankAccount(accNum, name, initialDeposit);
    accounts[accNum] = account;
    
    print("Account created successfully!");
    print("Account Number: " + accNum);
    print("Holder: " + name);
    print("Initial Balance: $" + initialDeposit);
    print("");
    
    return accNum;
}

function deposit(accountNumber, amount) {
    if (!has(accounts, accountNumber)) {
        print("Error: Account not found!");
        return;
    }
    
    var account = accounts[accountNumber];
    account.balance = account.balance + amount;
    
    print("Deposited $" + amount);
    print("New balance: $" + account.balance);
    print("");
}

function withdraw(accountNumber, amount) {
    if (!has(accounts, accountNumber)) {
        print("Error: Account not found!");
        return;
    }
    
    var account = accounts[accountNumber];
    
    if (account.balance < amount) {
        print("Error: Insufficient funds!");
        print("Current balance: $" + account.balance);
        print("");
        return;
    }
    
    account.balance = account.balance - amount;
    print("Withdrew $" + amount);
    print("New balance: $" + account.balance);
    print("");
}

function checkBalance(accountNumber) {
    if (!has(accounts, accountNumber)) {
        print("Error: Account not found!");
        return;
    }
    
    var account = accounts[accountNumber];
    print("Account: " + accountNumber);
    print("Holder: " + account.holderName);
    print("Balance: $" + account.balance);
    print("");
}

function transfer(fromAccount, toAccount, amount) {
    if (!has(accounts, fromAccount) or !has(accounts, toAccount)) {
        print("Error: One or both accounts not found!");
        return;
    }
    
    var from = accounts[fromAccount];
    var to = accounts[toAccount];
    
    if (from.balance < amount) {
        print("Error: Insufficient funds for transfer!");
        return;
    }
    
    from.balance = from.balance - amount;
    to.balance = to.balance + amount;
    
    print("Transfer successful!");
    print("From: " + from.holderName + " (Account " + fromAccount + ")");
    print("To: " + to.holderName + " (Account " + toAccount + ")");
    print("Amount: $" + amount);
    print("");
}

print("=== Banking System Demo ===");
print("");

var acc1 = createAccount("Alice Johnson", 1000);
var acc2 = createAccount("Bob Smith", 500);

checkBalance(acc1);
checkBalance(acc2);

deposit(acc1, 250);
withdraw(acc2, 100);

transfer(acc1, acc2, 300);

print("=== Final Balances ===");
checkBalance(acc1);
checkBalance(acc2);
```

**Run it**:
```bash
./bin/unnarize examples/tutorial/10_project_banking_system.unna
```

**Expected output**:
```
=== Banking System Demo ===

Account created successfully!
Account Number: 1000
Holder: Alice Johnson
Initial Balance: $1000

Account created successfully!
Account Number: 1001
Holder: Bob Smith
Initial Balance: $500

Account: 1000
Holder: Alice Johnson
Balance: $1000

Account: 1001
Holder: Bob Smith
Balance: $500

Deposited $250
New balance: $1250

Withdrew $100
New balance: $400

Transfer successful!
From: Alice Johnson (Account 1000)
To: Bob Smith (Account 1001)
Amount: $300

=== Final Balances ===
Account: 1000
Holder: Alice Johnson
Balance: $950

Account: 1001
Holder: Bob Smith
Balance: $700
```

---

## 🎓 Next Steps

Congratulations! You've completed the Unnarize tutorial. Here's what to do next:

### 1. Explore More Examples
Check out `examples/` directory for more advanced programs:
- `examples/02_structures.unna` - Data structures
- `examples/05_modules.unna` - Module system
- `examples/08_structs.unna` - Advanced structs

### 2. Read Documentation
- Main README: Project overview and features
- `index.html`: Complete language reference
- `examples/benchmarks/`: Performance testing

### 3. Build Your Own Projects
Ideas to practice:
- Calculator
- Contact manager
- Inventory system
- Game (number guessing, tic-tac-toe)
- Data processor

### 4. Contribute
- Report bugs
- Suggest features
- Share your projects
- Improve documentation

---

## 📝 Quick Reference

### Data Types
- **Integer**: `42`
- **Float**: `3.14`
- **String**: `"hello"`
- **Boolean**: `true`, `false`

### Control Flow
- **If**: `if (condition) { }`
- **While**: `while (condition) { }`
- **For**: `for (var i = 0; i < 10; i = i + 1) { }`
- **For-each**: `for (var item : array) { }`

### Data Structures
- **Array**: `[1, 2, 3]`
- **Map**: `map()`
- **Struct**: `struct Name { field; }`

### Built-in Functions
- **Output**: `print(value)`
- **Array**: `push()`, `pop()`, `length()`
- **Map**: `has()`, `keys()`, `delete()`

---

## 💡 Tips

1. **Start small**: Master basics before complex projects
2. **Experiment**: Modify tutorial code to see what happens
3. **Read errors**: Error messages help you learn
4. **Practice**: Build small programs regularly
5. **Ask questions**: Community is here to help

---

**Happy coding with Unnarize!** 🚀

*Tutorial created: 2025-12-29*  
*Unnarize version: 0.1.1-beta*
