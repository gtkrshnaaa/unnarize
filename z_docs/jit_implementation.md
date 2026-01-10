# JIT Compiler Infrastructure - Phase 2 Implementation

## Overview

This document describes the JIT (Just-In-Time) compiler infrastructure implemented for Unnarize Phase 2: Baseline JIT Compilation.

## Components Implemented

### 1. Executable Memory Manager (`src/jit/memory.c`, `include/jit/memory.h`)

Provides platform-specific executable memory allocation using POSIX `mmap`:

- **`allocateExecutableMemory(size)`**: Allocate RWX memory region
- **`freeExecutableMemory(ptr, size)`**: Free executable memory
- **`makeExecutable(ptr, size)`**: Change protection to RX (W^X security)
- **`makeWritable(ptr, size)`**: Change protection to RW (for patching)
- **`getPageSize()`**: Get system page size

**Features**:
- Zero external dependencies (uses POSIX APIs only)
- Implements W^X (Write XOR Execute) security principle
- Automatic page alignment (4KB boundaries)
- Error handling for allocation failures

### 2. x86-64 Assembler (`src/jit/assembler.c`, `include/jit/assembler.h`)

Manual instruction encoding for x86-64 machine code generation:

**Supported Instructions**:
- **Stack Operations**: `PUSH`, `POP`
- **Data Movement**: `MOV` (reg-reg, reg-imm32, reg-imm64, reg-mem, mem-reg)
- **Arithmetic**: `ADD`, `SUB`, `IMUL`, `IDIV`, `NEG`
- **Comparison**: `CMP`, `TEST`
- **Control Flow**: `JMP`, `JE`, `JNE`, `JL`, `JLE`, `JG`, `JGE`, `CALL`, `RET`
- **Special**: `NOP`, `INT3` (breakpoint)

**Features**:
- Manual instruction encoding (no external assembler)
- Support for REX prefix, ModR/M, SIB encoding
- Dynamic buffer management with automatic growth
- Jump patching for forward jumps

### 3. Template-Based JIT Compiler (`src/jit/jit_compiler.c`, `include/jit/jit_compiler.h`)

Compiles bytecode to native x86-64 code using 1:1 template mapping:

**Compilation Strategy**:
- **Register Allocation**: Top 4 stack values in RAX, RBX, RCX, RDX
- **Function Prolog**: Standard stack frame setup, save callee-saved registers
- **Bytecode Translation**: Direct 1:1 mapping from bytecode to assembly
- **Function Epilog**: Restore registers and stack frame, return

**Supported Opcodes** (Initial Implementation):
- `OP_LOAD_IMM`: Load immediate value
- `OP_ADD_II`, `OP_SUB_II`, `OP_MUL_II`: Integer arithmetic
- `OP_LOAD_LOCAL`, `OP_STORE_LOCAL`: Local variable access
- `OP_LOAD_LOCAL_0`, `OP_LOAD_LOCAL_1`: Fast local access
- `OP_LT_II`: Integer comparison
- `OP_JUMP`, `OP_JUMP_IF_FALSE`: Control flow
- `OP_RETURN`, `OP_RETURN_NIL`, `OP_HALT`: Function exit

**API**:
- **`compileFunction(vm, chunk)`**: Compile bytecode chunk to native code
- **`executeJIT(vm, jitFunc)`**: Execute JIT-compiled function
- **`freeJITFunction(jitFunc)`**: Free compiled function

## Build Integration

The JIT infrastructure is automatically compiled by the existing Makefile:

```makefile
SRCS_MAIN = $(wildcard $(SRC_DIR)/*.c) \
            $(wildcard $(SRC_DIR)/bytecode/*.c) \
            $(wildcard $(SRC_DIR)/jit/*.c) \        # JIT sources
            $(wildcard $(SRC_DIR)/runtime/*.c)
```

**Build Commands**:
```bash
make clean    # Clean build artifacts
make          # Build with JIT support
```

## Current Status

### ✅ Completed
- [x] Executable memory manager with mmap
- [x] x86-64 assembler with manual instruction encoding
- [x] Template-based JIT compiler core
- [x] Function prolog/epilog generation
- [x] Basic arithmetic and local variable support
- [x] Build system integration

### 🚧 In Progress
- [ ] Jump patching for forward jumps
- [ ] Complete opcode coverage
- [ ] VM integration (hotspot detection, JIT entry points)

### 📋 TODO (Next Steps)
- [ ] Integrate with bytecode interpreter
- [ ] Add hotspot counter in `OP_LOOP_HEADER`
- [ ] Implement JIT compilation trigger (threshold-based)
- [ ] Add JIT cache to VM structure
- [ ] Performance benchmarking
- [ ] Complete jump/branch handling

## Technical Details

### Calling Convention

JIT-compiled functions follow System V AMD64 ABI:
- **RDI**: VM pointer (first argument)
- **RAX**: Return value
- **Callee-saved**: RBX, R12-R15, RBP

### Memory Layout

```
Stack Frame:
+------------------+ <- RBP (frame pointer)
| Return Address   |
+------------------+
| Saved RBP        |
+------------------+ <- RBP - 8
| Local 0          |
+------------------+ <- RBP - 16
| Local 1          |
+------------------+
| ...              |
+------------------+ <- RSP (stack pointer)
```

### Security

- **W^X Enforcement**: Memory is either writable OR executable, never both
- **Page Alignment**: All allocations are page-aligned (4KB)
- **Zero Initialization**: Executable memory is zeroed before use

## Performance Target

**Phase 2 Goal**: 500M+ operations/sec (2.5x improvement over bytecode VM)

Current bytecode VM performance: ~200M ops/sec

## Platform Support

**Current**: Linux x86-64 only

**Future**: ARM64, Windows (Phase 3)

## References

- Intel 64 and IA-32 Architectures Software Developer's Manual
- System V AMD64 ABI Specification
- x86-64 Instruction Encoding Reference

## Commits

All JIT infrastructure files were committed atomically per file to branch `experiment`:

1. `feat(jit): add executable memory manager header`
2. `feat(jit): implement executable memory manager with mmap`
3. `feat(jit): add x86-64 assembler header`
4. `feat(jit): implement x86-64 assembler with manual instruction encoding`
5. `feat(jit): add baseline JIT compiler header`
6. `feat(jit): implement template-based JIT compiler for bytecode-to-native translation`
