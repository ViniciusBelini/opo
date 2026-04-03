# HTTP Library

The `std/http` library provides a high-performance HTTP server for Opo. It's built on native C code for parsing and formatting, while offering a clear, idiomatic Opo API for developers.

## Building an HTTP Server

### 1. Import the Library
```opo
"std/http" => http: imp
```

### 2. Define a Handler Function
A handler function receives an `http.Request` and returns an `http.Response`.

```opo
<req: http.Request> -> http.Response: home [
    http.response(200, {"Content-Type" => "text/plain"}, "Hello from Opo!")
]
```

### 3. Initialize and Start the Server
```opo
<> -> void: main [
    http.server(8080) => s: http.Server
    http.handle(s, "/", home)
    http.start(s)
]
```

## Core Types

### `http.Request`
This struct represents an incoming client request.
- `method: str`: The HTTP method (e.g., "GET", "POST").
- `path: str`: The request path (e.g., "/", "/users").
- `headers: {str:str}`: A map of request headers.
- `body: str`: The raw request body.

### `http.Response`
This struct contains the data to be sent back to the client.
- `status: int`: The HTTP status code (e.g., 200, 404).
- `response_headers: {str:str}`: A map of response headers.
- `response_body: str`: The response body string.

### `http.Server`
The server process that manages incoming connections.
- `port: int`: The port the server is listening on.

## Functions

- **`http.server(port: int) -> Server`**: Creates a new server on the specified port.
- **`http.handle(s: Server, path: str, h: <Request> -> Response)`**: Registers a handler for a path.
- **`http.start(s: Server)`**: Starts the server loop. Each request is automatically handled in a separate `go` routine.
- **`http.response(status: int, headers: {str:str}, body: str) -> Response`**: A helper function for creating response objects.

## Performance and Concurrency

The Opo HTTP library is designed for high-concurrency environments.

1.  **Native Speed**: Core HTTP parsing logic is implemented in C within the Opo VM, minimizing overhead.
2.  **Automatic Goroutines**: Every incoming request is automatically processed in its own lightweight `go` routine, allowing the server to handle multiple simultaneous connections efficiently.
3.  **Deterministic GC**: Uses Opo's reference counting for efficient, predictable memory management.
