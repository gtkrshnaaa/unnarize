# ucoreUon

> UON (Unnarize Object Notation) data format parser.

---

## Overview

UON is a simple, line-based data format designed for Unnarize. It's similar to CSV but with more structure, supporting typed fields and schemas.

---

## UON File Format

### Basic Structure

```
# Comments start with #
@schema name:string, age:int, active:bool

John Doe, 30, true
Jane Smith, 25, false
Bob Wilson, 45, true
```

### Field Types

| Type | Description | Example |
|------|-------------|---------|
| `string` | Text value | `"hello"`, `Alice` |
| `int` | Integer | `42`, `-10` |
| `float` | Decimal | `3.14`, `-0.5` |
| `bool` | Boolean | `true`, `false` |

---

## API Reference

| Function | Returns | Description |
|----------|---------|-------------|
| `parse(content)` | map | Parse UON string to data |
| `load(path)` | cursor | Load UON file for streaming |
| `get(path)` | array | Load and get all records |
| `next(cursor)` | map | Read next record from cursor |
| `generate(schema, data)` | string | Generate UON content |
| `save(data)` | bool | Save data (placeholder) |
| `insert(key, value)` | nil | Insert data (placeholder) |

---

## Reading UON Files

### Load and Read All

```javascript
var records = ucoreUon.get("data.uon");

for (var record : records) {
    print(record["name"] + ": " + record["age"]);
}
```

### Streaming with next()

```javascript
var cursor = ucoreUon.load("large_data.uon");

var record = ucoreUon.next(cursor);
while (record != nil) {
    processRecord(record);
    record = ucoreUon.next(cursor);
}
```

### Parsing UON String

```javascript
var content = "@schema name:string, age:int\nAlice, 30\nBob, 25";
var data = ucoreUon.parse(content);
print(data);
```

---

## Generating UON Content

### generate(schema, data)

```javascript
var schema = "name:string, age:int, active:bool";

var users = [];
var user1 = map();
user1["name"] = "Alice";
user1["age"] = 30;
user1["active"] = true;
push(users, user1);

var user2 = map();
user2["name"] = "Bob";
user2["age"] = 25;
user2["active"] = false;
push(users, user2);

var content = ucoreUon.generate(schema, users);
print(content);
// @schema name:string, age:int, active:bool
// Alice, 30, true
// Bob, 25, false
```

---

## Complete Example

### Data File (products.uon)

```
# Product catalog
@schema id:int, name:string, price:float, stock:int

1, Apple, 0.99, 100
2, Banana, 0.49, 150
3, Orange, 0.79, 80
4, Milk, 2.50, 50
5, Bread, 1.99, 30
```

### Reading Products

```javascript
function loadProducts() {
    var cursor = ucoreUon.open("products.uon");
    var products = ucoreUon.all(cursor);
    ucoreUon.close(cursor);
    return products;
}

function findProduct(products, id) {
    for (var p : products) {
        if (p["id"] == id) {
            return p;
        }
    }
    return nil;
}

function lowStock(products, threshold) {
    var result = [];
    for (var p : products) {
        if (p["stock"] < threshold) {
            push(result, p);
        }
    }
    return result;
}

// Usage
var products = loadProducts();

print("All Products:");
for (var p : products) {
    print(p["name"] + ": $" + p["price"] + " (qty: " + p["stock"] + ")");
}

print("\nLow Stock (<60):");
var low = lowStock(products, 60);
for (var p : low) {
    print(p["name"] + ": " + p["stock"]);
}
```

---

## Common Patterns

### Database-Like Operations

```javascript
// Load data
var cursor = ucoreUon.open("users.uon");
var users = ucoreUon.all(cursor);
ucoreUon.close(cursor);

// Filter
function findByAge(minAge) {
    var result = [];
    for (var user : users) {
        if (user["age"] >= minAge) {
            push(result, user);
        }
    }
    return result;
}

// Update
function updateUser(id, updates) {
    for (var i = 0; i < length(users); i = i + 1) {
        if (users[i]["id"] == id) {
            for (var key : keys(updates)) {
                users[i][key] = updates[key];
            }
            break;
        }
    }
}

// Save back
function saveUsers() {
    ucoreUon.write("users.uon", users);
}
```

### Import/Export

```javascript
// Import from CSV-like format
function importData(sourcePath, destPath) {
    var lines = ucoreString.split(ucoreSystem.readFile(sourcePath), "\n");
    var records = [];
    
    for (var line : lines) {
        if (ucoreString.trim(line) == "") continue;
        
        var parts = ucoreString.split(line, ",");
        var record = map();
        record["name"] = ucoreString.trim(parts[0]);
        record["value"] = ucoreString.trim(parts[1]);
        push(records, record);
    }
    
    ucoreUon.write(destPath, records);
}

// Export to JSON
function exportToJson(uonPath, jsonPath) {
    var cursor = ucoreUon.open(uonPath);
    var records = ucoreUon.all(cursor);
    ucoreUon.close(cursor);
    
    ucoreJson.writeFile(jsonPath, records);
}
```

### Streaming Large Files

```javascript
function processLargeFile(path, batchSize) {
    var cursor = ucoreUon.open(path);
    var batch = [];
    var count = 0;
    
    var record = ucoreUon.next(cursor);
    while (record != nil) {
        push(batch, record);
        count = count + 1;
        
        if (length(batch) >= batchSize) {
            processBatch(batch);
            batch = [];
        }
        
        record = ucoreUon.next(cursor);
    }
    
    // Process remaining
    if (length(batch) > 0) {
        processBatch(batch);
    }
    
    ucoreUon.close(cursor);
    print("Processed " + count + " records");
}

function processBatch(records) {
    print("Processing batch of " + length(records) + " records");
    // Do work...
}
```

---

## Examples

```javascript
// examples/corelib/uon/demo.unna
print("=== UON Demo ===");

// Create sample data
var items = [];

var item1 = map();
item1["id"] = 1;
item1["name"] = "Widget";
item1["price"] = 9.99;
push(items, item1);

var item2 = map();
item2["id"] = 2;
item2["name"] = "Gadget";
item2["price"] = 19.99;
push(items, item2);

// Write to file
ucoreUon.write("items.uon", items);
print("Wrote " + length(items) + " items");

// Read back
var cursor = ucoreUon.open("items.uon");
var loaded = ucoreUon.all(cursor);
ucoreUon.close(cursor);

print("Loaded " + length(loaded) + " items:");
for (var item : loaded) {
    print("  " + item["name"] + ": $" + item["price"]);
}
```

---

## Next Steps

- [ucoreJson](ucore-json.md) - Alternative data format
- [ucoreSystem](ucore-system.md) - File operations
- [Overview](overview.md) - All libraries
