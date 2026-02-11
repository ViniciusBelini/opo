# Algebraic Data Types: Enums and Options

Opo's type system is designed for safety and expressiveness, featuring generalized Enums (tagged unions) and a built-in `Option` type to eliminate `null` values.

## Enums

Enums allow you to define a type that can be one of several named variants. Each variant can optionally carry a payload of a specific type.

### Defining an Enum
`enum [ Variant1, Variant2(payload_type), ... ] => Name: type`

Example:
enum [ Success, Warning(str), Error(int) ] => Status: type


### Using Enum Variants
Access variants using the dot operator on the Enum name.

Status.Success => s1: Status Status.Warning("Low memory") => s2: Status Status.Error(404) => s3: Status


## Options

The `Option` type is a specialized Enum used to represent a value that might be missing. It is central to Opo's "no null" philosophy.

### Syntax
- `T?`: Shorthand for `Option<T>`.
- `some(value)`: Creates an Option containing a value.
- `none`: Creates an empty Option.

Example:
<name: str> -> int? : getAge [ name == "Alice" ? [ some(30) ] : [ none ] ]


## Pattern Matching with `match`

The `match` statement is the most powerful and safest way to work with Enums and Options. It forces you to consider all cases and allows binding payloads to local variables.

### Syntax
match value [ Variant1 [ # code for Variant1 ] Variant2(binding) [ # code for Variant2, 'binding' is available here ] ]


### Example with Enums:
<s: Status> -> void: checkStatus [ match s [ Success [ "Operation succeeded!" !! ] Warning(msg) [ "Warning: " + msg !! ] Error(code) [ "Error code: " + str(code) !! ] ] ]


### Example with Options:
getAge("Bob") => age: int? match age [ some(val) [ "Bob's age is " + str(val) !! ] none [ "Bob's age is unknown." !! ] ]


## Existence Check (`?`)

For a quick check if an Option has a value, you can use it directly in a conditional expression.

age ? [ "Has age" !! ] : [ "No age" !! ]


## Manual Unwrapping

You can access the payload of an `Option` using the `.some` property. **Caution**: This will return `void` (effectively causing an error in many contexts) if the option is `none`. Use `match` for safety.

`age.some !!`
