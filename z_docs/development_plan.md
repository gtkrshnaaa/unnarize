# Unnarize Development Roadmap

This document outlines the strategic development plan for the Unnarize Interpreter.

## 🎯 Goal
Transform Unnarize into a **High-Performance Bytecode Interpreter** with zero dependencies.

**Performance Targets:**
- **Baseline**: 21M ops/sec (Tree-walker) - ✅ COMPLETED
- **Current**: **162M ops/sec (Bytecode VM)** - ✅ COMPLETED

---

## 📅 Phases

### Phase 1: Bytecode Foundation ✅ COMPLETED
**Goal**: Replace AST tree-walking with a flat, cache-friendly bytecode VM.
- ✅ Design stack-based bytecode instruction set
- ✅ Implement AST-to-Bytecode compiler
- ✅ Build efficient direct-threaded interpreter loop
- ✅ Implement global/local variable scoping and stack management
- ✅ Optimize string handling (Interning)
- ✅ Verify stability with comprehensive benchmarks
- ✅ Clean compilation and installation system

**Result**: ~162M ops/sec on compute-intensive code

---

## 📂 Project Structure

```
src/
  bytecode/     - Bytecode compiler and interpreter
  runtime/      - GC, Objects, Standard Library
  vm.c          - VM core and type definitions
```

---

## 📊 Performance Evolution

| Phase | Execution Model | Performance | Status |
|-------|----------------|-------------|--------|
| Phase 0 | Tree-walking interpreter | 21M ops/sec | ✅ Done |
| **Phase 1** | **Bytecode VM** | **~162M ops/sec** | **✅ Done (Optimized)** |

---

## 🚀 Next Steps

1. Further optimization of Bytecode VM (threading, register allocation in VM)
2. Enhance Standard Library (Networking, IO)
3. Improve Tooling (Debugger, Profiler)
