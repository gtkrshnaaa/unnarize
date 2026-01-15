# Unnarize Documentation

> Comprehensive documentation for the Unnarize programming language.

---

## Quick Navigation

| Section | Description |
|---------|-------------|
| [Getting Started](getting-started/introduction.md) | Overview, installation, first steps |
| [Language Reference](language/variables.md) | Complete language syntax guide |
| [Core Libraries](core-libraries/overview.md) | Built-in library API reference |
| [Internals](internals/architecture.md) | VM architecture, bytecode, GC |
| [Examples](examples/basics.md) | Annotated code examples |

---

## Getting Started

- [Introduction](getting-started/introduction.md) - What is Unnarize, key features, philosophy
- [Installation](getting-started/installation.md) - Build, install, run your first script

---

## Language Reference

| Topic | Description |
|-------|-------------|
| [Variables](language/variables.md) | Types, declaration, scope |
| [Operators](language/operators.md) | Arithmetic, comparison, logical |
| [Arrays](language/arrays.md) | Array operations, built-in functions |
| [Structs](language/structs.md) | Custom data structures |
| [Control Flow](language/control-flow.md) | if/else, while, for, foreach |
| [Functions](language/functions.md) | Declaration, recursion, closures |
| [Modules](language/modules.md) | Import system |
| [Async/Await](language/async-await.md) | Asynchronous programming |

---

## Core Libraries

| Library | Description |
|---------|-------------|
| [Overview](core-libraries/overview.md) | All libraries at a glance |
| [ucoreString](core-libraries/ucore-string.md) | String manipulation |
| [ucoreScraper](core-libraries/ucore-scraper.md) | HTML parsing, web scraping |
| [ucoreJson](core-libraries/ucore-json.md) | JSON parse/stringify |
| [ucoreHttp](core-libraries/ucore-http.md) | HTTP client and server |
| [ucoreTimer](core-libraries/ucore-timer.md) | High-precision timing |
| [ucoreSystem](core-libraries/ucore-system.md) | File I/O, shell, environment |
| [ucoreUon](core-libraries/ucore-uon.md) | UON data format |

---

## Internals

| Topic | Description |
|-------|-------------|
| [Architecture](internals/architecture.md) | VM pipeline and components |
| [Bytecode](internals/bytecode.md) | Opcode reference (~100 opcodes) |
| [Garbage Collection](internals/garbage-collection.md) | Generational concurrent GC |
| [NaN Boxing](internals/nan-boxing.md) | Value representation |

---

## Examples

| Demo | Description |
|------|-------------|
| [Basics](examples/basics.md) | 11 fundamental examples |
| [SME System](examples/sme-system.md) | Complete store management application |

---

## Performance

| Benchmark | Result |
|-----------|--------|
| Heavy Loop | ~275 M ops/sec |
| Fibonacci(30) | ~70 ms |
| GC Stress (50K objects) | <10ms pause |
| String contains (14KB) | 24.5 M ops/sec |

---

## Quick Start

```bash
# Clone and build
git clone https://github.com/gtkrshnaaa/unnarize.git
cd unnarize && make

# Run example
./bin/unnarize examples/basics/01_variables.unna

# Install system-wide (optional)
sudo make install
```

---

## License

MIT License. Created by **gtkrshnaaa**.
