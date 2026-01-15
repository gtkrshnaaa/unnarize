# Modules

> Organize code into reusable files.

---

## Import Syntax

Use `import` to load code from another file:

```javascript
import "path/to/module.unna" as moduleName;
```

### Examples

```javascript
// Import from relative path
import "utils/math.unna" as math;
import "models/user.unna" as userModel;
import "services/api.unna" as api;
```

---

## Accessing Imported Code

After importing, access functions and variables using dot notation:

```javascript
// utils/math.unna
function add(a, b) {
    return a + b;
}

function multiply(a, b) {
    return a * b;
}

var PI = 3.14159;
```

```javascript
// main.unna
import "utils/math.unna" as math;

print(math.add(5, 3));       // 8
print(math.multiply(4, 2));  // 8
print(math.PI);              // 3.14159
```

---

## Module Resolution

Paths are resolved relative to the current file:

```
project/
├── main.unna
├── utils/
│   ├── string.unna
│   └── math.unna
├── models/
│   └── user.unna
└── services/
    └── api.unna
```

```javascript
// main.unna
import "utils/string.unna" as str;
import "utils/math.unna" as math;
import "models/user.unna" as user;
```

```javascript
// services/api.unna
import "../models/user.unna" as user;  // Go up one level
```

---

## Module Structure

### Simple Module

```javascript
// utils/greeting.unna
function hello(name) {
    return "Hello, " + name + "!";
}

function goodbye(name) {
    return "Goodbye, " + name + "!";
}
```

### Module with Structs

```javascript
// models/product.unna
struct Product {
    id;
    name;
    price;
}

function createProduct(id, name, price) {
    return Product(id, name, price);
}

function formatPrice(product) {
    return "$" + product.price;
}
```

### Module with State

```javascript
// services/counter.unna
var count = 0;

function increment() {
    count = count + 1;
    return count;
}

function reset() {
    count = 0;
}

function getCount() {
    return count;
}
```

---

## Complete Example

### Project Structure

```
store/
├── main.unna
├── models/
│   ├── product.unna
│   └── order.unna
├── services/
│   └── inventory.unna
└── utils/
    └── format.unna
```

### models/product.unna

```javascript
struct Product {
    id;
    name;
    price;
    stock;
}

function create(id, name, price, stock) {
    return Product(id, name, price, stock);
}

function inStock(product) {
    return product.stock > 0;
}
```

### services/inventory.unna

```javascript
import "../models/product.unna" as product;

var items = [];

function add(id, name, price, stock) {
    push(items, product.create(id, name, price, stock));
}

function findById(id) {
    for (var item : items) {
        if (item.id == id) {
            return item;
        }
    }
    return nil;
}

function list() {
    return items;
}
```

### utils/format.unna

```javascript
function currency(amount) {
    return "$" + amount;
}

function capitalize(str) {
    // Simple implementation
    return str;
}
```

### main.unna

```javascript
import "models/product.unna" as product;
import "services/inventory.unna" as inventory;
import "utils/format.unna" as format;

// Add products
inventory.add(1, "Apple", 0.99, 100);
inventory.add(2, "Banana", 0.49, 150);
inventory.add(3, "Orange", 0.79, 80);

// List all products
var items = inventory.list();
for (var item : items) {
    print(item.name + ": " + format.currency(item.price));
}

// Find specific product
var apple = inventory.findById(1);
if (apple != nil) {
    print("Found: " + apple.name);
}
```

---

## Best Practices

### 1. One Purpose Per Module

```javascript
// Good: Focused module
// utils/validation.unna
function isEmail(str) { ... }
function isPhone(str) { ... }
function isRequired(str) { ... }
```

### 2. Clear Naming

```javascript
// Descriptive alias
import "services/user-authentication.unna" as auth;
import "utils/date-formatting.unna" as dateFormat;
```

### 3. Avoid Circular Imports

```javascript
// Bad: A imports B, B imports A
// This can cause issues

// Good: Extract shared code to a third module
```

### 4. Group Related Functionality

```
project/
├── models/      # Data structures
├── services/    # Business logic
├── utils/       # Helper functions
├── config/      # Configuration
└── main.unna    # Entry point
```

---

## SME System Example

Real-world module organization from the testcase example:

```
testcase/
├── main.unna              # Entry point
├── models/
│   ├── product.unna       # Product struct
│   ├── sale.unna          # Sale struct
│   └── report.unna        # Report struct
├── services/
│   ├── inventory.unna     # Stock management
│   └── sales.unna         # Sales processing
└── utils/
    ├── format.unna        # Formatting helpers
    └── validation.unna    # Input validation
```

```javascript
// main.unna
import "models/product.unna" as product;
import "models/sale.unna" as sale;
import "services/inventory.unna" as inventory;
import "services/sales.unna" as salesService;
import "utils/format.unna" as format;

// Initialize inventory
inventory.add(product.create(1, "Rice 5kg", 12.50, 100));
inventory.add(product.create(2, "Sugar 1kg", 3.20, 200));

// Process sale
var items = [
    sale.createItem(1, 2),
    sale.createItem(2, 5)
];
var receipt = salesService.checkout(items);

print("Total: " + format.currency(receipt.total));
```

---

## Next Steps

- [Async/Await](async-await.md) - Asynchronous module functions
- [Core Libraries](../core-libraries/overview.md) - Built-in modules
- [Examples](../examples/sme-system.md) - Complete application structure
