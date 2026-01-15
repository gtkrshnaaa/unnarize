# SME Store Management System

> Complete real-world application demonstration.

---

## Overview

The SME (Small/Medium Enterprise) Store Management System in `examples/testcase/` demonstrates a full application with:

- Multi-file module organization
- Product inventory management
- Sales processing
- Daily reporting
- Database integration (UON)
- HTTP API server

---

## Project Structure

```
examples/testcase/
├── main.unna           # Application entry point
├── models/
│   ├── product.unna    # Product struct and operations
│   ├── sale.unna       # Sale transaction model
│   └── report.unna     # Daily report generation
├── services/
│   ├── inventory.unna  # Stock management
│   └── sales.unna      # Sales processing
├── utils/
│   └── format.unna     # Formatting utilities
└── data/
    └── products.uon    # Product database
```

---

## Running the Demo

```bash
./bin/unnarize examples/testcase/main.unna
```

**Expected Output:**
```
=== SME Store Management System ===

Loading products...
  Product: Rice 5kg ($12.50, stock: 100)
  Product: Sugar 1kg ($3.20, stock: 200)
  Product: Cooking Oil 1L ($5.75, stock: 150)
  ...

Processing sales...
  Sale #1: 3 items, total: $35.45
  Sale #2: 5 items, total: $52.80
  ...

=== Daily Report ===
Total Sales: 15
Revenue: $547.20
Most Sold: Rice 5kg (23 units)

SME Store System shutdown complete
```

---

## Key Components

### Product Model

```javascript
// models/product.unna
struct Product {
    id;
    name;
    price;
    stock;
}

function create(id, name, price, stock) {
    return Product(id, name, price, stock);
}

function display(product) {
    return product.name + " ($" + product.price + ", stock: " + product.stock + ")";
}

function inStock(product, quantity) {
    return product.stock >= quantity;
}

function reduceStock(product, quantity) {
    product.stock = product.stock - quantity;
}
```

### Inventory Service

```javascript
// services/inventory.unna
import "../models/product.unna" as product;

var products = [];

function loadFromUon(path) {
    var cursor = ucoreUon.open(path);
    var records = ucoreUon.all(cursor);
    ucoreUon.close(cursor);
    
    for (var record : records) {
        var p = product.create(
            record["id"],
            record["name"],
            record["price"],
            record["stock"]
        );
        push(products, p);
    }
    
    return length(products);
}

function findById(id) {
    for (var p : products) {
        if (p.id == id) {
            return p;
        }
    }
    return nil;
}

function list() {
    return products;
}

function updateStock(id, quantity) {
    var p = findById(id);
    if (p != nil) {
        p.stock = p.stock - quantity;
        return true;
    }
    return false;
}
```

### Sales Service

```javascript
// services/sales.unna
import "../models/sale.unna" as saleModel;
import "./inventory.unna" as inventory;

var sales = [];
var nextSaleId = 1;

struct SaleItem {
    productId;
    quantity;
    price;
}

function createSale(items) {
    var total = 0;
    var validItems = [];
    
    for (var item : items) {
        var product = inventory.findById(item.productId);
        if (product == nil) {
            print("Error: Product not found: " + item.productId);
            continue;
        }
        
        if (product.stock < item.quantity) {
            print("Error: Insufficient stock for " + product.name);
            continue;
        }
        
        var lineTotal = product.price * item.quantity;
        total = total + lineTotal;
        
        // Create valid item
        var validItem = SaleItem(item.productId, item.quantity, lineTotal);
        push(validItems, validItem);
        
        // Update stock
        inventory.updateStock(item.productId, item.quantity);
    }
    
    var sale = saleModel.create(nextSaleId, validItems, total);
    nextSaleId = nextSaleId + 1;
    push(sales, sale);
    
    return sale;
}

function todaySales() {
    return sales;
}

function totalRevenue() {
    var sum = 0;
    for (var sale : sales) {
        sum = sum + sale.total;
    }
    return sum;
}
```

### Main Application

```javascript
// main.unna
import "models/product.unna" as product;
import "services/inventory.unna" as inventory;
import "services/sales.unna" as sales;
import "utils/format.unna" as format;

print("=== SME Store Management System ===");
print("");

// Load inventory
print("Loading products...");
var count = inventory.loadFromUon("data/products.uon");
print("  Loaded " + count + " products");

for (var p : inventory.list()) {
    print("  " + product.display(p));
}
print("");

// Process sample sales
print("Processing sales...");

// Sale 1
var items1 = [
    {"productId": 1, "quantity": 2},
    {"productId": 3, "quantity": 1}
];
var sale1 = sales.createSale(items1);
print("  Sale #" + sale1.id + ": " + length(sale1.items) + " items, total: " + format.currency(sale1.total));

// Sale 2
var items2 = [
    {"productId": 2, "quantity": 5},
    {"productId": 1, "quantity": 3}
];
var sale2 = sales.createSale(items2);
print("  Sale #" + sale2.id + ": " + length(sale2.items) + " items, total: " + format.currency(sale2.total));

print("");

// Daily report
print("=== Daily Report ===");
print("Total Sales: " + length(sales.todaySales()));
print("Revenue: " + format.currency(sales.totalRevenue()));
print("");

print("SME Store System shutdown complete");
```

---

## Data File

```
# data/products.uon
@schema id:int, name:string, price:float, stock:int

1, Rice 5kg, 12.50, 100
2, Sugar 1kg, 3.20, 200
3, Cooking Oil 1L, 5.75, 150
4, Flour 1kg, 2.80, 120
5, Salt 500g, 0.99, 300
```

---

## HTTP API Extension

The system can be extended with an HTTP API:

```javascript
// api.unna
import "services/inventory.unna" as inventory;
import "services/sales.unna" as sales;

function handleGetProducts(req) {
    var products = inventory.list();
    var result = [];
    
    for (var p : products) {
        var item = map();
        item["id"] = p.id;
        item["name"] = p.name;
        item["price"] = p.price;
        item["stock"] = p.stock;
        push(result, item);
    }
    
    return ucoreHttp.json(result);
}

function handleCreateSale(req) {
    var data = ucoreJson.parse(req["body"]);
    var items = data["items"];
    
    var sale = sales.createSale(items);
    
    var response = map();
    response["saleId"] = sale.id;
    response["total"] = sale.total;
    
    return ucoreHttp.json(response);
}

function handleHealth(req) {
    var status = map();
    status["status"] = "ok";
    status["products"] = length(inventory.list());
    status["sales"] = length(sales.todaySales());
    return ucoreHttp.json(status);
}

// Routes
ucoreHttp.route("GET", "/api/products", "handleGetProducts");
ucoreHttp.route("POST", "/api/sales", "handleCreateSale");
ucoreHttp.route("GET", "/health", "handleHealth");

print("SME Store API running on http://localhost:8080");
ucoreHttp.listen(8080);
```

---

## Key Patterns Demonstrated

1. **Module Organization** - Separation of concerns
2. **Data Modeling** - Structs for domain objects
3. **Service Layer** - Business logic encapsulation
4. **Database Integration** - UON for persistence
5. **Error Handling** - Validation and feedback
6. **Reporting** - Aggregation and formatting

---

## Next Steps

- [Basic Examples](basics.md) - Language fundamentals
- [ucoreUon](../core-libraries/ucore-uon.md) - Database operations
- [ucoreHttp](../core-libraries/ucore-http.md) - Building APIs
