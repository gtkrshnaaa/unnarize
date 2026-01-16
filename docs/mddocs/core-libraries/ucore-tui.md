# ucoreTui

The `ucoreTui` library provides robust Terminal User Interface capabilities, including styled output, layout components, and interactive input handling.

## Layout Components

### `box(content, style)`
Wraps content in a styled box.
*   **content**: String content (multiline supported).
*   **style**: (Optional) "simple", "double", "rounded" (default).

```javascript
print(ucoreTui.box("Hello World", "rounded"));
```

### `table(data)`
Renders a 2D array as a formatted table with auto-aligned columns.
*   **data**: Array of Arrays `[[Header1, Header2], [Row1Col1, Row1Col2]]`.

### `tree(map)`
Renders a nested tree structure.
*   **map**: Map with `name` (string) and optional `children` (array of maps).

### `progressBar(current, total, width)`
Returns a progress bar string (e.g., `[████░░]`).
*   **current**: Number.
*   **total**: Number.
*   **width**: Visual width (default 40).

### `spinner(style)`
Returns an array of spinner frames for manual animation loop.
*   **style**: "dots", "line", "arc", "bounce".

## Streaming Box API (Advanced)
For complex layouts where string concatenation is inefficient.

*   `boxBegin(title, width)`: Starts a box. `width=-1` for auto screen width.
*   `boxLine(text)`: Prints a line of content inside the box borders.
*   `boxEnd()`: Closes the box.

## Interactive Input

### `input(prompt)` -> String
Reads a line of text.

### `inputPassword(prompt)` -> String
Reads text with `*` masking.

### `inputBox(title, width, default)` -> String
Opens a styled input widget. User can edit text.

### `confirm(prompt)` -> Bool
Asks Yes/No (returns `true` or `false`).

### `select(prompt, options)` -> Int
Interactive menu. Returns index of selected option options (Array of strings).

## Styling Primitives

*   `clear()`: Clear screen.
*   `moveTo(row, col)`: Move cursor.
*   `hideCursor() / showCursor()`
*   `size()`: Returns map `{"rows": r, "cols": c}`.
*   `fg(color, text)` / `bg(color, text)`: Apply colors (red, green, blue, yellow, cyan, magenta, white, black).
*   `style(styleStr, text)`: Apply styles ("bold", "dim", "italic", "underline").
