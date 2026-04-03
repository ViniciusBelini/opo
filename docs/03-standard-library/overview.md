# Standard Library Overview

The Opo Standard Library (stdlib) provides a set of built-in modules designed to facilitate common programming tasks. Every module is strictly typed and accessed via an alias.

## Core Modules

### `std/math`
Provides mathematical constants and functions for both integers and floating-point numbers.
- **Constants**: `PI`, `E`
- **Functions**: `abs`, `pow`, `sqrt_f`, `sin_f`, `cos_f`, `rand`, `seed`, etc.

### `std/string`
Utilities for advanced string manipulation.
- **Functions**: `slice`, `trim`, `contains`, `join`, `to_upper`, `to_lower`, etc.

### `std/array`
Common operations for dynamic arrays.
- **Functions**: `contains_int`, `contains_str`, `sum`, `max`, `reverse_int`, etc.

### `std/datetime`
Functions for working with time and dates.
- **Functions**: `now()` (unix timestamp), `format(t: int)`.

### `std/test`
A lightweight unit testing framework.
- **Functions**: `assert`, `assert_eq_int`, `assert_eq_str`.

## Using the Standard Library

To use a standard library module, import it using the `std/` prefix.

```opo
"std/math" => math: imp

<> -> void: main [
    math.sqrt_f(16.0) !!
]
```

## Performance and Implementation

The standard library is implemented as a combination of Opo code and native C functions. Performance-critical operations (like math functions and string parsing) are handled by the Opo VM's native layer, ensuring high execution speed.
