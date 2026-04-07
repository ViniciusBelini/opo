# Functions

Functions are the fundamental building blocks of Opo. They are strictly typed, require explicit parameter types, and use a unique symbolic syntax.

## Defining a Function

The general syntax for defining a named function is:

`<parameters> -> return_type: function_name [ body ]`

The return type can also be omitted and inferred from the function body:

`<parameters>: function_name [ body ]`

### Named Functions
Named functions are typically defined at the module level or nested within other functions.

```opo
<a: int, b: int> -> int: add [
    a + b
]
```

With inferred return type:

```opo
<a: int, b: int>: add [
    a + b
]
```

### Parameters
Parameters are enclosed in `<>` and separated by commas. Each parameter must have a name followed by a colon and its type. If a function takes no parameters, use `<>`.

### Return Types
Functions may declare their return type explicitly with `->`, or let the compiler infer it from the function body and `^` returns.

```opo
<name: str>: greet [
    "Hello, " + name
]
```

If a function does not return a value, you can still write `-> void` explicitly for clarity.

Type inference remains static: the compiler must be able to determine a single consistent return type. If different branches produce incompatible types, compilation fails.

## Calling a Function

Opo uses C-style syntax for calling functions.

```opo
add(10, 20) => sum
sum !!
```

## Anonymous Functions (Function Literals)

Opo supports anonymous functions, which can be assigned to variables or passed as arguments to other functions.

```opo
<n: int> [ n * 2 ] => double
double(21) !!
```

## Capturing Variables

Nested and anonymous functions can capture variables from the parent scope by listing them before the function signature.

```opo
[x, y] <> -> int: sum_copy [
    x + y
]
```

Only variable names are allowed inside the capture list. Expressions are not allowed.

Captured values are copied when the function is created. Reassigning the original variable later does not change the captured value, and reassigning the captured variable inside the function does not modify the original variable.

```opo
<> -> void: main [
    10 => x
    [x] <> -> int: get_x [
        x
    ]

    20 => x
    get_x() !!  # Prints 10
    x !!        # Prints 20
]
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
    <a: int, b: int>: multiply [ a * b ]
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
