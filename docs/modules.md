# Imports and Modules in Opo

Opo supports a modular system that allows you to split your code across multiple files and use them via imports.

## Exporting Items

By default, all functions and structs defined in a module are private to that module. To make them accessible from other modules, use the `pub` keyword.

### Exporting a Function
`pub <params> -> return_type: name [ body ]`

Example:
```
pub <a: int, b: int> -> int: add [
    a + b
]
```

### Exporting a Struct
`pub struct [ fields ] => Name: type`

Example:
```
pub struct [
    x: int,
    y: int
] => Point: type
```

## Importing Modules

Use the import syntax at the top level of your file.

### Syntax
`"path/to/file.opo" => alias: imp`

- **Path**: A string literal containing the relative path to the `.opo` file.
- **Alias**: The namespace that will be used to access the module's public items.
- **imp**: The type keyword for imports.

Example:
`"./math_utils.opo" => math: imp`

## Using Imported Items

Imported items are accessed using the dot operator with the alias as a prefix.

### Accessing a Function
`math.add(10, 20) !!`

### Accessing a Struct
`math.Point(1, 2) => p: math.Point`

## Standard Library Imports

Opo includes a standard library that can be imported using the `std/` prefix. These modules are automatically resolved to the language's installation directory.

Example:
```
"std/math" => math: imp
"std/string" => str_utils: imp

<> -> void: main [
    math.abs(-10) !!
]
```

When an import path starts with `std/`, the compiler searches for the module in the `lib/std/` directory relative to the Opo executable. You can omit the `.opo` extension for standard library imports.

## Rules and Behavior

1. **Namespacing**: All imported items must be prefixed with the module's alias.
2. **Visibility**: Only items marked with `pub` are accessible via the alias.
3. **Internal Access**: Functions within a module can call each other without any prefix, including private ones.
4. **Path Resolution**: Paths are resolved relative to the directory of the main entry file.
5. **Circular Imports**: Opo detects circular imports and will issue a compile-time error.
6. **Redundant Imports**: Modules are compiled only once, even if imported by multiple files.
