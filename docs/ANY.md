# The `any` Type

Opo provides an `any` type for situations where a value's type is not known at compile-time. However, Opo enforces safety by treating `any` as a restricted type that must be verified before use.

## Behavior

- **Implicit Upcasting**: Any type can be assigned to a variable of type `any`.
- **Explicit Unwrapping**: To use the value inside an `any`, you must use the `match` statement.
- **No Direct Operations**: You cannot perform arithmetic, logical, or comparison operations directly on `any` values (except for equality `==` and `!=`).

## Pattern Matching on `any`

When matching on an `any` value, you use type names as variants.

```opo
<x: any> -> void: process [
    match x [
        int(i) [
            "Integer: " + str(i + 1) !!
        ]
        str(s) [
            "String: " + s !!
        ]
        bol(b) [
            "Boolean: " + str(b) !!
        ]
        list(l) [
            "List of length: " + str(len(l)) !!
        ]
        map(m) [
            "Map of size: " + str(len(m)) !!
        ]
        void [
            "Nothing" !!
        ]
    ]
]
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

### Example

```opo
<> -> void: main [
    10 => val: any
    
    # val + 1 !! # Compilation Error: Cannot use 'any' in arithmetic.
    
    match val [
        int(i) [ (i + 1) !! ] # Prints 11
    ]
]
```
