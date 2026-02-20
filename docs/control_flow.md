# Control Flow in Opo

Opo provides expressive control flow mechanisms using symbolic operators.

## Loops

Opo uses the `@` operator for while loops.

```opo
0 => i: int
i < 10 @ [
    i !!
    i + 1 => i
]
```

### Break (`.`)

The `.` operator is used to break out of the innermost loop.

```opo
0 => i: int
i < 10 @ [
    i == 5 ? [ . ] : [ ]
    i !!
    i + 1 => i
]
```

### Continue (`..`)

The `..` operator is used to skip the rest of the current loop iteration and start the next one.

```opo
0 => i: int
i < 10 @ [
    i + 1 => i
    i % 2 == 0 ? [ .. ] : [ ]
    "Odd: " + str(i) !!
]
```

## Functions

### Early Return (`^`)

The `^` operator is used for early returns from a function.

```opo
<x: int> -> int: double_if_large [
    x > 10 ? [ ^ x * 2 ] : [ ]
    ^ x
]
```

In void functions, `^` can be used without a value to return early.

```opo
<x: int> -> void: check_positive [
    x < 0 ? [ ^ ] : [ ]
    "Positive" !!
]
```

## Conditionals

### If-Else (`? :`)

Opo uses `?` for "if" and `:` for "else".

```opo
x > 0 ? [
    "Positive" !!
] : [
    "Non-positive" !!
]
```

### Match (`match`)

Used for pattern matching on Enums and Options.

```opo
match opt_val [
    some(val) [ val !! ]
    none [ "Empty" !! ]
]
```
