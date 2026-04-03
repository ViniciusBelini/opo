# Control Flow

Opo provides simple and expressive control flow mechanisms using symbolic operators. This "symbolic-heavy" design is central to Opo's philosophy.

## Conditionals (If-Else)

The `?` and `:` operators are used for if-else logic.

```opo
condition ? [
    # True block
] : [
    # False block
]
```

### If-Else-If (Chaining)
```opo
x > 0 ? [
    "Positive" !!
] : x < 0 ? [
    "Negative" !!
] : [
    "Zero" !!
]
```

## Loops (While)

The `@` operator is used for while loops.

```opo
0 => i: int
i < 5 @ [
    i !!
    i + 1 => i
]
```

### Break (`.`)
The `.` operator immediately terminates the innermost loop.

```opo
0 => i: int
i < 10 @ [
    i == 5 ? [ . ] : [ ]
    i !!
    i + 1 => i
]
```

### Continue (`..`)
The `..` operator skips the rest of the current iteration and starts the next one.

```opo
0 => i: int
i < 10 @ [
    i + 1 => i
    i % 2 == 0 ? [ .. ] : [ ]
    i !!
]
```

## Pattern Matching (`match`)

The `match` statement is the safest way to work with complex types like `any`, `enum`, and `Option`.

```opo
match value [
    some(val) [ val !! ]
    none [ "Nothing here" !! ]
]
```

Inside a `match` block, bindings (like `val`) are available to access the underlying value of a variant.

## Early Return (`^`)

The `^` operator is used to return early from a function. For functions with a return type, it must be followed by a value of that type.

```opo
<x: int> -> int: double_if_large [
    x > 10 ? [ ^ x * 2 ] : [ ]
    ^ x
]
```

For `void` functions, `^` can be used alone.

```opo
<x: int> -> void: check_positive [
    x < 0 ? [ ^ ] : [ ]
    "Everything is fine" !!
]
```
