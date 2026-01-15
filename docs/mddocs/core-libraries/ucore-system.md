# ucoreSystem

> System operations: file I/O, shell execution, environment.

> [!IMPORTANT]
> **Strict Path Resolution**: Unnarize enforces strict sandboxing. All file paths used in this library are resolved relative to the directory of the **executing script**, regardless of where the `unnarize` command is run. Absolute paths (e.g., `/etc/passwd`) are typically rebased or rejected (treated as relative) to prevent sandbox escape.

---

## API Reference

| Function | Returns | Description |
|----------|---------|-------------|
| `readFile(path)` | string | Read file contents |
| `writeFile(path, content)` | bool | Write content to file |
| `fileExists(path)` | bool | Check if file exists |
| `exec(command)` | string | Execute shell command |
| `getenv(name)` | string | Get environment variable |
| `args()` | array | Get command line arguments |
| `input(prompt)` | string | Read user input from stdin |
| `exit(code)` | nil | Exit program |

---

## File Operations

### readFile(path)

Read entire file as string:

```javascript
var content = ucoreSystem.readFile("config.txt");
print(content);
```

### writeFile(path, content)

Write string to file (overwrites):

```javascript
ucoreSystem.writeFile("output.txt", "Hello, World!");
```

### fileExists(path)

Check if a file exists:

```javascript
if (ucoreSystem.fileExists("config.json")) {
    var config = ucoreJson.read("config.json");
} else {
    print("Config not found, using defaults");
}
```

### input(prompt)

Read user input from stdin:

```javascript
var name = ucoreSystem.input("Enter your name: ");
print("Hello, " + name);
```

---

## Shell Execution

### exec(command)

Execute a shell command and get output:

```javascript
var result = ucoreSystem.exec("ls -la");
print(result);

var date = ucoreSystem.exec("date");
print("Current date: " + date);

var files = ucoreSystem.exec("find . -name '*.unna'");
print(files);
```

### Command with Arguments

```javascript
var dir = "/home/user";
var result = ucoreSystem.exec("ls " + dir);
print(result);
```

---

## Environment

### getenv(name)

Get environment variable:

```javascript
var home = ucoreSystem.getenv("HOME");
print("Home directory: " + home);

var user = ucoreSystem.getenv("USER");
print("Current user: " + user);

var path = ucoreSystem.getenv("PATH");
print("PATH: " + path);
```

### args()

Get command line arguments:

```javascript
var arguments = ucoreSystem.args();

print("Script: " + arguments[0]);
for (var i = 1; i < length(arguments); i = i + 1) {
    print("Arg " + i + ": " + arguments[i]);
}
```

Usage:
```bash
./bin/unnarize script.unna arg1 arg2 arg3
```

---

## Program Control

### exit(code)

Exit program with status code:

```javascript
function main() {
    var config = ucoreSystem.readFile("config.txt");
    if (config == nil) {
        print("Error: Config not found");
        ucoreSystem.exit(1);
    }
    
    // Continue with program...
    print("Config loaded successfully");
    ucoreSystem.exit(0);
}

main();
```

---

## Common Patterns

### Configuration Loading

```javascript
function loadConfig(path, defaults) {
    if (!ucoreSystem.exists(path)) {
        return defaults;
    }
    
    var content = ucoreSystem.readFile(path);
    return ucoreJson.parse(content);
}

var defaultConfig = map();
defaultConfig["port"] = 8080;
defaultConfig["debug"] = false;

var config = loadConfig("config.json", defaultConfig);
print("Port: " + config["port"]);
```

### Logging

```javascript
function log(level, message) {
    var timestamp = ucoreTimer.now();
    var entry = "[" + timestamp + "] " + level + ": " + message + "\n";
    ucoreSystem.appendFile("app.log", entry);
}

log("INFO", "Application started");
log("DEBUG", "Processing request");
log("ERROR", "Something went wrong");
```

### Command Line Tool

```javascript
var args = ucoreSystem.args();

if (length(args) < 2) {
    print("Usage: unnarize script.unna <command> [args]");
    print("Commands:");
    print("  list     - List items");
    print("  add      - Add item");
    print("  remove   - Remove item");
    ucoreSystem.exit(1);
}

var command = args[1];

if (command == "list") {
    listItems();
} else if (command == "add") {
    if (length(args) < 3) {
        print("Error: add requires an argument");
        ucoreSystem.exit(1);
    }
    addItem(args[2]);
} else if (command == "remove") {
    if (length(args) < 3) {
        print("Error: remove requires an argument");
        ucoreSystem.exit(1);
    }
    removeItem(args[2]);
} else {
    print("Unknown command: " + command);
    ucoreSystem.exit(1);
}
```

### Script Runner

```javascript
function runScript(scriptPath) {
    if (!ucoreSystem.exists(scriptPath)) {
        print("Script not found: " + scriptPath);
        return false;
    }
    
    var result = ucoreSystem.exec("./bin/unnarize " + scriptPath);
    print(result);
    return true;
}
```

### Backup Function

```javascript
function backup(sourcePath) {
    var timestamp = ucoreTimer.now();
    var backupPath = sourcePath + ".backup." + timestamp;
    
    var content = ucoreSystem.readFile(sourcePath);
    ucoreSystem.writeFile(backupPath, content);
    
    print("Backup created: " + backupPath);
}

backup("important.txt");
```

---

## Examples

```javascript
// examples/corelib/system/demo.unna
print("=== System Demo ===");

// Environment
print("User: " + ucoreSystem.env("USER"));
print("Home: " + ucoreSystem.env("HOME"));

// Command line args
var args = ucoreSystem.args();
print("Arguments: " + length(args));

// File operations
ucoreSystem.writeFile("test.txt", "Hello from Unnarize!");
var content = ucoreSystem.readFile("test.txt");
print("File content: " + content);

// Shell execution
var result = ucoreSystem.exec("echo 'Hello from shell'");
print("Shell result: " + result);
```

---

## Next Steps

- [ucoreJson](ucore-json.md) - Read/write JSON files
- [ucoreTimer](ucore-timer.md) - Timing operations
- [Overview](overview.md) - All libraries
