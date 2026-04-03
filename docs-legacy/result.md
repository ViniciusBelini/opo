# Error Handling with `Result`

Opo uses the `Result<T>` type (shorthand `T!`) for robust and explicit error handling. This type is used by native functions that can fail, such as file operations, network requests, and type conversions.

## The `Result` Type

`Result<T>` is a built-in Enum with two variants:
- `ok(value)`: Indicates success and contains the result of the operation.
- `err(message)`: Indicates failure and contains a string describing the error.

## Shorthand Syntax

You can use the `!` suffix as a shorthand for `Result`:
- `str!` is equivalent to `Result<str>`
- `int!` is equivalent to `Result<int>`
- `[]str!` is equivalent to `Result<[]str>`

## Handling Results

### Pattern Matching (Recommended)

The safest way to handle a `Result` is using the `match` statement. The compiler ensures you handle both cases.

```opo
match readFile("config.txt") [
    ok(content) [
        "File content: " + content !!
    ]
    err(e) [
        "Failed to read file: " + e !!
    ]
]
```

### Existence Check

You can also use the `?` operator for a quick check. A `Result` evaluates to true if it is `ok` and false if it is `err`.

```opo
readFile("data.txt") => res
res ? [
    # Safe to unwrap ok value using dot notation if guarded
    "Success: " + res.ok !!
] : [
    "Failure: " + res.err !!
]
```

## Native Functions returning `Result`

Many built-in functions now return `Result` types instead of throwing runtime errors or returning null values:

| Function | Return Type |
| --- | --- |
| `readFile(path)` | `str!` |
| `writeFile(path, content)` | `bol!` |
| `int(value)` | `int!` |
| `flt(value)` | `flt!` |
| `json_parse(json)` | `any!` |
| `httpGet(url)` | `str!` |
| `listDir(path)` | `[]str!` |
| `system(command)` | `int!` |
| `ffiLoad(lib)` | `int!` |
| `ffiCall(...)` | `any!` |
