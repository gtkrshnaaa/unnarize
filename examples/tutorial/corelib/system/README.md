# System Module

The `ucoreSystem` module provides access to system-level functionality like environment variables, file system checks, and CLI arguments.

## API Reference

### `ucoreSystem.args()`
Returns an array of command line arguments passed to the script.
- **Example**: `var args = ucoreSystem.args();`

### `ucoreSystem.getenv(name)`
Retrieves the value of an environment variable. Returns an empty string if not found.
- **Parameters**: `name` (string) - e.g., "PATH", "HOME".
- **Returns**: String value.

### `ucoreSystem.fileExists(path)`
Checks if a file exists and is accessible.
- **Parameters**: `path` (string) - Relative or absolute path.
- **Returns**: `true` or `false`.

### `ucoreSystem.input(prompt)`
Reads a line of text from standard input (console). Execution blocks until the user presses Enter.
- **Parameters**: `prompt` (string) - Text to display.
- **Returns**: String content (without newline).
