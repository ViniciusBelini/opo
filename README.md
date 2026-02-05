# Opo Programming Language

Opo is a simple, fast, efficient, and safe programming language designed to be the opposite of Python. It is statically typed, explicit, and predictable.

## Design Principles

- **Not Python-like**: No significant whitespace, static typing, symbolic instead of keyword-heavy.
- **Not C-like**: Uses postfix notation and different delimiters.
- **Predictable**: No hidden magic, no implicit conversions.
- **Simple**: Small grammar, easy to learn.

## Syntax

### Types
- `int`: 64-bit integer
- `flt`: 64-bit float
- `bol`: Boolean (`tru`, `fls`)
- `str`: String
- `void`: No value

### Variables
`value => name: type`

Example:
`10 => x: int`

### Operations (Postfix)
- Arithmetic: `+`, `-`, `*`, `/`, `%`
- Comparison: `==`, `!=`, `<`, `>`, `<=`, `>=`
- Logic: `&&`, `||`, `!`
- Print: `!!`

Example:
`5 10 + !!` (Prints 15)

### Control Flow
- If-Else: `condition ? [ true_block ] : [ false_block ]`
- While: `condition @ [ loop_block ]`

### Functions
`<arg: type, ...> -> return_type: name [ body ]`

Example:
```
<n: int> -> int: factorial [
    n 0 == ? [
        1
    ] : [
        n n 1 - factorial *
    ]
]
```

### Main Entry Point
The program starts execution from the `main` function.

```
<> -> void: main [
    5 factorial !!
]
```

## Implementation

Opo is written in C and runs on a custom Bytecode Virtual Machine.
