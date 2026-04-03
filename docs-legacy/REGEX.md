# Regular Expressions (RegEx)

Opo provides native support for POSIX Extended Regular Expressions through the `std/regex` module.

## `re.is_match(pattern: str, text: str) -> bol`

Returns `tru` if the `text` matches the `pattern`, and `fls` otherwise.

### Example

```opo
"std/regex" => re: imp

<> -> void: main [
    re.is_match("^[a-z]+$", "hello") !!  # tru
    re.is_match("^[a-z]+$", "hello12") !! # fls
    
    re.is_match("[0-9]+", "abc 123 def") ? [
        "Contains numbers" !!
    ] : [
        "No numbers found" !!
    ]
]
```

Note: The implementation uses the system's `<regex.h>` and supports Extended Regular Expression (ERE) syntax.
