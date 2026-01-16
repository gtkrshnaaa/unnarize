# Installation

> Build and run Unnarize on your system.

---

## Prerequisites

Ensure you have a standard C development environment:

- **gcc** (GNU Compiler Collection) - Version 7+ recommended
- **make** - Build automation tool
- **git** - Version control

### Check Installation

```bash
gcc --version
make --version
git --version
```

---

## Build from Source

### Clone Repository

```bash
git clone https://github.com/gtkrshnaaa/unnarize.git
cd unnarize
```

### Compile

```bash
make
```

This creates the `./bin/unnarize` executable with:
- Optimization flags: `-O3 -march=native -mtune=native`
- C11 standard
- pthread support

### Clean Build (Optional)

```bash
make clean && make
```

---

## System Installation (Optional)

Install `unnarize` to `/usr/local/bin/` for system-wide access:

```bash
sudo make install
```

Verify installation:

```bash
unnarize -v
# Output: unnarize 0.1.6-beta
```

### Uninstall

```bash
sudo make uninstall
```

---

## Running Scripts

### Basic Usage

```bash
./bin/unnarize path/to/script.unna
```

Or if installed system-wide:

```bash
unnarize path/to/script.unna
```

### Try Examples

```bash
# Variables and types
./bin/unnarize examples/basics/01_variables.unna

# Functions
./bin/unnarize examples/basics/09_functions.unna

# Async/Await
./bin/unnarize examples/basics/11_async.unna

# Full application demo
./bin/unnarize examples/testcase/main.unna

# Performance benchmark
./bin/unnarize examples/benchmark/benchmark.unna
```

### Run Core Library Examples

```bash
# String manipulation
cd examples/corelib/string && ../../../bin/unnarize benchmark.unna

# Web scraper
cd examples/corelib/scraper && ../../../bin/unnarize benchmark.unna

# HTTP server
./bin/unnarize examples/corelib/http/server.unna
```

---

## Project Structure

```
unnarize/
├── bin/                  # Built executable
├── examples/             # Example scripts
│   ├── basics/           # Core language demos (11 files)
│   ├── benchmark/        # Performance testing
│   ├── corelib/          # Library examples
│   │   ├── http/         # HTTP client/server
│   │   ├── json/         # JSON parsing
│   │   ├── scraper/      # Web scraping
│   │   ├── string/       # String ops
│   │   ├── system/       # File/shell
│   │   ├── timer/        # Timing
│   │   └── uon/          # UON format
│   ├── garbagecollection/# GC stress tests
│   └── testcase/         # SME Store demo
├── include/              # Header files
│   └── bytecode/         # Opcode definitions
├── src/                  # Source files
│   ├── bytecode/         # Compiler, interpreter
│   ├── lexer.c           # Tokenizer
│   ├── parser.c          # AST builder
│   ├── vm.c              # Virtual machine
│   └── gc.c              # Garbage collector
├── corelib/              # Native C libraries
│   ├── include/          # Library headers
│   └── src/              # Library implementations
├── docs/                 # Documentation
├── Makefile              # Build system
└── README.md             # Project overview
```

---

## Makefile Targets

| Target | Description |
|--------|-------------|
| `make` | Build the executable |
| `make clean` | Remove build artifacts |
| `make install` | Install to `/usr/local/bin` |
| `make uninstall` | Remove system installation |
| `make list_source` | Generate source listing |
| `make core_list` | Generate core source listing |

---

## Compiler Flags

The build uses aggressive optimization:

```makefile
CFLAGS = -Wall -Wextra -std=c11 -O3 -march=native -mtune=native
LDFLAGS = -ldl -lpthread -Wl,-export-dynamic
```

| Flag | Purpose |
|------|---------|
| `-O3` | Maximum optimization |
| `-march=native` | CPU-specific instructions |
| `-mtune=native` | CPU-specific tuning |
| `-lpthread` | Threading support |
| `-ldl` | Dynamic loading |

---

## Troubleshooting

### Missing pthread

```
error: pthread.h: No such file or directory
```

Install development libraries:
```bash
# Ubuntu/Debian
sudo apt-get install build-essential

# Fedora
sudo dnf install gcc make
```

### Permission Denied

```bash
chmod +x ./bin/unnarize
```

---

## Next Steps

- [Variables](../language/variables.md) - Learn variable declaration
- [Functions](../language/functions.md) - Define reusable code
- [Examples](../examples/basics.md) - Explore example scripts
