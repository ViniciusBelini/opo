# Exception Handling in Opo

Opo provides a `try-catch` mechanism and a `throw` statement for handling errors and exceptional conditions. It also features automatic capturing of runtime errors.

## Syntax

### Try-Catch
```
try [
    # Risky code
    throw "something happened"
] catch err_variable [
    # Handle error
    "Caught: " + err_variable !!
]
```

The `try` block is executed. If a `throw` statement is encountered, or if a **runtime error** occurs, execution immediately jumps to the `catch` block. The error value is assigned to the specified `err_variable`.

### Runtime Error Capturing
Opo's VM automatically converts low-level errors into catchable exceptions. This includes:
- Division by zero
- Array index out of bounds
- Map key not found
- Invalid member access

Example:
```
try [
    10 / 0 => x
] catch e [
    "Error during math: " + e !! # Prints: Error during math: Division by zero
]
```

### Throw
`throw expression`

The `expression` is evaluated and its result is thrown as an error. If there is no surrounding `try-catch` block, the program terminates with an "Unhandled Exception" error.

## The `err` Type
Opo has a dedicated `err` type. You can create an error value using the `error(message)` built-in function.

Example:
`throw error("Database connection failed")`

Catching and checking:
```
try [
    ...
] catch e [
    typeOf(e) == "err" ? [
        "It was a formal error: " + str(e) !!
    ] : [
        "It was something else: " + str(e) !!
    ]
]
```
