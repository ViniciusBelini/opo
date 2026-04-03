# Structs in Opo

Structs allow you to create custom data types with named fields.

## Syntax

### Defining a Struct
`struct [ field1: type, field2: type, ... ] => Name: type`

Example:
```
struct [
    x: int,
    y: int
] => Point: type
```

### Instantiating a Struct
`Name(val1, val2, ...)`

Example:
`Point(10, 20) => p: Point`

### Accessing Members
`p.x !!` (Prints 10)

### Updating Members
`30 => p.x`
