# Variables

Variables in Opo are the primary way to store and manipulate data. Opo uses a unique symbolic operator (`=>`) for declaration and assignment, and it strictly enforces static typing.

## Declaration and Assignment

In Opo, a variable is declared by providing a value, followed by the `=>` operator and the variable name. The type annotation is optional on the first assignment.

```opo
10 => x
```

You can still write the type explicitly when you want to make the code more readable:

```opo
10 => x: int
"Opo" => name: str
```

### Initial Values
Variables **must** be initialized when they are declared. Opo does not allow uninitialized variables.

### Type Inference
When the type is omitted on the first assignment, Opo infers it from the value on the right-hand side.

```opo
10 => x
"Opo" => name
```

This does **not** make variables dynamic. The inferred type becomes the variable's fixed compile-time type.

### Reassignment
To change the value of an existing variable, use the `=>` operator without the type declaration.

```opo
10 => x
20 => x
```

## Static Typing

Every variable has a fixed type that is determined at compile-time. Once a variable is declared, whether by explicit annotation or inference, it cannot hold values of any other type.

```opo
10 => x
"Hello" => x  # Compilation Error: Cannot assign str to int
```

## Scoping

Opo uses block-level scoping marked by square brackets `[]`. Variables declared within a block are only accessible within that block and its nested blocks.

```opo
10 => x

x > 5 ? [
    20 => y
    x + y !!  # Valid
] : [
    # y is not accessible here
]

# y is not accessible here
```

### shadowing
Opo allows variable shadowing, where a variable declared in an inner scope can have the same name as a variable in an outer scope.

```opo
10 => x

x > 5 ? [
    20 => x
    x !!  # Prints 20
] : [ ]

x !!  # Prints 10
```
