# I/O and File System

Opo provides simple and efficient mechanisms for Input/Output (I/O) operations, with a focus on file system management through the `std/fs` module.

## The `std/fs` Module

The `std/fs` module is the recommended way to interact with the file system. It provides high-level functions for reading, writing, and listing files.

### Reading a File
```opo
match fs.read_file("content.txt") [
    ok(content) [ "File contents: " + content !! ]
    err(e) [ "Error occurred: " + e !! ]
]
```

### Writing to a File
```opo
fs.write_file("new_file.txt", "Hello Opo!") => success: bol
```

### Checking if a File Exists
```opo
fs.exists("test.opo") ? [ "Found it!" !! ] : [ "Not found." !! ]
```

### Deleting a File
```opo
fs.remove("temp.txt") !!
```

### Listing Directory Contents
```opo
fs.list_dir(".") => files: []str
```

## Low-level Natives

The Following natives are available globally for basic I/O operations without needing to import a module:

- **`readFile(path: str) -> str!`**: Reads the entire contents of a file into a string. Returns an error type if the operation fails.
- **`writeFile(path: str, content: str) -> bol!`**: Writes a string to a file. Returns `tru` on success or an error type on failure.
- **`fileExists(path: str) -> bol`**: Returns `tru` if the file exists, `fls` otherwise.
- **`removeFile(path: str) -> bol!`**: Deletes the specified file.
- **`listDir(path: str) -> []str!`**: Returns an array of strings representing the names of the files in the directory.

## Error Handling

I/O operations in Opo typically return a `Result` type (often represented as `!` in the return type). It is recommended to use `match` to handle potential errors gracefully.
```opo
match readFile("config.json") [
    ok(data) [ process(data) ]
    err(_) [ "Configuration file not found." !! ]
]
```