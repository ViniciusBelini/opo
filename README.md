# Opo Programming Language

Opo is a simple, fast, efficient, and safe programming language designed to be the opposite of Python. It is statically typed, explicit, and predictable.

## Design Principles

- **Not Python-like**: No significant whitespace, static typing, symbolic instead of keyword-heavy.
- **C-like Infix Syntax**: Uses infix notation for arithmetic and comparisons, with C-style function calls.
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

### Operations (Infix)
- Arithmetic: `+`, `-`, `*`, `/`, `%`
- Comparison: `==`, `!=`, `<`, `>`, `<=`, `>=`
- Logic: `&&`, `||`, `!`
- Print: `!!` (Postfix)

Example:
`(5 + 10) !!` (Prints 15)

### Control Flow
Opo uses symbolic operators for control flow.

#### If-Else
`condition ? [ true_block ] : [ false_block ]`

Example:
```
score >= 60 ? [
    "Passed" !!
] : [
    "Failed" !!
]
```

You can chain conditions for an "else if" effect:
```
x > 0 ? [
    "Positive" !!
] : x < 0 ? [
    "Negative" !!
] : [
    "Zero" !!
]
```

#### While Loop
`condition @ [ loop_block ]`

Example:
```
0 => i: int
i < 5 @ [
    i !!
    i + 1 => i
]
```

### Functions
`<arg: type, ...> -> return_type: name [ body ]`

Example:
```
<n: int> -> int: factorial [
    n == 0 ? [
        1
    ] : [
        n * factorial(n - 1)
    ]
]
```

### Entry Point
The program starts execution from the `main` function. All code must be inside functions.

```
<> -> void: main [
    println("Hello world!")
]
```

## Standard Library

### Math
The math module is available as a built-in or via `import "std/math"`.

- `rand(min: flt, max: flt) -> flt`: Returns a random float between `min` and `max`. If no seed has been set, it automatically seeds using the current timestamp on the first call.
- `seed(n: int) -> void`: Manually seeds the random number generator for deterministic results.

## Implementation

Opo is written in C and runs on a custom Bytecode Virtual Machine.
