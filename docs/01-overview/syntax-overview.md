# Syntax Overview

This guide provides a quick reference for Opo's unique and symbolic syntax.

## Variables and Types

Opo is statically typed and uses the `=>` operator for declarations and assignments.

```opo
10 => x: int
"Opo" => name: str
tru => active: bol
```

### Basic Types
- `int`: 64-bit integer
- `flt`: 64-bit float
- `bol`: Boolean (`tru`, `fls`)
- `str`: String
- `void`: No value

## Operators

### Arithmetic and Comparison
Opo uses standard infix notation for arithmetic and comparisons.
- Arithmetic: `+`, `-`, `*`, `/`, `%`
- Comparison: `==`, `!=`, `<`, `>`, `<=`, `>=`
- Logic: `&&`, `||`, `!`

### Special Operators
- `!!`: Print (postfix)
- `=>`: Declaration and assignment
- `? :`: If-Else
- `@`: While loop
- `.`: Break
- `..`: Continue
- `^`: Return

## Control Flow

### Conditionals (If-Else)
```opo
x > 0 ? [
    "Positive" !!
] : [
    "Non-positive" !!
]
```

### While Loop
```opo
0 => i: int
i < 5 @ [
    i !!
    i + 1 => i
]
```

### Pattern Matching
```opo
match opt_val [
    some(val) [ val !! ]
    none [ "Empty" !! ]
]
```

## Functions

Functions are defined with the `<args> -> return_type: name [ body ]` syntax.

```opo
<n: int> -> int: factorial [
    n == 0 ? [
        1
    ] : [
        n * factorial(n - 1)
    ]
]
```

## Entry Point

All Opo code must reside within functions. The `main` function is the program's starting point.

```opo
<> -> void: main [
    "Hello Opo!" !!
]
```
