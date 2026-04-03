# Functions

Functions are the fundamental building blocks of Opo. They are strictly typed, they explicitly declare their parameters and return types, and they use a unique symbolic syntax.

## Defining a Function

The general syntax for defining a named function is:

`<parameters> -> return_type: function_name [ body ]`

### Named Functions
Named functions are typically defined at the module level or nested within other functions.

```opo
<a: int, b: int> -> int: add [
    a + b
]
```

### Parameters
Parameters are enclosed in `<>` and separated by commas. Each parameter must have a name followed by a colon and its type. If a function takes no parameters, use `<>`.

### Return Types
Every function must explicitly declare its return type using the `->` operator. If a function does not return a value, use `void`.

## Calling a Function

Opo uses C-style syntax for calling functions.

```opo
add(10, 20) => sum: int
sum !!
```

## Anonymous Functions (Function Literals)

Opo supports anonymous functions, which can be assigned to variables or passed as arguments to other functions.

```opo
<n: int> -> int [ n * 2 ] => double: fun
double(21) !!
```

### Immediate Function Calls
An anonymous function can be called immediately after definition by wrapping it in parentheses.

```opo
(<n: int> -> int [ n + 1 ])(10) !!
```

## Nested Functions

Functions can be defined within other functions, allowing for encapsulation and clean code organization.

```opo
<> -> void: main [
    <a: int, b: int> -> int: multiply [ a * b ]
    multiply(5, 5) !!
]
```

## The `main` Function

Every Opo program must have a `main` function, which serves as the entry point for execution. By convention, `main` takes no arguments and returns `void`.

```opo
<> -> void: main [
    "Hello World!" !!
]
```

## Early Return (`^`)

Use the `^` operator to return a value and exit a function. For functions returning `void`, `^` can be used alone to exit early.

```opo
<n: int> -> int: check_odd [
    n % 2 == 0 ? [ ^ 0 ] : [ ]
    ^ 1
]
```
