# Unnarize Core Libraries

This directory contains tutorials and examples for the built-in standard libraries of Unnarize. These modules are compiled into the interpreter and are always available.

## Available Modules

| Module | Variable Name | Description | Tutorial Path |
|--------|--------------|-------------|---------------|
| **Timer** | `ucoreTimer` | High-precision timing and benchmarking | [`timer/`](./timer/) |
| **System** | `ucoreSystem`| System interaction, CLI args, environment variables | [`system/`](./system/) |
| **UON** | `ucoreUon` | Streaming database parser for `.uon` files | [`uon/`](./uon/) |
| **HTTP** | `ucoreHttp` | HTTP Server and Client capabilities | [`http/`](./http/) |

## Getting Started

Each module directory contains a dedicated `*_demo.unna` file that serves as a runnable tutorial.

### Running Examples

**System Demo:**
```bash
./bin/unnarize examples/tutorial/corelib/system/system_demo.unna
```

**Timer / Benchmarking:**
```bash
./bin/unnarize examples/tutorial/corelib/timer/timer_demo.unna
```

**UON Database:**
```bash
./bin/unnarize examples/tutorial/corelib/uon/uon_demo.unna
```

**HTTP Server:**
```bash
./bin/unnarize examples/tutorial/corelib/http/http_server.unna
```
