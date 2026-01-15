# ucoreScraper

> HTML parsing with CSS selectors for web scraping.

---

## API Reference

| Function | Returns | Description |
|----------|---------|-------------|
| `parse(html)` | doc | Parse HTML string |
| `parseFile(path)` | doc | Parse HTML file |
| `select(doc, selector)` | array | Query with CSS selector |
| `text(element)` | string | Get element text content |
| `attr(element, name)` | string | Get attribute value |
| `html(element)` | string | Get inner HTML |
| `fetch(url)` | string | Download URL content |
| `download(url, path)` | bool | Download file to path |

---

## Parsing HTML

### parse(html)

Parse an HTML string:

```javascript
var html = "<html><body><h1>Title</h1><p>Content</p></body></html>";
var doc = ucoreScraper.parse(html);
```

### parseFile(path)

Parse an HTML file:

```javascript
var doc = ucoreScraper.parseFile("page.html");
```

---

## CSS Selectors

### select(doc, selector)

Query elements using CSS selectors:

```javascript
var doc = ucoreScraper.parseFile("page.html");

// By tag name
var headings = ucoreScraper.select(doc, "h1");
var paragraphs = ucoreScraper.select(doc, "p");
var links = ucoreScraper.select(doc, "a");

// By class
var items = ucoreScraper.select(doc, ".item");
var cards = ucoreScraper.select(doc, ".card");

// By ID
var header = ucoreScraper.select(doc, "#header");

// Nested selectors
var navLinks = ucoreScraper.select(doc, "nav a");
var tableRows = ucoreScraper.select(doc, "table tr");

// Attribute selectors
var externalLinks = ucoreScraper.select(doc, "a[target='_blank']");
```

### Supported Selectors

| Selector | Description | Example |
|----------|-------------|---------|
| `tag` | Element by tag | `div`, `p`, `a` |
| `.class` | Element by class | `.item`, `.card` |
| `#id` | Element by ID | `#header`, `#main` |
| `parent child` | Descendant | `div p`, `ul li` |
| `[attr]` | Has attribute | `[href]`, `[data-id]` |
| `[attr=val]` | Attribute equals | `[type='text']` |

---

## Extracting Data

### text(element)

Get text content:

```javascript
var doc = ucoreScraper.parseFile("page.html");
var headings = ucoreScraper.select(doc, "h1");

for (var h : headings) {
    print(ucoreScraper.text(h));
}
```

### attr(element, name)

Get attribute value:

```javascript
var links = ucoreScraper.select(doc, "a");

for (var link : links) {
    var href = ucoreScraper.attr(link, "href");
    var text = ucoreScraper.text(link);
    print(text + " -> " + href);
}
```

### html(element)

Get inner HTML:

```javascript
var divs = ucoreScraper.select(doc, "div.content");

for (var div : divs) {
    print(ucoreScraper.html(div));
}
```

---

## Fetching Content

### fetch(url)

Download URL content:

```javascript
var html = ucoreScraper.fetch("https://example.com");
var doc = ucoreScraper.parse(html);
```

### download(url, path)

Save file to disk:

```javascript
ucoreScraper.download("https://example.com/image.jpg", "local.jpg");
```

---

## Complete Web Scraping Example

```javascript
// Scrape product listings
function scrapeProducts(url) {
    var html = ucoreScraper.fetch(url);
    var doc = ucoreScraper.parse(html);
    
    var products = [];
    var items = ucoreScraper.select(doc, ".product");
    
    for (var item : items) {
        var product = map();
        
        // Get name
        var nameEl = ucoreScraper.select(item, ".product-name");
        if (length(nameEl) > 0) {
            product["name"] = ucoreScraper.text(nameEl[0]);
        }
        
        // Get price
        var priceEl = ucoreScraper.select(item, ".product-price");
        if (length(priceEl) > 0) {
            product["price"] = ucoreScraper.text(priceEl[0]);
        }
        
        // Get link
        var linkEl = ucoreScraper.select(item, "a");
        if (length(linkEl) > 0) {
            product["url"] = ucoreScraper.attr(linkEl[0], "href");
        }
        
        push(products, product);
    }
    
    return products;
}

var products = scrapeProducts("https://shop.example.com");
for (var p : products) {
    print(p["name"] + ": " + p["price"]);
}
```

---

## Common Patterns

### Extract All Links

```javascript
function extractLinks(doc) {
    var links = ucoreScraper.select(doc, "a");
    var result = [];
    
    for (var link : links) {
        var item = map();
        item["text"] = ucoreScraper.text(link);
        item["href"] = ucoreScraper.attr(link, "href");
        push(result, item);
    }
    
    return result;
}
```

### Extract Table Data

```javascript
function extractTable(doc, selector) {
    var rows = ucoreScraper.select(doc, selector + " tr");
    var data = [];
    
    for (var row : rows) {
        var cells = ucoreScraper.select(row, "td");
        var rowData = [];
        
        for (var cell : cells) {
            push(rowData, ucoreScraper.text(cell));
        }
        
        push(data, rowData);
    }
    
    return data;
}

var doc = ucoreScraper.parseFile("data.html");
var tableData = extractTable(doc, "table.data");
```

### Extract Images

```javascript
function extractImages(doc) {
    var images = ucoreScraper.select(doc, "img");
    var result = [];
    
    for (var img : images) {
        var item = map();
        item["src"] = ucoreScraper.attr(img, "src");
        item["alt"] = ucoreScraper.attr(img, "alt");
        push(result, item);
    }
    
    return result;
}
```

---

## Performance

Benchmarks on 370KB HTML file:

| Operation | Speed |
|-----------|-------|
| `parseFile` (class selector) | 339 ops/sec |
| `parseFile` (table rows) | 212 ops/sec |
| `select` (in-memory) | 128 ops/sec |
| `parseFile` (links) | 83 ops/sec |

---

## Examples

```javascript
// examples/corelib/scraper/demo.unna
print("=== Web Scraper Demo ===");

var html = "<html><body>";
html = html + "<h1>Page Title</h1>";
html = html + "<ul><li>Item 1</li><li>Item 2</li></ul>";
html = html + "<a href=\"/about\">About</a>";
html = html + "</body></html>";

var doc = ucoreScraper.parse(html);

// Get heading
var h1 = ucoreScraper.select(doc, "h1");
print("Title: " + ucoreScraper.text(h1[0]));

// Get list items
var items = ucoreScraper.select(doc, "li");
for (var item : items) {
    print("- " + ucoreScraper.text(item));
}

// Get link
var links = ucoreScraper.select(doc, "a");
print("Link: " + ucoreScraper.attr(links[0], "href"));
```

---

## Next Steps

- [ucoreHttp](ucore-http.md) - Fetch web pages
- [ucoreString](ucore-string.md) - Process extracted text
- [Overview](overview.md) - All libraries
