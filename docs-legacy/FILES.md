# File Manipulation

Opo provides built-in functions and a standard library module `std/fs` for managing files.

## `std/fs` Module

It is recommended to use the `std/fs` module for a cleaner interface.

```opo
"std/fs" => fs: imp

<> -> void: main[
    match fs.read_file("README.md") [
        ok(content) [
            "File content: " + content !!
        ]
        err(e) [
            "Error: " + e !!
        ]
    ]
]
```

### Functions in `std/fs`

- `fs.read_file(path: str) -> str!`: Reads entire file.
- `fs.write_file(path: str, content: str) -> bol!`: Writes content to file.
- `fs.exists(path: str) -> bol`: Checks if file exists.
- `fs.remove(path: str) -> bol!`: Deletes a file.
- `fs.list_dir(path: str) -> []str!`: Lists directory contents.

## Low-level Natives

The following natives are available globally:
- `readFile(path: str) -> str!`
- `writeFile(path: str, content: str) -> bol!`
- `fileExists(path: str) -> bol`
- `removeFile(path: str) -> bol!`
- `listDir(path: str) -> []str!`
