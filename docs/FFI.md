# Foreign Function Interface (FFI)

Opo allows calling C functions from shared libraries using the `ffiLoad` and `ffiCall` built-in functions.

## `ffiLoad(path: str) -> int`

Loads a shared library (`.so` or `.dll`).
- `path`: The path to the shared library file. If an empty string `""` is provided, it loads the current process's symbols.
- Returns an `int!` handle to the library.

## `ffiCall(handle: int, name: str, arg_types: str, ret_type: str, args...) -> any`

Calls a function from a loaded library.
- `handle`: The library handle returned by `ffiLoad`.
- `name`: The name of the function to call.
- `arg_types`: A string where each character represents the type of an argument:
    - `'i'`: 64-bit integer
    - `'f'`: 64-bit float
    - `'s'`: String (passed as `const char*`)
    - `'p'`: Pointer (passed as `uint64_t`)
- `ret_type`: A string (only first character used) representing the return type:
    - `'v'`: void
    - `'i'`: 64-bit integer
    - `'f'`: 64-bit float
    - `'s'`: String (C string converted to Opo string)
    - `'p'`: Pointer (returned as `uint64_t` integer)
- `args...`: The actual arguments to pass to the function.

### Example

```opo
<> -> void: main [
    # Load libc

    match ffiLoad("")[
        ok(libc)[
            # Call 'puts' from libc
            ffiCall(libc, "puts", "s", "i", "Hello from Opo FFI!")

            # Call 'abs' from libc
            match ffiCall(libc, "abs", "i", "i", -42)[
                ok(x)[
                    println(x) # Prints 42
                ]
                err(e)[
                    println("Fail: "+e)
                ]
            ]
        ]
        err(e)[
            println("Fail: "+e)
        ]
    ]
]
```
