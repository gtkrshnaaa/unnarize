# Unnarize Development Rules

Strict guidelines for contributing to Unnarize. Adherence is mandatory to maintain code quality and performance.

## 1. General Philosophy
- **Pure C**: Use C11 standard. No C++ features.
- **Zero Dependencies**: Do not introduce external libraries. The `Makefile` must work with standard GCC/Clang and libc.
- **Performance First**: Every change must be verified against `examples/benchmarks/`. No regressions allowed.

## 2. Code Style
- **Language**: English only for variables, functions, comments, and file names.
- **Naming**: 
    - Variables/Functions: `camelCase` (e.g., `internString`, `compileNode`).
    - Structs/Enums: `PascalCase` (e.g., `ObjString`, `OpCode`).
    - Constants/Macros: `SCREAMING_SNAKE_CASE` (e.g., `STACK_MAX`).
- **Formatting**:
    - Indentation: 4 spaces.
    - Braces: Same line (`K&R` style).

## 3. Workflow & Git Strategy
- **Branching**: 
    - `main`: Stable releases only.
    - `experiment` / `dev`: Active development.
- **Commits**:
    - **Atomic Commits**: Commit **per file** or **per logical change**. Do not bundle unrelated changes.
    - **Message Format**: English, imperative mood.
        - `feat(vm): add stack resizing`
        - `fix(compiler): resolve global variable scope issue`
        - `docs: add development plan`
    - **User**:
        ```bash
        git config --local user.email "gtkrshnaaa@gmail.com"
        git config --local user.name "gtkrshnaaa"
        ```

## 4. Build System & commands
- **Clean Build**: Always ensure the build environment is clean before release.
    ```bash
    make clean    # Remove all artifacts
    ```
- **Compile**:
    ```bash
    make          # Build 'unnarize' binary in bin/
    ```
- **Install (Fresh)**:
    ```bash
    sudo make install  # Cleans, builds, and installs to /usr/local/bin
    ```

## 5. Directory Structure
Code must be placed in the correct tier directory:
- `src/bytecode/`: Interpreter, Opcodes, Chunk management.
- `src/jit/`: Native code generation, Assembly emitters.
- `src/runtime/`: GC, Object primitives (Strings, Maps), Builtins.
- `include/`: Public headers matching `src/` structure.

## 6. Testing
- Run **all** benchmarks before pushing:
    ```bash
    unnarize examples/benchmarks/run_all_benchmarks.unna
    ```
- Verify no memory leaks or segfaults.
