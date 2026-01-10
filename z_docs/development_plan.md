# Unnarize Development Roadmap

This document outlines the strategic development plan for the Unnarize JIT compiler, targeting world-class performance (1.2B+ ops/sec).

## 🎯 Goal
Transform Unnarize into a **Full Native JIT-compiled language** with industry-leading performance.

**Performance Targets:**
- **Baseline**: 21M ops/sec (Tree-walker) - ✅ COMPLETED
- **Tier 1**: 800M ops/sec (Bytecode VM) - ✅ COMPLETED
- **Tier 2**: **1.2B ops/sec (Full Native JIT)** - 🚧 IN PROGRESS
- **Tier 3**: 2B+ ops/sec (Optimizing JIT) - FUTURE

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

**Result**: 800M ops/sec on compute-intensive code

---

### Phase 2: Full Native JIT Compilation 🚧 IN PROGRESS
**Goal**: **Replace bytecode interpreter with native x86-64 code generation for ALL hot paths.**

This is a **complete transformation** to Full Native JIT:
- All hot loops compiled to native code
- Automatic hotspot detection and compilation
- Seamless interpreter-to-JIT switching
- Complete opcode coverage in JIT compiler
- Advanced optimizations (register allocation, inline caching, constant folding)

**Target**: **1.2 billion operations/sec** (1.5x improvement over bytecode VM)

#### Infrastructure ✅ COMPLETED
- ✅ Executable memory manager (`mmap` RWX, W^X security)
- ✅ x86-64 machine code encoder (25+ instructions, manual encoding)
- ✅ Template-based JIT compiler core
- ✅ Function prolog/epilog generation

#### VM Integration 🚧 IN PROGRESS
- 🚧 Add JIT state to VM structure
- 🚧 Implement hotspot detection in interpreter
- 🚧 Automatic compilation trigger (threshold-based)
- 🚧 JIT cache management
- 🚧 Seamless interpreter-to-JIT switching

#### Complete Opcode Coverage 📋 TODO
- 📋 All arithmetic operations (ADD, SUB, MUL, DIV, MOD, NEG)
- 📋 All comparison operations (LT, LE, GT, GE, EQ, NE)
- 📋 Global variable access (LOAD/STORE/DEFINE)
- 📋 Function calls (CALL, CALL_0/1/2, native calls)
- 📋 Control flow (JUMP, JUMP_IF_FALSE/TRUE, LOOP)
- 📋 Stack operations (POP, DUP)

#### Jump Patching 📋 TODO
- 📋 Two-pass compilation
- 📋 Jump target tracking
- 📋 Forward jump patching
- 📋 Backward jump support (loops)

#### Optimizations for 1.2B ops/sec 📋 TODO
- 📋 Extended register allocation (8 registers: RAX-R9)
- 📋 Inline caching for property access
- 📋 Constant folding at compile time
- 📋 Loop unrolling (4x for small loops)

#### Testing & Verification 📋 TODO
- 📋 Benchmark with ucoreTimer
- 📋 Verify 1.2B ops/sec target on simple loops
- 📋 Verify 1.0B ops/sec on arithmetic
- 📋 Memory safety (valgrind clean)

---

### Phase 3: Optimizing JIT (Tier 3) - FUTURE
**Goal**: Apply advanced compiler optimizations using an Intermediate Representation (IR).

**Target**: 2B+ ops/sec

- **Profiler**:
    - Type profiling (polymorphism detection)
    - Advanced hotspot detection
- **Optimization Pipeline**:
    - SSA-based IR generation
    - Type specialization / monomorphization
    - Inlining of small functions
    - Loop unrolling and invariant code motion
    - Advanced register allocation (Graph coloring)
    - Dead code elimination

---

### Phase 4: Advanced Features - FUTURE
- **On-Stack Replacement (OSR)**: Switch to JIT code mid-loop
- **Deoptimization**: Bail out when speculative assumptions fail
- **SIMD**: Auto-vectorization for array operations
- **Garbage Collection**: Generational, incremental GC
- **Multi-tier JIT**: Baseline → Optimizing tiers

---

## 📂 Project Structure

```
src/
  bytecode/     - Bytecode compiler and interpreter (Tier 1)
  jit/          - Native Code Generation (Tier 2/3)
    memory.c    - Executable memory manager
    assembler.c - x86-64 instruction encoder
    jit_compiler.c - JIT compilation engine
  runtime/      - GC, Objects, Standard Library
  vm.c          - VM core and type definitions
```

---

## 🎯 Current Focus: Phase 2 - Full Native JIT

**Objective**: Transform Unnarize to use native code generation for all hot execution paths.

**Key Principle**: This is NOT a hybrid approach. Hot code runs as **native x86-64 machine code**, not bytecode.

**Implementation Strategy**:
1. ✅ Build JIT infrastructure (memory, assembler, compiler core)
2. 🚧 Integrate JIT into VM (hotspot detection, automatic compilation)
3. 📋 Complete opcode coverage (all operations in native code)
4. 📋 Implement jump patching (proper control flow)
5. 📋 Add optimizations (register allocation, inline caching)
6. 📋 Benchmark and tune to hit 1.2B ops/sec target

**Success Criteria**:
- ✅ Simple loop: ≥ 1.2B ops/sec
- ✅ Arithmetic: ≥ 1.0B ops/sec
- ✅ Function calls: ≥ 500M ops/sec
- ✅ All examples work correctly
- ✅ No memory leaks

---

## 📊 Performance Evolution

| Phase | Execution Model | Performance | Status |
|-------|----------------|-------------|--------|
| Phase 0 | Tree-walking interpreter | 21M ops/sec | ✅ Done |
| Phase 1 | Bytecode VM | 800M ops/sec | ✅ Done |
| **Phase 2** | **Full Native JIT** | **1.2B ops/sec** | **🚧 In Progress** |
| Phase 3 | Optimizing JIT | 2B+ ops/sec | 📋 Future |

---

## 🚀 Next Steps

1. Complete VM integration (hotspot detection, JIT cache)
2. Implement jump patching (two-pass compilation)
3. Add remaining opcodes (DIV, MOD, globals, calls)
4. Optimize register allocation (8 registers)
5. Add inline caching for properties
6. Benchmark and tune to 1.2B ops/sec
7. Document and release Phase 2

**Estimated Time**: 13-18 hours of focused development
