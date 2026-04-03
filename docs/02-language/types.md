# Types

Opo is a statically-typed language, meaning every variable and expression has a type known at compile-time. Its type system is designed for safety, performance, and explicit behavior.

## Basic Types

Opo provides a set of core primitive types:

- **`int`**: 64-bit signed integer.
- **`flt`**: 64-bit floating-point number.
- **`bol`**: Boolean value, either `tru` or `fls`.
- **`str`**: Immutable UTF-8 string.
- **`void`**: Represents the absence of a value, typically used for functions that don't return anything.

## Compound Types

### Arrays (`[]type`)
Arrays are dynamic, strictly typed collections of elements.
Example: `[1, 2, 3] => numbers: []int`
Access elements using the dot operator: `numbers.0 !!`

### Maps (`map<key_type, value_type>`)
Maps are key-value pairs where both keys and values are strictly typed.
Example: `{"a": 1, "b": 2} => my_map: map<str, int>`
Access values using the dot operator: `my_map."a" !!`

### Structs
Structs allow grouping related data into a single named type.
```opo
struct [
    x: int,
    y: int
] => Point: type

Point(10, 20) => p: Point
p.x !!
```

### Enums
Enums represent a type that can be one of several named variants, optionally carrying payloads.
```opo
enum [
    Success,
    Error(str)
] => Result: type

Result.Success => r: Result
```

## Special Types

### The `any` Type
The `any` type can hold a value of any other type. However, Opo enforces strict safety by requiring you to refine the type using `match` or an explicit cast (`as`) before performing operations.

```opo
<x: any> -> void: process [
    match x [
        int(i) [ "Integer: " + str(i) !! ]
        str(s) [ "String: " + s !! ]
    ]
]
```

### Options (`type?`)
Opo uses the `Option` type (shorthand `?`) to represent values that might be missing, effectively eliminating `null` pointers.
- `some(value)`
- `none`

```opo
some(20) => age: int?
age ? [ age.some !! ] : [ "No age" !! ]
```

## Type Stability and Conversions
Opo does **not** perform implicit type conversions. For example, adding an `int` and a `flt` requires an explicit conversion:
`str(my_int) + " is my number" !!`
`flt(my_int) + 0.5 => my_float: flt`
