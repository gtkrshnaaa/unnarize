# Unnarize Modularity

Current Unnarize version supports modularity via **Native Plugins** (.so files loaded at runtime).
The high-level `import` statement for `.unna` source files is currently in development.

## Using Native Modules

You can load C-compiled shared libraries using `loadextern`.
See the internal examples for how to build and use plugins:

- Source: `examples/plugins/src/`
- Examples: `examples/plugins/`

### Example Workflow

1. Write a C module (e.g. `mathLib.c`)
2. Compile to `.so` (shared object)
3. Load in Unnarize:
   ```c
   loadextern "path/to/mathLib.so";
   
   print(Math.add(10, 20)); // implementation provided by C
   ```

*Note: Check `examples/plugins/README.md` (if available) or the Makefile for build instructions.*
