# Variables

Variables in Opo are the primary way to store and manipulate data. Opo uses a unique symbolic operator (`=>`) for declaration and assignment, and it strictly enforces static typing.

## Declaration and Assignment

In Opo, a variable is declared by providing a value, followed by the `=>` operator, the variable name, and its type.

```opo
10 => x: int
"Opo" => name: str
```

### Initial Values
Variables **must** be initialized when they are declared. Opo does not allow uninitialized variables.

### Reassignment
To change the value of an existing variable, use the `=>` operator without the type declaration.

```opo
10 => x: int
20 => x
```

## Static Typing

Every variable has a fixed type that is determined at compile-time. Once a variable is declared with a specific type, it cannot hold values of any other type.

```opo
10 => x: int
"Hello" => x  # Compilation Error: Cannot assign str to int
```

## Scoping

Opo uses block-level scoping marked by square brackets `[]`. Variables declared within a block are only accessible within that block and its nested blocks.

```opo
10 => x: int

x > 5 ? [
    20 => y: int
    x + y !!  # Valid
] : [
    # y is not accessible here
]

# y is not accessible here
```

### shadowing
Opo allows variable shadowing, where a variable declared in an inner scope can have the same name as a variable in an outer scope.

```opo
10 => x: int

x > 5 ? [
    20 => x: int
    x !!  # Prints 20
] : [ ]

x !!  # Prints 10
```
