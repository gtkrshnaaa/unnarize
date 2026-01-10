# Unnarize Development Roadmap

This document outlines the strategic development plan for the Unnarize JIT compiler, targeting world-class performance (1B+ ops/sec).

## 🎯 Goal
Transform Unnarize from a tree-walking interpreter into a high-performance JIT-compiled language.

**Performance Targets:**
- **Baseline**: 21M ops/sec (Tree-walker) - [COMPLETED]
- **Tier 1**: 200M ops/sec (Bytecode VM) - [CURRENT]
- **Tier 2**: 500M ops/sec (Baseline JIT)
- **Tier 3**: 1B+ ops/sec (Optimizing JIT)

---

## 📅 Phases

### Phase 1: Bytecode Foundation [COMPLETED]
**Goal**: Replace AST tree-walking with a flat, cache-friendly bytecode VM.
- [x] Design stack-based bytecode instruction set.
- [x] Implement AST-to-Bytecode compiler.
- [x] Build efficient direct-threaded interpreter loop.
- [x] Implement global/local variable scoping and stack management.
- [x] Optimize string handling (Interning).
- [x] Verify stability with comprehensive benchmarks.
- [x] Clean compilation and installation system.

### Phase 2: Baseline JIT Compilation [NEXT]
**Goal**: Emit native x86-64 machine code for hot paths to bypass interpreter overhead.
- [ ] **Infrastructure**:
    - [ ] Create executable memory manager (`mmap` RWX).
    - [ ] Implement x86-64 machine code encoder (Assembler).
- [ ] **Compilation**:
    - [ ] Template-based JIT (1:1 mapping from Bytecode to Assembly).
    - [ ] Implement function prologs/epilogs (Stack frame setup).
    - [ ] Handle control flow (Jumps/Branches) in native code.
    - [ ] Support native C function calls (`dlopen`/`dlsym`).
- [ ] **Integration**:
    - [ ] Add JIT entry points in Bytecode VM.
    - [ ] Implement seamless switching between Interpreter and JIT code.

### Phase 3: Optimizing JIT (Tier 3)
**Goal**: Apply advanced compiler optimizations using an Intermediate Representation (IR).
- [ ] **Profiler**:
    - [ ] Implement type profiling (polymorphism detection).
    - [ ] Hotspot detection (loop counters).
- [ ] **Optimization Pipeline**:
    - [ ] SSA-based IR generation.
    - [ ] Type specialization / monomorphization.
    - [ ] Inlining of small functions.
    - [ ] Loop unrolling and invariant code motion.
    - [ ] Register allocation (Graph coloring or Linear Scan).

### Phase 4: Advanced Features
- [ ] **On-Stack Replacement (OSR)**: Switch to JIT code in the middle of hot loops.
- [ ] **Deoptimization**: Bail out to interpreter when speculative assumptions fail.
- [ ] **SIMD**: Auto-vectorization for array operations.
- [ ] **Garbage Collection**: Generational, incremental GC.

---

## 📂 Project Structure

```
src/
  bytecode/     - VM and Interpreter (Tier 1)
  jit/          - Native Code Generation (Tier 2/3)
  runtime/      - GC, Objects, Standard Library
  vm.c          - Main entry and type definitions
```
