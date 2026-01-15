# ucoreTimer

> High-precision timing functions.

---

## API Reference

| Function | Returns | Description |
|----------|---------|-------------|
| `now()` | float | Current timestamp in milliseconds |
| `sleep(ms)` | nil | Pause execution for milliseconds |

---

## now()

Get current time in milliseconds (monotonic clock):

```javascript
var timestamp = ucoreTimer.now();
print("Current time: " + timestamp + "ms");
```

### Measuring Elapsed Time

```javascript
var start = ucoreTimer.now();

// Do some work
for (var i = 0; i < 1000000; i = i + 1) {
    var x = i * 2;
}

var elapsed = ucoreTimer.now() - start;
print("Elapsed: " + elapsed + "ms");
```

---

## sleep(ms)

Pause execution for specified milliseconds:

```javascript
print("Starting...");
ucoreTimer.sleep(1000);  // Sleep 1 second
print("Done!");
```

### Countdown Example

```javascript
for (var i = 5; i > 0; i = i - 1) {
    print(i + "...");
    ucoreTimer.sleep(1000);
}
print("Go!");
```

---

## Benchmarking

### Simple Benchmark

```javascript
function benchmark(name, fn) {
    var start = ucoreTimer.now();
    fn();
    var elapsed = ucoreTimer.now() - start;
    print(name + ": " + elapsed + "ms");
}

function testLoop() {
    var sum = 0;
    for (var i = 0; i < 10000000; i = i + 1) {
        sum = sum + i;
    }
}

benchmark("10M loop", testLoop);
```

### Operations Per Second

```javascript
function benchmarkOps(name, iterations, fn) {
    var start = ucoreTimer.now();
    
    for (var i = 0; i < iterations; i = i + 1) {
        fn();
    }
    
    var elapsed = ucoreTimer.now() - start;
    var seconds = elapsed / 1000;
    var opsPerSec = iterations / seconds;
    
    print(name + ": " + opsPerSec + " ops/sec");
}

function testAdd() {
    var x = 1 + 2;
}

benchmarkOps("Addition", 10000000, testAdd);
```

### Comparative Benchmark

```javascript
function compare(tests) {
    print("=== Benchmark Results ===");
    
    for (var test : tests) {
        var start = ucoreTimer.now();
        test["fn"]();
        var elapsed = ucoreTimer.now() - start;
        print(test["name"] + ": " + elapsed + "ms");
    }
}

var tests = [
    {"name": "Array push", "fn": testArrayPush},
    {"name": "String concat", "fn": testStringConcat},
    {"name": "Recursion", "fn": testRecursion}
];

compare(tests);
```

---

## Common Patterns

### Rate Limiting

```javascript
var lastRequest = 0;
var minInterval = 100;  // 100ms between requests

function rateLimitedRequest(url) {
    var now = ucoreTimer.now();
    var elapsed = now - lastRequest;
    
    if (elapsed < minInterval) {
        ucoreTimer.sleep(minInterval - elapsed);
    }
    
    lastRequest = ucoreTimer.now();
    return ucoreHttp.get(url);
}
```

### Timeout Pattern

```javascript
function withTimeout(fn, maxMs) {
    var start = ucoreTimer.now();
    var result = fn();
    var elapsed = ucoreTimer.now() - start;
    
    if (elapsed > maxMs) {
        print("Warning: Operation took " + elapsed + "ms (limit: " + maxMs + "ms)");
    }
    
    return result;
}
```

### Progress Reporting

```javascript
function processWithProgress(items) {
    var total = length(items);
    var start = ucoreTimer.now();
    
    for (var i = 0; i < total; i = i + 1) {
        // Process item
        processItem(items[i]);
        
        // Report progress every 100 items
        if ((i + 1) % 100 == 0) {
            var elapsed = ucoreTimer.now() - start;
            var rate = (i + 1) / (elapsed / 1000);
            print("Processed " + (i + 1) + "/" + total + " (" + rate + "/sec)");
        }
    }
}
```

### Retry with Delay

```javascript
function retryWithDelay(fn, maxRetries, delayMs) {
    var attempt = 0;
    
    while (attempt < maxRetries) {
        var result = fn();
        if (result != nil) {
            return result;
        }
        
        attempt = attempt + 1;
        if (attempt < maxRetries) {
            print("Retry " + attempt + " in " + delayMs + "ms...");
            ucoreTimer.sleep(delayMs);
        }
    }
    
    return nil;
}
```

---

## Examples

```javascript
// examples/corelib/timer/demo.unna
print("=== Timer Demo ===");

// Get current time
print("Timestamp: " + ucoreTimer.now() + "ms");

// Measure operation
var start = ucoreTimer.now();

var sum = 0;
for (var i = 0; i < 1000000; i = i + 1) {
    sum = sum + i;
}

var elapsed = ucoreTimer.now() - start;
print("Sum: " + sum);
print("Time: " + elapsed + "ms");

// Sleep demo
print("Sleeping for 500ms...");
ucoreTimer.sleep(500);
print("Done!");
```

---

## Next Steps

- [ucoreHttp](ucore-http.md) - Timed requests
- [ucoreSystem](ucore-system.md) - System operations
- [Overview](overview.md) - All libraries
