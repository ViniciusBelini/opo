# RegEx Library

Opo provides native support for POSIX Extended Regular Expressions (ERE) through the `std/regex` module. This library allows for powerful pattern matching and string validation.

## The `std/regex` Module

To use the regular expressions library, import it using the `std/` prefix.

```opo
"std/regex" => re: imp
```

### `re.is_match(pattern: str, text: str) -> bol`

The primary function in the library is `is_match`, which returns `tru` if the provided text matches the given pattern, and `fls` otherwise.

#### Example: Validating a String
```opo
<> -> void: main [
    re.is_match("^[a-z]+$", "hello") !!  # tru
    re.is_match("^[a-z]+$", "hello12") !! # fls
]
```

#### Example: Checking for Numbers
```opo
re.is_match("[0-9]+", "abc 123 def") ? [
    "Contains numbers" !!
] : [
    "No numbers found" !!
]
```

## Regular Expression Syntax

The implementation uses the system's POSIX `<regex.h>` and supports Extended Regular Expression (ERE) syntax. Common features include:

- **`.`**: Matches any single character.
- **`*`**: Matches zero or more of the preceding element.
- **`+`**: Matches one or more of the preceding element.
- **`?`**: Matches zero or one of the preceding element.
- **`^`**: Matches the start of a line.
- **`$`**: Matches the end of a line.
- **`[abc]`**: Matches any of the characters 'a', 'b', or 'c'.
- **`[a-z]`**: Matches any character in the range 'a' to 'z'.
- **`(abc)`**: Groups characters for capture or quantifier application.
- **`|`**: Matches either the expression before or after the bar (logical OR).

## Performance Notes

RegEx operations are performed by native code in the Opo VM. For complex patterns, it is recommended to keep patterns as simple as possible to ensure fast matching performance.
