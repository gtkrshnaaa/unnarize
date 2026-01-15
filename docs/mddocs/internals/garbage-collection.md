# Garbage Collection

> The "Gapless" Generational Concurrent Garbage Collector.

---

## Overview

Unnarize implements a sophisticated GC system comparable to Node.js V8 and Go:

| Feature | Unnarize | Node.js (V8) | Go |
|---------|----------|--------------|-----|
| Algorithm | Generational Mark-Sweep | Mark-Sweep-Compact | Tri-color Mark-Sweep |
| Tri-color invariant | ✅ | ✅ | ✅ |
| Generational | ✅ | ✅ | ❌ |
| Concurrent Marking | ✅ | ✅ | ✅ |
| Parallel Sweeping | ✅ | ✅ | ✅ |

---

## "Gapless" Architecture

The term "Gapless" refers to the elimination of noticeable execution pauses:

```
┌──────────────────────────────────────────────────┐
│                  HEAP MEMORY                      │
├──────────────────────┬───────────────────────────┤
│   NURSERY (Young)    │    OLD GENERATION         │
│                      │                           │
│  • New allocations   │  • Promoted survivors     │
│  • Frequent collect  │  • Background sweep       │
│  • Instant sweep     │  • Concurrent mark        │
└──────────────────────┴───────────────────────────┘
```

---

## Generational Heap

### Nursery (Young Generation)

- All new objects are allocated here
- Collected frequently (every N allocations)
- Most objects die young → sweep is fast

### Old Generation

- Objects that survive a nursery collection
- Collected less frequently
- Marking happens in background thread

### Promotion

```c
// After marking, survivors move to old gen
if (obj->generation == 0 && obj->isMarked) {
    obj->generation = 1;
    // Move from vm->nursery to vm->objects
}
```

---

## Tri-Color Marking

Objects are in one of three states:

| Color | Meaning | isMarked | In Gray Stack |
|-------|---------|----------|---------------|
| White | Not visited | false | no |
| Gray | Visited, children pending | true | yes |
| Black | Fully processed | true | no |

### Algorithm

```
1. Mark roots as Gray
2. While Gray stack not empty:
   a. Pop object from Gray stack
   b. Mark all children as Gray
   c. Object becomes Black (implicitly)
3. Sweep: Free all White objects
```

---

## Concurrent Marking

Marking happens in a background thread while the main thread continues execution:

```c
void* concurrentMarkWorker(void* arg) {
    VM* vm = ((ConcurrentGCArgs*)arg)->vm;
    
    // Phase 1: Mark roots (requires brief pause)
    pthread_mutex_lock(&gcMutex);
    markRoots(vm);
    pthread_mutex_unlock(&gcMutex);
    
    // Phase 2: Trace references (concurrent)
    while (vm->grayCount > 0) {
        traceReferencesIncremental(vm, 100);
        // Allow main thread to run
    }
    
    // Phase 3: Sweep (concurrent)
    sweep(vm, &vm->objects);
    
    return NULL;
}
```

---

## Write Barriers

When the main thread modifies objects during concurrent marking, we must maintain the tri-color invariant:

**Problem**: If a Black object gains a reference to a White object, the White object might be incorrectly collected.

**Solution**: Write barrier re-grays the Black object.

```c
#define WRITE_BARRIER(vm, obj) \
    do { \
        if (isGCActive() && ((Obj*)(obj))->isMarked) { \
            grayObject((vm), (Obj*)(obj)); \
        } \
    } while(0)
```

### Usage in Interpreter

```c
op_array_push:
    arrayPush(vm, arr, val);
    WRITE_BARRIER(vm, arr);  // Re-scan if arr was Black
    DISPATCH();
```

---

## Allocate Black

New objects created during marking are pre-marked to avoid collection:

```c
Obj* allocateObject(VM* vm, size_t size, ObjType type) {
    Obj* object = (Obj*)malloc(size);
    object->type = type;
    
    // Allocate Black: mark if GC is active
    object->isMarked = (vm->gcPhase == 1 || isGCActive());
    
    // Add to nursery
    object->next = vm->nursery;
    vm->nursery = object;
    vm->nurseryCount++;
    
    return object;
}
```

---

## Thread Safety

Critical sections protected by mutex:

```c
static pthread_mutex_t gcMutex = PTHREAD_MUTEX_INITIALIZER;

void markObject(VM* vm, Obj* object) {
    if (object == NULL) return;
    
    // Lock if concurrent GC is active
    if (gcConcurrentActive) pthread_mutex_lock(&gcMutex);
    
    if (object->isMarked) {
        if (gcConcurrentActive) pthread_mutex_unlock(&gcMutex);
        return;
    }
    
    object->isMarked = true;
    // Push to gray stack...
    
    if (gcConcurrentActive) pthread_mutex_unlock(&gcMutex);
}
```

---

## Incremental Collection

For low-latency applications, GC work can be spread across frames:

```c
bool collectGarbageIncremental(VM* vm, int workUnits) {
    switch (vm->gcPhase) {
        case 0: // Idle
            markRoots(vm);
            vm->gcPhase = 1;
            return false;
            
        case 1: // Marking
            int remaining = traceReferencesIncremental(vm, workUnits);
            if (remaining == 0) {
                vm->gcPhase = 2;
            }
            return false;
            
        case 2: // Sweeping
            sweep(vm, &vm->objects);
            vm->gcPhase = 0;
            return true;
    }
}
```

---

## Root Set

The GC scans these as starting points:

1. **Value Stack** - All values on the VM stack
2. **Call Stack** - Function objects in call frames
3. **Global Environment** - Global variables
4. **String Pool** - Interned strings
5. **Module Cache** - Loaded modules

```c
void markRoots(VM* vm) {
    // Stack
    for (int i = 0; i < vm->stackTop; i++) {
        markValue(vm, vm->stack[i]);
    }
    
    // Call frames
    for (int i = 0; i < vm->callStackTop; i++) {
        markObject(vm, (Obj*)vm->callStack[i].function);
    }
    
    // Globals
    markObject(vm, (Obj*)vm->globalEnv);
}
```

---

## Statistics Tracking

```c
struct VM {
    // GC Statistics
    uint64_t gcCollectCount;     // Total GC runs
    uint64_t gcTotalPauseUs;     // Total pause time
    uint64_t gcLastPauseUs;      // Last GC pause
    uint64_t gcTotalFreed;       // Bytes freed
    size_t gcPeakMemory;         // Peak usage
};
```

---

## Verification Results

Tested under stress conditions:

| Test | Load | Result |
|------|------|--------|
| Massive Allocation | 50,000 arrays | Stable |
| Long Running | 15,000 requests | <10ms pause |
| Real World | SME Store System | No crashes |

---

## Configuration

```c
// Threshold for triggering GC
vm->nurseryThreshold = 1000;  // Objects
vm->nextGC = 1024 * 1024;     // Bytes

// Adaptive threshold
if (freed > allocated * 0.5) {
    vm->nextGC *= 2;  // Less frequent GC
} else {
    vm->nextGC /= 2;  // More frequent GC
}
```

---

## Performance Tips

1. **Reuse objects** when possible instead of creating new ones
2. **Use structs** for related data to reduce object count
3. **Avoid deep nesting** which increases marking time
4. **Process in batches** for streaming data

---

## Next Steps

- [Architecture](architecture.md) - Overall VM structure
- [Bytecode](bytecode.md) - GC-related opcodes
- [NaN Boxing](nan-boxing.md) - Value representation
