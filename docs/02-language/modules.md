# Modules and Imports

Opo supports a modular system that allows you to organize your code across multiple files and reuse functionality via imports. This system is designed for clarity and explicit namespacing.

## Exporting Items (`pub`)

By default, all functions and structs defined in a module are private to that module. To make them accessible from other modules, use the `pub` keyword.

### Exporting a Function
```opo
pub <a: int, b: int> -> int: add [
    a + b
]
```

### Exporting a Struct
```opo
pub struct [
    x: int,
    y: int
] => Point: type
```

## Importing Modules

Use the import syntax at the top level of your file to bring in other modules.

### Syntax
`"path/to/file.opo" => alias: imp`

- **Path**: A string literal containing the relative path to the `.opo` file.
- **Alias**: The namespace used to access the module's public items.
- **imp**: The type keyword for imports.

Example:
`"./math_utils.opo" => math: imp`

## Using Imported Items

Imported items are accessed using the dot operator with the alias as a prefix. This ensures that there are no name collisions between different modules.

### Accessing a Function
`math.add(10, 20) !!`

### Accessing a Struct
`math.Point(1, 2) => p: math.Point`

## Standard Library Imports

Opo includes a built-in standard library that can be imported using the `std/` prefix.

Example:
```opo
"std/math" => math: imp

<> -> void: main [
    math.abs(-10) !!
]
```

When an import path starts with `std/`, the compiler searches for the module in the internal library directory.

## Rules and Behavior

1.  **Namespacing**: All imported items **must** be prefixed with the module's alias.
2.  **Visibility**: Only items marked with `pub` are accessible via the alias.
3.  **Internal Access**: Functions within a module can call each other without any prefix, including private ones.
4.  **Single Compilation**: Each module is compiled only once, even if imported by multiple files.
5.  **Circular Imports**: Opo detects circular imports and will issue a compile-time error.
