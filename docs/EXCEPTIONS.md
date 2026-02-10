# Exception Handling in Opo

Opo provides a `try-catch` mechanism and a `throw` statement for handling errors and exceptional conditions.

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

The `try` block is executed. If a `throw` statement is encountered (directly or in a function called from the block), execution immediately jumps to the `catch` block. The error value thrown is assigned to the specified `err_variable`.

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
