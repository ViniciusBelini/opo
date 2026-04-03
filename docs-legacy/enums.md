# Algebraic Data Types: Enums and Options

Opo's type system is designed for safety and expressiveness, featuring generalized Enums (tagged unions) and a built-in `Option` type to eliminate `null` values. Opo enforces **Rust-like safety**, ensuring you cannot unwrap a value that might not be there.

## Enums

Enums allow you to define a type that can be one of several named variants. Each variant can optionally carry a payload of a specific type.

### Defining an Enum
`enum [ Variant1, Variant2(payload_type), ... ] => Name: type`

Example:
```
enum [
    Success,
    Warning(str),
    Error(int)
] => Status: type
```

### Using Enum Variants
Access variants using the dot operator on the Enum name.

```
Status.Success => s1: Status
Status.Warning("Low memory") => s2: Status
Status.Error(404) => s3: Status
```

## Options

The `Option` type is a specialized Enum used to represent a value that might be missing. It is central to Opo's "no null" philosophy.

### Syntax
- `T?`: Shorthand for `Option<T>`.
- `some(value)`: Creates an Option containing a value.
- `none`: Creates an empty Option.

Example:
```
<name: str> -> int? : getAge [
    name == "Alice" ? [ some(30) ] : [ none ]
]
```

## Pattern Matching with `match`

The `match` statement is the most powerful and safest way to work with Enums and Options. It forces you to consider all cases at compile-time (exhaustiveness check) and allows binding payloads to local variables.

### Syntax
```
match value [
    Variant1 [ # code for Variant1 ]
    Variant2(binding) [ # code for Variant2, 'binding' is available here ]
]
```

### Example with Enums:
```
<s: Status> -> void: checkStatus [
    match s [
        Success [ "Operation succeeded!" !! ]
        Warning(msg) [ "Warning: " + msg !! ]
        Error(code) [ "Error code: " + str(code) !! ]
    ]
]
```

## Flow-Sensitive Safety

Opo tracks the state of your variables. You can only access the payload of an Enum or Option if you have already proven that it exists.

### Existence Check (`?`)
For a quick check if an Option has a value, you can use it directly in a conditional expression. Inside the "true" block, it is safe to unwrap.

```
getAge("Bob") => age: int?

age ? [
    # Safe to unwrap here because of the check
    "Bob's age is " + str(age.some) !!
] : [
    "No age found." !!
]
```

### Unsafe Unwrapping (Compilation Error)
Opo prevents you from making mistakes. The following code will **fail to compile**:

```
getAge("Alice") => age: int?
age.some !! # Error: Unsafe unwrap of Enum variant.
```

To unwrap safely, always use `match` or an existence check `?`.
