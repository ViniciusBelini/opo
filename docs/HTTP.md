# HTTP Support

Opo provides simple HTTP client support through the `std/net` module.

## `net.get(url: str) -> str`

Performs an HTTP GET request and returns the response body as a string.

### Example

```opo
"std/net" => net: imp

<> -> void: main [
    net.get("https://api.github.com/zen") => zen: str
    "GitHub says: " + zen !!
]
```

Note: Currently, HTTP support depends on the `curl` command being available on the system.
