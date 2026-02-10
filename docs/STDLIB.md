# Opo Standard Library (stdlib)

Opo comes with a set of built-in modules to facilitate common tasks.

## `std/math`
Mathematical constants and functions.
- `PI`: 3.1415926535
- `E`: 2.7182818284
- `max(a: int, b: int) -> int`
- `min(a: int, b: int) -> int`
- `abs(n: int) -> int`
- `pow(base: int, exp: int) -> int`
- `factorial(n: int) -> int`
- `sqrt_f(x: flt) -> flt`
- `sin_f(x: flt) -> flt`
- `cos_f(x: flt) -> flt`
- `tan_f(x: flt) -> flt`
- `log_f(x: flt) -> flt`

## `std/string`
String manipulation utilities.
- `slice(s: str, start: int, end: int) -> str`
- `is_whitespace(c: str) -> bol`
- `trim_left(s: str) -> str`
- `trim_right(s: str) -> str`
- `trim(s: str) -> str`
- `contains(s: str, sub: str) -> bol`
- `join(parts: []str, sep: str) -> str`
- `to_upper(s: str) -> str`
- `to_lower(s: str) -> str`

## `std/array`
Common array operations.
- `contains_int(arr: []int, val: int) -> bol`
- `contains_str(arr: []str, val: str) -> bol`
- `sum(arr: []int) -> int`
- `max(arr: []int) -> int`
- `reverse_int(arr: []int) -> []int`

## `std/datetime`
Basic date and time functions.
- `now() -> int`: Returns current unix timestamp.
- `format(t: int) -> str`: Returns a string representation of the timestamp.

## `std/test`
A simple unit testing framework.
- `assert(condition: bol, message: str) -> void`
- `assert_eq_int(a: int, b: int, message: str) -> void`
- `assert_eq_str(a: str, b: str, message: str) -> void`
