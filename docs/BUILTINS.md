# Built-in Functions in Opo

Opo provides several built-in functions to facilitate common tasks and enable library creation.

## Core Functions

### `len(obj)`
Returns the length of a string or an array.
- Input: `str` or `[]type`
- Output: `int`

### `str(val)`
Converts a value to its string representation.
- Input: `int`, `flt`, `bol`, or `str`
- Output: `str`

### `int(val)`
Converts a string or float to an integer.
- Input: `str`, `flt`, or `int`
- Output: `int`

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
- Output: `str` (content)

### `writeFile(path, content)`
Writes a string to a file.
- Input: `str` (path), `str` (content)
- Output: `bol` (success)

## System

### `args()`
Returns an array of command-line arguments.
- Output: `[]str`

## Array Operations

### `append(arr, val)`
Appends a value to an array.
- Input: `[]type`, `type`
- Output: `[]type`
