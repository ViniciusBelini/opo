# Breaking Changes

This document tracks all backward-incompatible changes made to the Opo Programming Language since its initial development. Understanding these changes will help you migrate your Opo code as the language evolves.

## Roadmap to 1.0

### Symbolic Syntax Refactoring
The most significant shift in Opo's early development was moving from keyword-heavy syntax (like `if`, `else`, `while`) to the current symbolic operators (`?`, `:`, `@`).
- **Old Syntax**: `if (x > 0) { ... }`
- **New Syntax**: `x > 0 ? [ ... ]`
- **Rationale**: To better align with the "Opposite of Python" philosophy and create a more concise, expert-oriented grammar.

### Assignment Operator Change
Switched from traditional `=` or `:=` to the `=>` operator for both initial declarations and reassignments.
- **Old Syntax**: `int x = 10`
- **New Syntax**: `10 => x: int`
- **Rationale**: To emphasize value-to-variable flow and clear up assignment vs. equality (`==`).

### Mandatory Static Typing
The compiler no longer allows undeclared or dynamically typed variables (except for the explicitly defined `any` type).
- **Rationale**: To improve runtime performance, enable better build-time error detection, and create more predictable codebases.

### Introduction of `Option` and `Result`
The language removed the concept of `null` pointers and replaced it with `Option<T>` for potentially missing values and `Result<T>` (shorthand `!`) for operations that might fail.
- **Rationale**: To provide Rust-like safety and eliminate null-pointer exceptions at runtime.

### Function Definition Syntax
Transitioned to the current `<args> -> ret: name [ body ]` syntax for all function definitions, including the entry `main` function.
- **Rationale**: To create a unified syntax for both named and anonymous functions, making the language more consistent and easier to parse.
