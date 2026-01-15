# ucoreJson

> JSON parsing and serialization.

---

## API Reference

| Function | Returns | Description |
|----------|---------|-------------|
| `parse(str)` | map/array | Parse JSON string to object |
| `stringify(obj)` | string | Convert object to JSON string |
| `read(path)` | map/array | Read and parse JSON file |
| `write(path, obj)` | bool | Write object as JSON to file |

---

## Parsing JSON

### parse(str)

Convert a JSON string to an Unnarize object:

```javascript
var jsonStr = "{\"name\":\"Alice\",\"age\":30}";
var obj = ucoreJson.parse(jsonStr);

print(obj["name"]);  // "Alice"
print(obj["age"]);   // 30
```

### Parsing Arrays

```javascript
var jsonArr = "[1, 2, 3, 4, 5]";
var arr = ucoreJson.parse(jsonArr);

print(arr[0]);       // 1
print(length(arr));  // 5
```

### Nested Objects

```javascript
var json = "{\"user\":{\"name\":\"Bob\",\"email\":\"bob@example.com\"}}";
var data = ucoreJson.parse(json);

print(data["user"]["name"]);   // "Bob"
print(data["user"]["email"]);  // "bob@example.com"
```

---

## Creating JSON

### stringify(obj)

Convert an Unnarize object to a JSON string:

```javascript
var user = map();
user["name"] = "Alice";
user["age"] = 30;
user["active"] = true;

var json = ucoreJson.stringify(user);
print(json);  // {"name":"Alice","age":30,"active":true}
```

### Arrays

```javascript
var items = ["apple", "banana", "cherry"];
print(ucoreJson.stringify(items));
// ["apple","banana","cherry"]
```

### Nested Objects

```javascript
var address = map();
address["city"] = "New York";
address["zip"] = "10001";

var user = map();
user["name"] = "Alice";
user["address"] = address;

print(ucoreJson.stringify(user));
// {"name":"Alice","address":{"city":"New York","zip":"10001"}}
```

---

## File Operations

### read(path)

Read and parse a JSON file:

```javascript
// config.json: {"port": 8080, "debug": true}
var config = ucoreJson.read("config.json");

print(config["port"]);   // 8080
print(config["debug"]);  // true
```

### write(path, obj)

Write an object to a JSON file:

```javascript
var settings = map();
settings["theme"] = "dark";
settings["language"] = "en";

ucoreJson.write("settings.json", settings);
// Creates: {"theme":"dark","language":"en"}
```

---

## Common Patterns

### API Response Handling

```javascript
function fetchUser(id) {
    var url = "https://api.example.com/users/" + id;
    var response = ucoreHttp.get(url);
    return ucoreJson.parse(response);
}

var user = fetchUser(1);
print("Welcome, " + user["name"]);
```

### Building API Response

```javascript
function createResponse(success, data, error) {
    var response = map();
    response["success"] = success;
    response["data"] = data;
    response["error"] = error;
    response["timestamp"] = ucoreTimer.now();
    return ucoreJson.stringify(response);
}

// Success response
print(createResponse(true, "User created", nil));
// {"success":true,"data":"User created","error":null,"timestamp":1234567890}

// Error response
print(createResponse(false, nil, "Invalid input"));
// {"success":false,"data":null,"error":"Invalid input","timestamp":1234567890}
```

### Configuration File

```javascript
// Load config with defaults
function loadConfig(path) {
    var config = ucoreJson.read(path);
    
    // Set defaults if missing
    if (!has(config, "port")) {
        config["port"] = 8080;
    }
    if (!has(config, "debug")) {
        config["debug"] = false;
    }
    
    return config;
}

var config = loadConfig("app.json");
print("Server port: " + config["port"]);
```

### Data Storage

```javascript
struct Todo {
    id;
    text;
    done;
}

var todos = [];

function saveTodos() {
    var data = [];
    for (var todo : todos) {
        var item = map();
        item["id"] = todo.id;
        item["text"] = todo.text;
        item["done"] = todo.done;
        push(data, item);
    }
    ucoreJson.write("todos.json", data);
}

function loadTodos() {
    var data = ucoreJson.read("todos.json");
    todos = [];
    for (var item : data) {
        push(todos, Todo(item["id"], item["text"], item["done"]));
    }
}
```

---

## Type Mapping

| JSON Type | Unnarize Type |
|-----------|---------------|
| `null` | `nil` |
| `true`/`false` | `true`/`false` |
| Number | Integer or Float |
| String | String |
| Array | Array |
| Object | Map |

---

## Examples

```javascript
// examples/corelib/json/demo.unna
print("=== JSON Demo ===");

// Parse JSON
var json = "{\"name\":\"Alice\",\"items\":[1,2,3]}";
var data = ucoreJson.parse(json);
print("Name: " + data["name"]);
print("Items: " + data["items"]);

// Create JSON
var obj = map();
obj["status"] = "ok";
obj["count"] = 42;
print("Generated: " + ucoreJson.stringify(obj));

// File operations
ucoreJson.write("test.json", obj);
var loaded = ucoreJson.read("test.json");
print("Loaded status: " + loaded["status"]);
```

---

## Next Steps

- [ucoreHttp](ucore-http.md) - Send JSON in HTTP requests
- [ucoreUon](ucore-uon.md) - Alternative data format
- [Overview](overview.md) - All libraries
