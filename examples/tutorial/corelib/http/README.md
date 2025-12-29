# HTTP Module

The `ucoreHttp` module provides comprehensive capabilities to build web servers and make HTTP requests directly from Unnarize. It bridges your application with the web.

## Features

- **Embedded Web Server**: Launch a lightweight HTTP server with a single command.
- **Routing System**: Map specific URLs and Methods to function handlers.
- **HTTP Client**: Consume external APIs using `GET` and `POST`.
- **JSON Utility**: Built-in serializer for JSON APIs.

## API Reference

### 1. Server & Routing

#### `ucoreHttp.listen(port, [defaultHandler])`
Starts the HTTP server on the specified port. This function **blocks execution** and listens for incoming connections.

- **Parameters**: 
  - `port` (int): Port number (e.g., 8080).
  - `defaultHandler` (string, optional): Name of the global function to handle requests that don't match any route.
- **Returns**: `false` if failed to bind port.

#### `ucoreHttp.route(method, path, handlerName)`
Registers a route to a specific handler function.

- **Parameters**:
  - `method` (string): HTTP Method ("GET", "POST", "PUT", "DELETE").
  - `path` (string): Exact URL path (e.g., "/api/login").
  - `handlerName` (string): Name of the global function to execute.
- **Returns**: `true`.

---

### 2. Request Handlers

Handler functions must accept a single argument `req` (Request Map) and return a string (Response Body).

**Request Map Structure (`req`):**
| Key | Type | Description |
|-----|------|-------------|
| `"method"` | string | HTTP Method (e.g., "GET") |
| `"path"` | string | Request Path (e.g., "/index") |
| `"body"` | string | Request Body (for POST/PUT) |

**Example Handler:**
```c
function indexHandler(req) {
    if (req["method"] == "POST") {
        return "Received: " + req["body"];
    }
    return "Hello World";
}
```

---

### 3. Client Requests

#### `ucoreHttp.get(url)`
Performs a blocking HTTP GET request.

- **Parameters**: `url` (string) - Full URL (e.g., "http://example.com").
- **Returns**: Response body (string) or `nil` on failure.

#### `ucoreHttp.post(url, body)`
Performs a blocking HTTP POST request.

- **Parameters**: 
  - `url` (string)
  - `body` (string) - Data to send.
- **Returns**: Response body (string) or `nil`.

---

### 4. Utilities

#### `ucoreHttp.json(value)`
Serializes an Unnarize value into a JSON string. Supports nested Maps and Arrays.

- **Parameters**: `value` (Map, Array, Number, String, Bool, Nil).
- **Returns**: JSON string.

**Example:**
```c
var data = map();
data["status"] = "ok";
var json = ucoreHttp.json(data); // '{"status":"ok"}'
```
