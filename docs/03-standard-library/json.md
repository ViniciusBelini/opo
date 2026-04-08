# JSON Library

The `std/json` module provides functions for parsing JSON strings into Opo values and stringifying Opo values into JSON.

## The `std/json` Module

### Parsing JSON
To parse a JSON string, use the `parse` function. It returns a `Result` type.

```opo
use "std/json" as json

match json.parse("{\"name\": \"Opo\", \"version\": 1.0}") [
    ok(val) [
        "Language: " + val.name !!
        "Version: " + str(val.version) !!
    ]
    err(e) [ "Failed to parse JSON: " + e !! ]
]
```

You can also use `parse_or` to provide a default value if parsing fails:

```opo
json.parse_or("invalid json", { name: "default" }) => data: {name: str}
```

### Stringifying Values
To convert a value to a JSON string, use `stringify`, `encode`, or `format`.

- **`stringify(val: any) -> str`**: Converts a value to a compact JSON string.
- **`encode(val: any) -> str`**: Alias for `stringify(val)`.
- **`format(val: any, indent: int) -> str`**: Converts a value to JSON with specified indentation.

```opo
{ "name" => "Opo", "features" => ["simple", "fast"] } => lang: {str:any}
json.format(lang, 2) => formatted: str
json.stringify(lang) => compact: str
```

## Supported Types
The following Opo types are supported for JSON conversion:

- **int**: Becomes a JSON number.
- **flt**: Becomes a JSON number.
- **str**: Becomes a JSON string with automatic escaping.
- **bol**: Becomes `true` or `false`.
- **void**: Becomes `null`.
- **[]any**: Becomes a JSON array.
- **{str:any}**: Becomes a JSON object.

## Low-level Natives
The following natives are available globally for JSON operations:

- **`json_parse(json: str) -> any!`**: Parses a JSON string.
- **`json_stringify(val: any, indent: int) -> str`**: Stringifies a value.
