# GC Improvement Documentation

> Date: 2026-01-14  
> Version: 1.0

## Overview

This document details the Garbage Collector improvements implemented to support long-running processes (days/weeks) at Node.js/Go quality level.

---

## Changes Summary

| File | Changes |
|------|---------|
| `include/vm.h` | Added GC stats fields, gcPhase for incremental marking |
| `src/vm.c` | Initialize GC statistics |
| `src/gc.c` | Enhanced with timing, stats tracking, adaptive threshold |

---

## New GC Features

### 1. Statistics Tracking

| Metric | Description |
|--------|-------------|
| `gcCollectCount` | Total number of GC runs |
| `gcTotalPauseUs` | Total pause time (microseconds) |
| `gcLastPauseUs` | Most recent GC pause time |
| `gcTotalFreed` | Total bytes freed by GC |
| `gcPeakMemory` | Peak memory usage observed |

### 2. Adaptive Threshold

The GC now adjusts `nextGC` threshold based on heap pressure:

| Condition | Action |
|-----------|--------|
| Freed > 50% of heap | `nextGC = allocated * 3` (relax) |
| Freed < 20% of heap | `nextGC = allocated * 1.5` (trigger sooner) |
| Normal | `nextGC = allocated * 2` |
| Minimum | 256KB (avoids thrashing) |

### 3. GC Phase Tracking

| Phase | Value | Description |
|-------|-------|-------------|
| `GC_IDLE` | 0 | No GC running |
| `GC_MARKING` | 1 | Mark phase active |
| `GC_SWEEPING` | 2 | Sweep phase active |

---

## Implementation Details

### sweep() Enhancement

```c
static size_t sweep(VM* vm) {
    // Now returns bytes freed
    // Estimates object sizes for accurate tracking:
    // - OBJ_STRING: sizeof(ObjString) + length
    // - OBJ_ARRAY: sizeof(Array) + capacity * sizeof(Value)
    // etc.
}
```

### collectGarbage() Enhancement

```c
void collectGarbage(VM* vm) {
    uint64_t startTime = getCurrentTimeUs();
    
    // Track peak memory
    if (vm->bytesAllocated > vm->gcPeakMemory)
        vm->gcPeakMemory = vm->bytesAllocated;
    
    // Mark phase with phase tracking
    vm->gcPhase = GC_MARKING;
    markRoots(vm);
    traceReferences(vm);
    
    // Sweep phase
    vm->gcPhase = GC_SWEEPING;
    size_t freedBytes = sweep(vm);
    vm->gcPhase = GC_IDLE;
    
    // Update statistics
    uint64_t pauseTime = getCurrentTimeUs() - startTime;
    vm->gcCollectCount++;
    vm->gcTotalPauseUs += pauseTime;
    vm->gcLastPauseUs = pauseTime;
    vm->gcTotalFreed += freedBytes;
    
    // Adaptive threshold based on freed ratio
    // ...
}
```

---

## Future Improvements (Phase 4+)

| Feature | Description | Difficulty |
|---------|-------------|------------|
| Incremental Marking | Break marking into small steps | High |
| Write Barriers | Track mutations during marking | High |
| Generational GC | Separate nursery for young objects | Very High |
| Concurrent Marking | Background thread marking | Very High |

---

## Verification

```bash
# Test long-running allocation
cat > /tmp/gc_stress.unna << 'EOF'
var iterations = 10000;
for (var i = 0; i < iterations; i = i + 1) {
    var arr = array();
    for (var j = 0; j < 50; j = j + 1) {
        push(arr, "item" + j);
    }
}
print("Completed " + iterations + " iterations");
EOF
unnarize /tmp/gc_stress.unna
```

---

## Commits

1. `include/vm.h` - Add GCStats fields and gcPhase
2. `src/vm.c` - Initialize GC statistics 
3. `src/gc.c` - Enhanced with timing, stats, adaptive threshold
