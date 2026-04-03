# Built-in Functions in Opo

Opo provides several built-in functions to facilitate common tasks and enable library creation.

## Core Functions

### `len(obj)`
Returns the length of a string or an array.
- Input: `str` or `[]type`
- Output: `int`

### `str(val)`
Converts a value to its string representation.
- Input: any
- Output: `str`
- Note: Supports most types including collections and enums.

### `int(val)`
Converts a string or float to an integer.
- Input: `str`, `flt`, or `int`
- Output: `int!` (Result)
- Note: Returns `err` if the string format is invalid or the type is not supported.

### `flt(val)`
Converts a string or integer to a float.
- Input: `str`, `int`, or `flt`
- Output: `flt!` (Result)
- Note: Returns `err` if the string format is invalid or the type is not supported.

### `typeOf(val)`
Returns the type of a value as a string.
- Input: any
- Output: `str`

## Console I/O

### `print(val)`
Prints a value to stdout without a newline.
- Input: any
- Output: `void`

### `println(val)`
Prints a value to stdout with a newline.
- Input: any
- Output: `void`

### `readLine()`
Reads a line of text from stdin.
- Output: `str`

## File I/O

### `readFile(path)`
Reads the entire content of a file into a string.
- Input: `str` (path)
- Output: `str!` (Result containing content or error message)

### `writeFile(path, content)`
Writes a string to a file.
- Input: `str` (path), `str` (content)
- Output: `bol!` (Result containing success bol or error message)

## System

### `args()`
Returns an array of command-line arguments.
- Output: `[]str`

## Array Operations

### `append(arr, val)`
Appends a value to an array.
- Input: `[]type`, `type`
- Output: `[]type`

## Advanced System Functions

### `exit(code)`
Terminates the program with the given exit code.
- Input: `int`
- Output: `void`

### `clock()`
Returns the CPU time used by the program since it started, in seconds.
- Output: `flt`

### `system(cmd)`
Executes a system command.
- Input: `str`
- Output: `int!` (Result containing exit code or error message)

### `keys(map)`
Returns an array containing all keys in a map.
- Input: `{K:V}`
- Output: `[]K`

### `delete(map, key)`
Removes a key and its associated value from a map.
- Input: `{K:V}`, `K`
- Output: `void`

### `has(map, key)`
Checks if a key exists in a map.
- Input: `{K:V}`, `K`
- Output: `bol`

### `ascii(char)`
Returns the ASCII integer value of the first character of a string.
- Input: `str`
- Output: `int`

### `char(code)`
Returns a string consisting of a single character with the given ASCII code.
- Input: `int`
- Output: `str`

### `error(message)`
Creates an error value with the given message.
- Input: any
- Output: `err`

### `time()`
Returns the current Unix timestamp.
- Output: `int`

### `sqrt(val)`, `sin(val)`, `cos(val)`, `tan(val)`, `log(val)`
Mathematical functions.
- Input: `int` or `flt`
- Output: `flt`

### `rand(x, y)`
Returns a random float between `x` and `y` (inclusive).
- Input: `flt` (x), `flt` (y)
- Output: `flt`

### `seed(n)`
Seeds the random number generator.
- Input: `int` (n)
- Output: `void`
