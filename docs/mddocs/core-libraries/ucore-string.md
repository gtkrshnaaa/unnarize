# ucoreString

> Text manipulation and processing.

---

## API Reference

| Function | Returns | Description |
|----------|---------|-------------|
| `split(str, delimiter)` | array | Split string into array |
| `join(array, delimiter)` | string | Join array into string |
| `replace(str, old, new)` | string | Replace occurrences |
| `trim(str)` | string | Remove leading/trailing whitespace |
| `toLower(str)` | string | Convert to lowercase |
| `toUpper(str)` | string | Convert to uppercase |
| `contains(str, substr)` | bool | Check if contains substring |
| `match(str, pattern)` | bool | Regex match |
| `extract(str, pattern)` | array | Regex extract matches |

---

## Basic Operations

### Case Conversion

```javascript
var text = "Hello World";

print(ucoreString.toUpper(text));  // "HELLO WORLD"
print(ucoreString.toLower(text));  // "hello world"
```

### Trim

```javascript
var padded = "  hello  ";
print(ucoreString.trim(padded));  // "hello"
```

---

## Search Operations

### Contains

```javascript
var text = "The quick brown fox";

print(ucoreString.contains(text, "quick"));  // true
print(ucoreString.contains(text, "slow"));   // false
```

---

## Split and Join

### Split

```javascript
var csv = "apple,banana,cherry";
var parts = ucoreString.split(csv, ",");

print(parts[0]);  // "apple"
print(parts[1]);  // "banana"
print(parts[2]);  // "cherry"
print(length(parts));  // 3

// Split by space
var words = ucoreString.split("hello world", " ");
print(words);  // ["hello", "world"]
```

### Join

```javascript
var items = ["apple", "banana", "cherry"];

print(ucoreString.join(items, ", "));   // "apple, banana, cherry"
print(ucoreString.join(items, " | "));  // "apple | banana | cherry"
print(ucoreString.join(items, ""));     // "applebananacherry"
```

---

## String Manipulation

### Replace

```javascript
var text = "Hello World";

print(ucoreString.replace(text, "World", "Unnarize"));
// "Hello Unnarize"

// Replace all occurrences
var repeated = "a-b-c-d";
print(ucoreString.replace(repeated, "-", "_"));
// "a_b_c_d"
```

---

## Regular Expressions

### Match

```javascript
var email = "user@example.com";
var pattern = "^[a-zA-Z0-9]+@[a-zA-Z0-9]+\\.[a-zA-Z]+$";

if (ucoreString.match(email, pattern)) {
    print("Valid email");
}
```

### Extract

```javascript
var text = "Contact: 555-1234 or 555-5678";
var pattern = "[0-9]{3}-[0-9]{4}";

var phones = ucoreString.extract(text, pattern);
for (var phone : phones) {
    print("Phone: " + phone);
}
// Phone: 555-1234
// Phone: 555-5678
```

---

## Common Patterns

### Parsing CSV

```javascript
function parseCSVLine(line) {
    return ucoreString.split(line, ",");
}

var line = "John,Doe,30,Engineer";
var fields = parseCSVLine(line);

print("First: " + fields[0]);  // John
print("Last: " + fields[1]);   // Doe
print("Age: " + fields[2]);    // 30
print("Job: " + fields[3]);    // Engineer
```

### Building Strings

```javascript
function formatUser(name, email, role) {
    var parts = [name, email, role];
    return ucoreString.join(parts, " | ");
}

print(formatUser("Alice", "alice@example.com", "Admin"));
// "Alice | alice@example.com | Admin"
```

### Validation

```javascript
function isValidEmail(email) {
    return ucoreString.contains(email, "@") && 
           ucoreString.contains(email, ".");
}

function isValidPhone(phone) {
    return ucoreString.match(phone, "^[0-9]{3}-[0-9]{3}-[0-9]{4}$");
}

print(isValidEmail("test@example.com"));  // true
print(isValidPhone("555-123-4567"));      // true
```

### Slug Generation

```javascript
function slugify(text) {
    var lower = ucoreString.lower(text);
    var slug = ucoreString.replace(lower, " ", "-");
    return slug;
}

print(slugify("Hello World"));  // "hello-world"
print(slugify("My Blog Post")); // "my-blog-post"
```

---

## Performance

Benchmarks on 14KB text:

| Operation | Speed |
|-----------|-------|
| `contains` | 24.5 M ops/sec |
| `toLower` | 42,500 ops/sec |
| `replace` | 38,800 ops/sec |
| `split` | 12,300 ops/sec |
| `extract` (regex) | 2,600 ops/sec |

---

## Examples

```javascript
// examples/corelib/string/demo.unna
var text = "Hello, World!";

print("Original: " + text);
print("Upper: " + ucoreString.upper(text));
print("Lower: " + ucoreString.lower(text));
print("Contains 'World': " + ucoreString.contains(text, "World"));

var csv = "apple,banana,cherry";
var fruits = ucoreString.split(csv, ",");
print("Fruits: " + ucoreString.join(fruits, " | "));
```

---

## Next Steps

- [ucoreJson](ucore-json.md) - Parse JSON strings
- [ucoreScraper](ucore-scraper.md) - Extract text from HTML
- [Overview](overview.md) - All libraries
