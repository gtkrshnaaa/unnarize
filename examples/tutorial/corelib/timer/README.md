# Timer Module

The `ucoreTimer` module provides high-precision timing functions, essential for benchmarking algorithms and performance testing.

## API Reference

### `ucoreTimer.now()`
Returns the current system high-resolution timestamp in milliseconds.

- **Returns**: Float (milliseconds).

## Benchmarking Pattern

To measure code execution time:

1. Capture start time: `var start = ucoreTimer.now();`
2. Run your code.
3. Capture end time: `var end = ucoreTimer.now();`
4. Calculate difference: `var elapsed = end - start;`
