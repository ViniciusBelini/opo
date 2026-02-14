# HTTP Support

Opo provides simple HTTP client support through the `std/net` module.

## `net.get(url: str) -> str!`

Performs an HTTP GET request and returns a `Result` containing the response body as a string.

### Example

```opo
"std/net" => net: imp

<> -> void: main [
    match net.get("https://api.github.com/zen") [
        ok(zen) [ "GitHub says: " + zen !! ]
        err(e) [ "Failed to fetch: " + e !! ]
    ]
]
```

Note: Currently, HTTP support depends on the `curl` command being available on the system.
