# ucoreHttp

> HTTP client and server functionality.

---

## API Reference

### Client Functions

| Function | Returns | Description |
|----------|---------|-------------|
| `get(url)` | string | HTTP GET request |
| `post(url, body)` | string | HTTP POST with body |
| `put(url, body)` | string | HTTP PUT with body |
| `patch(url, body)` | string | HTTP PATCH with body |
| `delete(url)` | string | HTTP DELETE request |
| `json(data)` | string | Serialize to JSON |

### Server Functions

| Function | Returns | Description |
|----------|---------|-------------|
| `route(method, path, handler)` | bool | Register route |
| `listen(port, [handler])` | bool | Start server |
| `use(middleware)` | bool | Add middleware |
| `static(path, dir)` | bool | Serve static files |

---

## HTTP Client

### GET Request

```javascript
var response = ucoreHttp.get("https://api.example.com/users");
print(response);

// Parse JSON response
var users = ucoreJson.parse(response);
for (var user : users) {
    print(user["name"]);
}
```

### POST Request

```javascript
var payload = map();
payload["name"] = "Alice";
payload["email"] = "alice@example.com";

var body = ucoreHttp.json(payload);
var response = ucoreHttp.post("https://api.example.com/users", body);
print(response);
```

### PUT Request

```javascript
var user = map();
user["name"] = "Alice Updated";
user["email"] = "alice.new@example.com";

ucoreHttp.put("https://api.example.com/users/1", ucoreHttp.json(user));
```

### PATCH Request

```javascript
var patch = map();
patch["email"] = "newemail@example.com";

ucoreHttp.patch("https://api.example.com/users/1", ucoreHttp.json(patch));
```

### DELETE Request

```javascript
ucoreHttp.delete("https://api.example.com/users/1");
```

---

## HTTP Server

### Request Object

Handler functions receive a request Map:

| Key | Type | Description |
|-----|------|-------------|
| `method` | string | HTTP method (GET, POST, etc.) |
| `path` | string | Request path (/api/users) |
| `body` | string | Request body (POST/PUT/PATCH) |
| `query` | map | Query parameters (?id=1) |

### Basic Server

```javascript
function handleHome(req) {
    return "Welcome to Unnarize!";
}

function handleApi(req) {
    var response = map();
    response["status"] = "ok";
    return ucoreHttp.json(response);
}

ucoreHttp.route("GET", "/", "handleHome");
ucoreHttp.route("GET", "/api", "handleApi");

print("Server running on http://localhost:8080");
ucoreHttp.listen(8080);
```

### Route with Parameters

```javascript
function handleUser(req) {
    // Get ID from query string: /api/user?id=1
    var id = req["query"]["id"];
    
    var user = map();
    user["id"] = id;
    user["name"] = "User " + id;
    
    return ucoreHttp.json(user);
}

ucoreHttp.route("GET", "/api/user", "handleUser");
```

### POST Handler

```javascript
var users = [];

function createUser(req) {
    var data = ucoreJson.parse(req["body"]);
    data["id"] = length(users) + 1;
    push(users, data);
    return ucoreHttp.json(data);
}

ucoreHttp.route("POST", "/api/users", "createUser");
```

---

## Advanced Features

### Middleware

```javascript
function logMiddleware(req) {
    print("[" + ucoreTimer.now() + "] " + req["method"] + " " + req["path"]);
    return nil;  // Continue to route handler
}

ucoreHttp.use("logMiddleware");
```

### Static Files

```javascript
// Serve files from ./public at /static/*
ucoreHttp.static("/static", "./public");

// Access: http://localhost:8080/static/style.css
// Serves: ./public/style.css
```

### Single Handler Mode

```javascript
function handleAll(req) {
    print("Request: " + req["method"] + " " + req["path"]);
    return "OK";
}

// All requests go to handleAll
ucoreHttp.listen(8080, "handleAll");
```

---

## Complete REST API

```javascript
// Data storage
var users = [];
var nextId = 1;

// GET /api/users - List all users
function getUsers(req) {
    return ucoreHttp.json(users);
}

// POST /api/users - Create user
function createUser(req) {
    var data = ucoreJson.parse(req["body"]);
    data["id"] = nextId;
    nextId = nextId + 1;
    push(users, data);
    return ucoreHttp.json(data);
}

// GET /api/users?id=N - Get single user
function getUser(req) {
    var id = req["query"]["id"];
    for (var user : users) {
        if (user["id"] == id) {
            return ucoreHttp.json(user);
        }
    }
    return "{\"error\":\"Not found\"}";
}

// DELETE /api/users?id=N - Delete user
function deleteUser(req) {
    var id = req["query"]["id"];
    var newUsers = [];
    for (var user : users) {
        if (user["id"] != id) {
            push(newUsers, user);
        }
    }
    users = newUsers;
    return "{\"success\":true}";
}

// Health check
function health(req) {
    var res = map();
    res["status"] = "ok";
    res["uptime"] = ucoreTimer.now();
    return ucoreHttp.json(res);
}

// Register routes
ucoreHttp.route("GET", "/api/users", "getUsers");
ucoreHttp.route("POST", "/api/users", "createUser");
ucoreHttp.route("GET", "/api/user", "getUser");
ucoreHttp.route("DELETE", "/api/user", "deleteUser");
ucoreHttp.route("GET", "/health", "health");

// Start server
print("REST API running on http://localhost:8080");
ucoreHttp.listen(8080);
```

---

## Common Patterns

### JSON Response Helper

```javascript
function jsonResponse(data) {
    return ucoreHttp.json(data);
}

function errorResponse(message, code) {
    var err = map();
    err["error"] = message;
    err["code"] = code;
    return ucoreHttp.json(err);
}
```

### Request Validation

```javascript
function createUser(req) {
    var data = ucoreJson.parse(req["body"]);
    
    if (!has(data, "name")) {
        return errorResponse("Name is required", 400);
    }
    if (!has(data, "email")) {
        return errorResponse("Email is required", 400);
    }
    
    // Create user...
    return jsonResponse(data);
}
```

---

## Examples

```javascript
// examples/corelib/http/server.unna
print("=== HTTP Server Demo ===");

function handleRoot(req) {
    return "Hello from Unnarize HTTP Server!";
}

function handleApi(req) {
    var data = map();
    data["message"] = "API is working";
    data["method"] = req["method"];
    data["path"] = req["path"];
    return ucoreHttp.json(data);
}

ucoreHttp.route("GET", "/", "handleRoot");
ucoreHttp.route("GET", "/api", "handleApi");

print("Server: http://localhost:8080");
ucoreHttp.listen(8080);
```

---

## Next Steps

- [ucoreJson](ucore-json.md) - JSON handling for API data
- [ucoreTimer](ucore-timer.md) - Request timing
- [Overview](overview.md) - All libraries
