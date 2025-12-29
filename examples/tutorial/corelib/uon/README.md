# UON (Unnarize Object Notation) Module

The `ucoreUon` module provides a high-performance, streaming interface for reading UON data files. UON is designed to be a lightweight, human-readable database format for Unnarize applications.

## Features

- **Schema-aware**: Defines table structures clearly.
- **Streaming Access**: Reads data record-by-record, minimizing memory usage for large files.
- **Simple Syntax**: Similar to JSON but optimized for tabular data flows.

## File Format

A UON file consists of two main sections:

1. **`@schema`**: Defines the tables and their columns.
2. **`@flow`**: Contains the actual data records.

### Example `data.uon`

```uon
@schema {
    users: [id, name, active]
    products: [sku, price]
}

@flow {
    users: [
        { id: 1, name: "Alice", active: true },
        { id: 2, name: "Bob", active: false }
    ],

    products: [
        { sku: "P001", price: 99.99 }
    ]
}
```

## API Reference

### `ucoreUon.load(path)`
Loads a UON file and parses directly the schema section.

- **Parameters**: `path` (string) - Relative or absolute path to the `.uon` file.
- **Returns**: `true` if loaded successfully, `false` otherwise.

### `ucoreUon.get(tableName)`
opens a cursor for iterating through a specific table.

- **Parameters**: `tableName` (string) - Name of the table defined in schema.
- **Returns**: A `cursor` object (resource) if found, or `nil`.

### `ucoreUon.next(cursor)`
Reads the next record from the stream.

- **Parameters**: `cursor` - The cursor object returned by `get()`.
- **Returns**: A `Map` containing the record fields, or `nil` if end-of-stream is reached.

## Usage Pattern

```c
// 1. Load the file
ucoreUon.load("database.uon");

// 2. Get a cursor
var cursor = ucoreUon.get("users");

// 3. Iterate
while (true) {
    var record = ucoreUon.next(cursor);
    if (!record) break;
    print(record["name"]);
}
```
