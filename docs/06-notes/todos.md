# TODOs and Development Tasks

This document tracks the current priority tasks for the Opo language development team. This list is updated as features are implemented and bugs are resolved.

## Compiler Improvements

- [ ] Finalize first-class anonymous function support.
- [ ] Implement constant definitions (`const`) for global and local variables.
- [ ] Improve error reporting with line and column highlighting in all phases.
- [ ] Resolve compiler-level type mismatches in complex expressions.
- [ ] Add support for type aliases.

## VM and Runtime

- [ ] Optimize the reference counting mechanism for performance.
- [ ] Implement better stack management for nested function calls.
- [ ] Improve concurrency primitives with better error handling.
- [ ] Fix potential memory leaks in circular reference patterns.

## Standard Library

- [ ] Expand `std/http` to support POST request body parsing.
- [ ] Add more string manipulation functions to `std/string`.
- [ ] Implement `std/json` for native JSON encoding and decoding.
- [ ] Provide a basic logging framework in `std/log`.

## Tooling and Documentation

- [x] Create the initial documentation structure (`docs/`).
- [ ] Develop a basic CLI to manage Opo projects.
- [ ] Add more comprehensive tests for edge cases in the compiler and VM.
- [ ] Maintain a list of breaking changes.
