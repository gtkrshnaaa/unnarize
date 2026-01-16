# Core Libraries Overview

> Built-in libraries available in every Unnarize program.

---

## Available Libraries

| Library | Description | Use Case |
|---------|-------------|----------|
| [ucoreString](ucore-string.md) | Text manipulation | String processing, formatting |
| [ucoreScraper](ucore-scraper.md) | HTML parsing | Web scraping, DOM queries |
| [ucoreJson](ucore-json.md) | JSON handling | API data, config files |
| [ucoreHttp](ucore-http.md) | HTTP client/server | Web services, REST APIs |
| [ucoreTimer](ucore-timer.md) | High-precision timing | Benchmarks, delays |
| [ucoreSystem](ucore-system.md) | System operations | Files, shell, environment |
| [ucoreUon](ucore-uon.md) | UON data format | Custom database format |
| [ucoreTui](ucore-tui.md) | Terminal UI | Rich CLI, Input, Layouts |

---

## Quick Reference

### ucoreString

```javascript
var text = "Hello World";
print(ucoreString.upper(text));      // "HELLO WORLD"
print(ucoreString.lower(text));      // "hello world"
print(ucoreString.contains(text, "World")); // true

var parts = ucoreString.split("a,b,c", ",");
print(parts);  // ["a", "b", "c"]
```

### ucoreScraper

```javascript
var doc = ucoreScraper.parseFile("page.html");
var links = ucoreScraper.select(doc, "a");
var titles = ucoreScraper.select(doc, "h1");
```

### ucoreJson

```javascript
var data = ucoreJson.parse("{\"name\":\"Alice\"}");
print(data["name"]);  // "Alice"

var obj = map();
obj["id"] = 1;
print(ucoreJson.stringify(obj));  // {"id":1}
```

### ucoreHttp

```javascript
// Client
var response = ucoreHttp.get("https://api.example.com");

// Server
function handler(req) { return "OK"; }
ucoreHttp.route("GET", "/", "handler");
ucoreHttp.listen(8080);
```

### ucoreTimer

```javascript
var start = ucoreTimer.now();
// Do work...
var elapsed = ucoreTimer.now() - start;
print("Elapsed: " + elapsed + "ms");
```

### ucoreSystem

```javascript
var content = ucoreSystem.readFile("config.txt");
ucoreSystem.writeFile("output.txt", "data");
var result = ucoreSystem.exec("ls -la");
```

### ucoreUon

```javascript
var db = ucoreUon.open("data.uon");
var records = ucoreUon.all(db);
for (var record : records) {
    print(record["name"]);
}
```

### ucoreTui

```javascript
print(ucoreTui.box("Hello", "rounded"));
var name = ucoreTui.input("Name: ");
var choice = ucoreTui.select("Mode", ["Easy", "Hard"]);
```

---

## Library Performance

Benchmarks on Intel Core i5-1135G7:

| Library | Operation | Speed |
|---------|-----------|-------|
| ucoreString | `contains` (14KB) | 24.5 M ops/sec |
| ucoreString | `toLower` (14KB) | 42,500 ops/sec |
| ucoreString | `replace` (14KB) | 38,800 ops/sec |
| ucoreScraper | `parseFile` (370KB) | 339 ops/sec |
| ucoreScraper | `select` (in-memory) | 128 ops/sec |
| ucoreJson | `parse` | ~50,000 ops/sec |

---

## Common Patterns

### REST API Client

```javascript
function getUser(id) {
    var url = "https://api.example.com/users/" + id;
    var response = ucoreHttp.get(url);
    return ucoreJson.parse(response);
}

var user = getUser(1);
print("Name: " + user["name"]);
```

### REST API Server

```javascript
var users = [];

function handleGetUsers(req) {
    return ucoreHttp.json(users);
}

function handleCreateUser(req) {
    var data = ucoreJson.parse(req["body"]);
    push(users, data);
    return ucoreHttp.json(data);
}

ucoreHttp.route("GET", "/api/users", "handleGetUsers");
ucoreHttp.route("POST", "/api/users", "handleCreateUser");
ucoreHttp.listen(8080);
```

### Web Scraping

```javascript
var html = ucoreHttp.get("https://example.com");
var doc = ucoreScraper.parse(html);
var titles = ucoreScraper.select(doc, "h1");

for (var title : titles) {
    print(ucoreScraper.text(title));
}
```

### Benchmarking

```javascript
function benchmark(name, iterations, fn) {
    var start = ucoreTimer.now();
    for (var i = 0; i < iterations; i = i + 1) {
        fn();
    }
    var elapsed = ucoreTimer.now() - start;
    var opsPerSec = iterations / (elapsed / 1000);
    print(name + ": " + opsPerSec + " ops/sec");
}
```

---

## Next Steps

- [ucoreString](ucore-string.md) - Start with string manipulation
- [ucoreHttp](ucore-http.md) - Build web services
- [Examples](../examples/basics.md) - See libraries in action
