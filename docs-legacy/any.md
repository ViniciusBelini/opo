# The `any` Type

Opo provides an `any` type for situations where a value's type is not known at compile-time. Opo enforces strict safety by treating `any` as a restricted type that must be verified before use.

## Behavior

- **Implicit Upcasting**: Any type can be assigned to a variable of type `any`.
- **Strict Restriction**: You cannot perform ANY operation on an `any` value except for:
    - Assignment (`=>`)
    - Passing as a function argument
    - Equality check (`==` and `!=`)
    - Pattern matching (`match`)
    - Existence check (`?`)
    - Type casting (`as`)

## Forbidden Operations

The following will result in a **Compilation Error**:
- Arithmetic (`+`, `-`, `*`, `/`, `%`)
- Comparison (`<`, `>`, `<=`, `>=`)
- Member access (`obj.field`)
- Indexing (`arr.0`, `map."key"`, `arr.(i)`)
- Function calls (`val()`)

## Pattern Matching on `any`

To use the value inside an `any`, you must refine its type. Opo provides two ways to do this: `match` and `as`.

### Refining with `match` (Safe)

The `match` statement is the safest way to work with `any`. Inside the match block, the binding (the variable in parentheses) will have the refined type, while the original variable remains `any`.

```opo
<x: any> -> void: process [
    match x [
        int(i) [
            # 'i' is of type 'int' here
            "Integer: " + str(i + 1) !!
        ]
        str(s) [
            # 's' is of type 'str' here
            "String: " + s !!
        ]
        void [
            "Nothing" !!
        ]
    ]
]
```

### Refining with `as` (Explicit Cast)

The `as` operator allows for an explicit type cast. This is useful when you are certain of the underlying type. If the cast fails at runtime, it results in a runtime error.

```opo
m."key" as str => name: str
```

## Supported Types for Match

The following type names are supported as variants when matching on `any`:
- `int`
- `flt`
- `bol`
- `str`
- `void`
- `list` (for arrays)
- `map`
- `fun`
- `chan`
- Any user-defined `struct` or `enum` name.
