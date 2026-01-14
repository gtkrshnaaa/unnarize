# VM Improvement Plan

> Document Version: 1.0  
> Date: 2026-01-13  
> Status: Planning

---

## Overview

This document outlines planned improvements to the Unnarize Virtual Machine, focusing on:
1. **Loop optimizations** - Add specialized opcodes for common patterns
2. **Async/Await fix** - Implement proper event loop (or remove broken feature)

---

## Current State Analysis

### Opcode System ✅ (Already Well-Optimized)

| Optimization | Status |
|-------------|--------|
| Computed goto dispatch | ✅ Implemented |
| Specialized int ops (`OP_ADD_II`) | ✅ Implemented |
| Specialized float ops (`OP_ADD_FF`) | ✅ Implemented |
| Fast local access (`OP_LOAD_LOCAL_0/1/2`) | ✅ Implemented |
| Optimized calls (`OP_CALL_0/1/2`) | ✅ Implemented |
| Branch hints (`likely()`/`unlikely()`) | ✅ Implemented |

### Async System ❌ (Not Implemented)

| Component | Status |
|-----------|--------|
| `Future` struct | ✅ Exists in vm.h |
| `Function.isAsync` field | ✅ Exists |
| Async/await syntax parsing | ✅ Works |
| `OP_AWAIT` opcode | ❌ Missing |
| Event loop | ❌ Missing |
| Future resolution | ❌ Missing |

**Result:** `async`/`await` keywords are parsed but functions run synchronously.

---

## Phase 1: Loop Optimizations

### 1.1 Add OP_INC_LOCAL

**Purpose:** Optimize `i = i + 1` pattern in loops

**Current bytecode for `i++`:**
```
OP_LOAD_LOCAL [slot]
OP_LOAD_IMM 1
OP_ADD_II
OP_STORE_LOCAL [slot]
```

**Proposed bytecode:**
```
OP_INC_LOCAL [slot]
```

**Implementation:**
- Add `OP_INC_LOCAL` to `opcodes.h`
- Add handler in `interpreter.c`
- Detect pattern in `compiler.c`

### 1.2 Add OP_DEC_LOCAL

Same as above for `i = i - 1`.

### 1.3 Files to Modify

| File | Changes |
|------|---------|
| `include/bytecode/opcodes.h` | Add `OP_INC_LOCAL`, `OP_DEC_LOCAL` |
| `src/bytecode/opcodes.c` | Add opcode info |
| `src/bytecode/interpreter.c` | Add dispatch handlers |
| `src/bytecode/compiler.c` | Detect pattern, emit optimized op |

---

## Phase 2: Async Fix (Decision Required)

### Option A: Remove Async (Recommended for Now)

**Pros:** Simple, honest, no false promises  
**Cons:** Loses marketing appeal of "async support"

**Implementation:**
1. Add compile-time warning when `async`/`await` detected
2. Update documentation to say "Not yet implemented"
3. Remove from examples or mark as experimental

### Option B: Implement Simple Coroutines

**Scope:** Single-threaded cooperative multitasking

**Components needed:**
1. `OP_ASYNC_CALL` - Create Future, schedule execution
2. `OP_AWAIT` - Yield if Future not resolved
3. Task queue in VM
4. Run loop to process pending tasks

**Estimated work:** 5-7 days

### Option C: Thread Pool (Complex)

Use pthreads (already has Future with mutex/condvar).

**Estimated work:** 10+ days, significant complexity

---

## Verification Plan

### Loop Optimization Tests

```bash
# Test 1: Run timer benchmark (uses loops)
unnarize examples/corelib/timer/demo.unna

# Expected: Should see ops/sec numbers
# Compare before/after optimization
```

```bash
# Test 2: Create specific loop benchmark
cat > /tmp/loop_test.unna << 'EOF'
var sum = 0;
var start = ucoreTimer.now();
for (var i = 0; i < 1000000; i++) {
    sum = sum + 1;
}
var elapsed = ucoreTimer.now() - start;
print("Time: " + elapsed + "ms");
print("Sum: " + sum);
EOF
unnarize /tmp/loop_test.unna

# Expected: Time should decrease after OP_INC_LOCAL
```

### Async Tests (if implementing Option B)

```bash
# Test: Run async example
unnarize examples/basics/11_async.unna

# Expected BEFORE fix:
#   Got:   Got:     (broken, values lost)

# Expected AFTER fix:
#   Got: Data:API
#   Got: Data:Database
```

### Build Verification

```bash
# Must compile without warnings
sudo make install 2>&1 | grep -E "(warning|error)" || echo "Clean build!"
```

---

## Implementation Order

1. [x] **Phase 1.1**: Add `OP_INC_LOCAL` opcode
2. [x] **Phase 1.2**: Add `OP_DEC_LOCAL` opcode
3. [x] **Phase 1.3**: Detect `i = i + 1` pattern in compiler
4. [x] **Test**: Verify loop benchmark improvement (50M → 68M ops/sec, ~36% faster!)
5. [x] **Phase 2**: Async warning added (documents as experimental)
6. [x] **Commit**: All changes pushed to experiment branch

---

## Results

| Metric | Before | After | Improvement |
|--------|--------|-------|-------------|
| 1M iterations | 19.9ms | 14.6ms | **27% faster** |
| Ops/sec | 50M | 68M | **36% increase** |

---

## Questions for Review

1. **Async approach:** Should we implement coroutines (Option B) or remove async for now (Option A)?
2. **Additional opcodes:** Any other patterns worth optimizing?
