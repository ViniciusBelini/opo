# Built-in Functions

Opo provides a set of core built-in functions that are available globally in any Opo program without the need for an import. These functions provide essential functionality for data manipulation, I/O, and system interactions.

## Core Functions

### `len(obj) -> int`
Returns the number of elements in an array or the number of characters in a string.
```opo
len("Opo") !!  # 3
len([1, 2, 3]) !!  # 3
```

### `str(val) -> str`
Converts any value to its string representation. This is commonly used for formatting and debugging.
```opo
str(42) !!  # "42"
str(tr) !!  # "tru"
```

### `typeOf(val) -> str`
Returns a string representing the type of the provided value.
```opo
typeOf(10) !!  # "int"
typeOf("Hello") !!  # "str"
```

### `int(val) -> int!`
Converts a string or float to an integer. Returns a `Result` type containing either the integer or an error if the conversion fails.
```opo
int("100") => res: int!
int(3.14) => res: int!
```

### `flt(val) -> flt!`
Converts a string or integer to a float. Returns a `Result` type.
```opo
flt("3.14") => res: flt!
flt(100) => res: flt!
```

## Console and File I/O

### `print(val) -> void`
Prints the string representation of a value to standard output without a newline.
```opo
print("Hello ")
print("World")
```

### `println(val) -> void`
Prints the string representation of a value to standard output followed by a newline character.
```opo
println("Hello Opo")
```

### `readLine() -> str`
Reads a line of text from the standard input.
```opo
readLine() => user_input: str
```

### `readFile(path: str) -> str!`
Reads the entire contents of a file into a string.
```opo
readFile("data.txt") => content: str!
```

### `writeFile(path: str, content: str) -> bol!`
Writes the provided string content to a file at the specified path.
```opo
writeFile("log.txt", "Action logged") => success: bol!
```

## System and Runtime

### `args() -> []str`
Returns an array containing the command-line arguments passed to the Opo program.
```opo
args() => arguments: []str
```

### `exit(code: int) -> void`
Terminates the program immediately with the provided exit code.
```opo
exit(1)
```

### `clock() -> flt`
Returns the CPU time used by the program since it started, in seconds.
```opo
clock() => uptime: flt
```

### `time() -> int`
Returns the current Unix timestamp as an integer.
```opo
time() => timestamp: int
```

### `system(cmd: str) -> int!`
Executes a system shell command and returns its exit code.
```opo
system("ls -la") => res: int!
```

## Collection Operations

### `append(arr: []type, val: type) -> []type`
Appends a value to the end of a dynamic array and returns the modified array.
```opo
append([1, 2], 3) !!  # [1, 2, 3]
```

### `keys(map: {K:V}) -> []K`
Returns an array of all keys currently stored in a map.
```opo
keys({"a": 1, "b": 2}) !!  # ["a", "b"]
```

### `has(map: {K:V}, key: K) -> bol`
Checks if a specific key exists in a map.
```opo
has({"age": 30}, "age") !!  # tru
```

### `delete(map: {K:V}, key: K) -> void`
Removes a key and its corresponding value from a map.
```opo
delete(my_map, "temporary_key")
```

## Advanced Utilities

### `ascii(char: str) -> int`
Returns the integer ASCII value of the first character in the provided string.
```opo
ascii("A") !!  # 65
```

### `char(code: int) -> str`
Returns a string containing a single character with the specified ASCII value.
```opo
char(65) !!  # "A"
```

### `error(message: any) -> err`
Creates an error value with the provided message, typically used for custom error handling.
```opo
error("Something went wrong") => e: err
```
