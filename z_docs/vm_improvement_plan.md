# VM Improvement Plan

> Document Version: 2.0  
> Date: 2026-01-14  
> Status: In Progress

---

## Overview

This document outlines improvements to the Unnarize Virtual Machine:
1. **Loop optimizations** ✅ - Specialized opcodes for i++/i--
2. **Async/Await** ✅ - Fully implemented with event loop
3. **GC Improvements** 🔄 - In progress (see `z_docs/gc_improvement.md`)

---

## Current State Analysis

### Opcode System ✅ (Well-Optimized)

| Optimization | Status |
|-------------|--------|
| Computed goto dispatch | ✅ |
| Specialized int/float ops | ✅ |
| Fast local access | ✅ |
| Optimized calls | ✅ |
| OP_INC_LOCAL / OP_DEC_LOCAL | ✅ NEW |

### Async System ✅ (Implemented)

| Component | Status |
|-----------|--------|
| `Future` struct | ✅ |
| `OP_ASYNC_CALL` opcode | ✅ NEW |
| `OP_AWAIT` opcode | ✅ NEW |
| Future resolution | ✅ |

**Result:** Async/await now fully working!

### GC System 🔄 (In Progress)

| Feature | Status |
|---------|--------|
| Tri-color mark-sweep | ✅ |
| Stats tracking | ✅ NEW |
| Adaptive threshold | ✅ NEW |
| Incremental marking | 🔄 Next |

---

## Completed: Phase 1 - Loop Optimizations

| Change | File |
|--------|------|
| Added `OP_INC_LOCAL` | opcodes.h, interpreter.c |
| Added `OP_DEC_LOCAL` | opcodes.h, interpreter.c |
| Pattern detection | compiler.c |

**Result:** ~36% faster loops (50M → 68M ops/sec)

---

## Completed: Phase 2 - Async/Await

| Change | File |
|--------|------|
| `OP_ASYNC_CALL` opcode | opcodes.h, interpreter.c |
| `OP_AWAIT` opcode | opcodes.h, interpreter.c |
| Future handling | interpreter.c |
| Await compilation | compiler.c |

**Result:** 
- Before: `Got:   Got:` (broken)
- After: `Got: Data:API` ✅

---

## In Progress: Phase 3 - GC Improvements

See detailed plan: `z_docs/gc_improvement.md`

| Feature | Status |
|---------|--------|
| Stats (pause time, freed) | ✅ Done |
| Adaptive threshold | ✅ Done |
| Phase tracking | ✅ Done |
| Incremental marking | 🔄 Next |
| Write barriers | Planned |

---

## Verification

```bash
# Async test
unnarize examples/basics/11_async.unna
# Expected: Got: Data:API, Got: Data:Database ✅

# GC stress test
unnarize examples/garbagecollection/stress_test.unna
# Expected: ALL TESTS PASSED ✅

# Build verification
sudo make install 2>&1 | grep -E "(warning|error)" || echo "Clean!" ✅
```

---

## Summary

| Phase | Status | Improvement |
|-------|--------|-------------|
| Loop opcodes | ✅ Done | 36% faster |
| Async/Await | ✅ Done | 100% working |
| GC Phase 1 | ✅ Done | Stats + adaptive |
| GC Phase 2 | 🔄 In Progress | Incremental marking |

