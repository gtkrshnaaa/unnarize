# Structs

> Custom data structures with named fields.

---

## Defining Structs

Use the `struct` keyword to define a new data type:

```javascript
struct Person {
    name;
    age;
    email;
}

struct Point {
    x;
    y;
}

struct Product {
    id;
    name;
    price;
    stock;
}
```

Fields are listed by name, separated by semicolons.

---

## Creating Instances

Call the struct name as a function, passing values for each field in order:

```javascript
struct User {
    id;
    name;
    active;
}

var user = User(1, "Alice", true);

print(user.id);      // 1
print(user.name);    // "Alice"
print(user.active);  // true
```

---

## Accessing Fields

Use dot notation to access field values:

```javascript
struct Point {
    x;
    y;
}

var p = Point(10, 20);

print(p.x);  // 10
print(p.y);  // 20

// Calculate distance
var distance = sqrt(p.x * p.x + p.y * p.y);
print(distance);  // 22.36...
```

---

## Modifying Fields

```javascript
struct Counter {
    value;
}

var counter = Counter(0);

counter.value = counter.value + 1;
print(counter.value);  // 1

counter.value += 10;
print(counter.value);  // 11
```

---

## Structs in Functions

### Passing Structs

```javascript
struct Rectangle {
    width;
    height;
}

function area(rect) {
    return rect.width * rect.height;
}

function perimeter(rect) {
    return 2 * (rect.width + rect.height);
}

var r = Rectangle(10, 5);
print("Area: " + area(r));          // 50
print("Perimeter: " + perimeter(r)); // 30
```

### Returning Structs

```javascript
struct Point {
    x;
    y;
}

function createPoint(x, y) {
    return Point(x, y);
}

function add(p1, p2) {
    return Point(p1.x + p2.x, p1.y + p2.y);
}

var a = createPoint(1, 2);
var b = createPoint(3, 4);
var c = add(a, b);

print(c.x + ", " + c.y);  // 4, 6
```

---

## Nested Structs

Structs can contain other structs:

```javascript
struct Address {
    street;
    city;
    zip;
}

struct Person {
    name;
    address;
}

var addr = Address("123 Main St", "Springfield", "12345");
var person = Person("Alice", addr);

print(person.name);              // "Alice"
print(person.address.city);      // "Springfield"
print(person.address.zip);       // "12345"
```

---

## Arrays of Structs

```javascript
struct Todo {
    id;
    text;
    done;
}

var todos = [
    Todo(1, "Buy groceries", false),
    Todo(2, "Write code", true),
    Todo(3, "Exercise", false)
];

// List incomplete todos
for (var todo : todos) {
    if (!todo.done) {
        print(todo.id + ": " + todo.text);
    }
}
// Output:
// 1: Buy groceries
// 3: Exercise
```

---

## Common Patterns

### Factory Function

```javascript
struct User {
    id;
    name;
    role;
}

var nextId = 0;

function createUser(name, role) {
    nextId = nextId + 1;
    return User(nextId, name, role);
}

var admin = createUser("Alice", "admin");
var guest = createUser("Bob", "guest");

print(admin.id + ": " + admin.name);  // 1: Alice
print(guest.id + ": " + guest.name);  // 2: Bob
```

### Builder Pattern

```javascript
struct Config {
    host;
    port;
    debug;
}

function defaultConfig() {
    return Config("localhost", 8080, false);
}

var config = defaultConfig();
config.debug = true;
config.port = 3000;

print(config.host + ":" + config.port);  // localhost:3000
```

### Data Transfer Object

```javascript
struct Response {
    status;
    data;
    error;
}

function success(data) {
    return Response(200, data, nil);
}

function error(message) {
    return Response(500, nil, message);
}

var res = success("User created");
if (res.status == 200) {
    print("Success: " + res.data);
}
```

---

## Comparison with Other Data Structures

| Feature | Struct | Array | Map |
|---------|--------|-------|-----|
| Named fields | ✅ | ❌ | ✅ |
| Ordered | ✅ | ✅ | ❌ |
| Fixed fields | ✅ | ❌ | ❌ |
| Type definition | ✅ | ❌ | ❌ |
| Dot access | ✅ | ❌ | ❌ |

---

## Examples

```javascript
// examples/basics/04_structs.unna
struct User {
    id;
    name;
    email;
}

var u = User(1, "Alice", "alice@example.com");

print("ID: " + u.id);
print("Name: " + u.name);
print("Email: " + u.email);

// Modify field
u.email = "alice.new@example.com";
print("Updated email: " + u.email);

// Nested struct
struct Department {
    name;
    manager;
}

var dev = Department("Engineering", u);
print("Department: " + dev.name);
print("Manager: " + dev.manager.name);
```

---

## Next Steps

- [Control Flow](control-flow.md) - Conditionals and loops
- [Functions](functions.md) - Work with structs in functions
- [Modules](modules.md) - Organize struct definitions
