# Async/Await

> Asynchronous programming with Futures.

---

## Overview

Unnarize supports native asynchronous programming:

- **`async function`** - Declares an asynchronous function
- **`await`** - Waits for a Future to resolve
- **`Future`** - Represents a value that will be available later

---

## Async Functions

Declare a function as async to enable asynchronous behavior:

```javascript
async function fetchData(url) {
    // Simulates async operation
    return "Response from " + url;
}

async function processTask(id) {
    print("Processing task " + id);
    return "Task " + id + " complete";
}
```

### Return Value

Async functions automatically return a `Future`:

```javascript
async function getData() {
    return "Hello";
}

var future = getData();  // Returns Future, not "Hello"
var result = await future;  // Now we get "Hello"
```

---

## Await Expression

Use `await` to wait for a Future to complete:

```javascript
async function main() {
    var result = await fetchData("api.example.com");
    print(result);  // "Response from api.example.com"
}

main();
```

### Await Inside Async Functions

`await` is typically used inside async functions:

```javascript
async function workflow() {
    // Sequential async operations
    var step1 = await fetchData("step1");
    var step2 = await fetchData("step2");
    var step3 = await fetchData("step3");
    
    return step1 + ", " + step2 + ", " + step3;
}
```

---

## Parallel Execution

Start multiple async operations and await them together:

```javascript
async function fetchAll() {
    // Start all tasks in parallel
    var task1 = fetchData("api1");  // Returns Future immediately
    var task2 = fetchData("api2");  // Returns Future immediately
    var task3 = fetchData("api3");  // Returns Future immediately
    
    // Wait for all to complete
    var result1 = await task1;
    var result2 = await task2;
    var result3 = await task3;
    
    print(result1);
    print(result2);
    print(result3);
}

fetchAll();
```

---

## Complete Example

```javascript
// examples/basics/11_async.unna
print("=== Async/Await ===");

// Async function definition
async function fetchData(source) {
    print("  Fetching from " + source + "...");
    return "Data:" + source;
}

async function processTask(id) {
    print("  Processing task " + id + "...");
    return "Task" + id + " complete";
}

// Async workflow
async function runWorkflow() {
    print("Workflow started");
    
    // Start tasks in parallel
    var task1 = fetchData("API");
    var task2 = fetchData("Database");
    
    // Await results
    var result1 = await task1;
    var result2 = await task2;
    
    print("  Got: " + result1);
    print("  Got: " + result2);
    
    print("Workflow finished");
}

// Execute async workflow
runWorkflow();

print("=== Complete ===");
```

**Output:**
```
=== Async/Await ===
Workflow started
  Fetching from API...
  Fetching from Database...
  Got: Data:API
  Got: Data:Database
Workflow finished
=== Complete ===
```

---

## Practical Use Cases

### HTTP Requests

```javascript
async function fetchUser(id) {
    var response = await ucoreHttp.get("https://api.example.com/users/" + id);
    return ucoreJson.parse(response);
}

async function main() {
    var user = await fetchUser(1);
    print("User: " + user["name"]);
}

main();
```

### Timer-Based Operations

```javascript
async function delayedMessage(msg, delay) {
    // Simulated delay (actual sleep would be native)
    return msg;
}

async function sequence() {
    print(await delayedMessage("Step 1", 100));
    print(await delayedMessage("Step 2", 200));
    print(await delayedMessage("Step 3", 300));
}

sequence();
```

### API Server with Async Handlers

```javascript
async function handleRequest(req) {
    // Simulate async database query
    var data = await queryDatabase(req["id"]);
    return ucoreHttp.json(data);
}

function queryDatabase(id) {
    // Return data
    var result = map();
    result["id"] = id;
    result["name"] = "User " + id;
    return result;
}

ucoreHttp.route("GET", "/api/user", "handleRequest");
ucoreHttp.listen(8080);
```

---

## Patterns

### Sequential vs Parallel

```javascript
// Sequential: Slower (waits for each)
async function sequential() {
    var a = await slowOperation();
    var b = await slowOperation();
    var c = await slowOperation();
    return a + b + c;
}

// Parallel: Faster (runs simultaneously)
async function parallel() {
    var futureA = slowOperation();
    var futureB = slowOperation();
    var futureC = slowOperation();
    
    var a = await futureA;
    var b = await futureB;
    var c = await futureC;
    
    return a + b + c;
}
```

### Error Handling Pattern

```javascript
struct Result {
    success;
    data;
    error;
}

async function safeFetch(url) {
    var response = await fetchData(url);
    if (response == nil) {
        return Result(false, nil, "Fetch failed");
    }
    return Result(true, response, nil);
}

async function main() {
    var result = await safeFetch("api.example.com");
    if (result.success) {
        print("Data: " + result.data);
    } else {
        print("Error: " + result.error);
    }
}
```

### Async Pipeline

```javascript
async function step1(data) {
    print("Step 1: Processing...");
    return data + "_step1";
}

async function step2(data) {
    print("Step 2: Transforming...");
    return data + "_step2";
}

async function step3(data) {
    print("Step 3: Finalizing...");
    return data + "_step3";
}

async function pipeline(initialData) {
    var r1 = await step1(initialData);
    var r2 = await step2(r1);
    var r3 = await step3(r2);
    return r3;
}

async function main() {
    var result = await pipeline("input");
    print("Final: " + result);
    // Final: input_step1_step2_step3
}

main();
```

---

## Implementation Details

### Future Object

Internally, a `Future` wraps:
- Completion state (`done` flag)
- Result value
- Synchronization primitives (mutex, condition variable)

### Await Semantics

When you `await` a Future:
1. If already complete, return the result immediately
2. If not complete, block until completion
3. Return the resolved value

---

## Best Practices

1. **Use async for I/O operations** - Network, file, timer operations benefit most
2. **Start early, await late** - Start Futures early and await when you need results
3. **Don't over-use await** - Unnecessary awaits add overhead
4. **Consider cancellation** - Long-running operations should be manageable

---

## Next Steps

- [Core Libraries](../core-libraries/overview.md) - Async-compatible libraries
- [ucoreHttp](../core-libraries/ucore-http.md) - HTTP client/server with async
- [Examples](../examples/sme-system.md) - Async in real applications
