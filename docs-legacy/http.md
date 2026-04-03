# Opo HTTP Library Documentation

The `std/http` library provides a performant and self-sufficient way to create HTTP servers in Opo. It leverages native C functions in the Opo VM for high-speed parsing and formatting, while offering a clean, idiomatic Opo API.

## Core Types

### `Request`
Represents an incoming HTTP request.
- `method: str`: The HTTP method (e.g., "GET", "POST").
- `path: str`: The request path (e.g., "/", "/hello").
- `headers: {str:str}`: A map containing the request headers.
- `body: str`: The request body as a string.

### `Response`
Represents an HTTP response to be sent back to the client.
- `status: int`: The HTTP status code (e.g., 200, 404).
- `response_headers: {str:str}`: A map containing response headers.
- `response_body: str`: The response body string.

*Note: The fields are named \`response_headers\` and \`response_body\` to avoid name collisions.*

### `Server`
Represents the HTTP server instance.
- `port: int`: The port the server listens on.
- `handlers: {str: <Request> -> Response}`: A map of path strings to handler functions.

## Functions

### `http.server(port: int) -> Server`
Creates a new server instance listening on the specified port.

### `http.handle(s: Server, path: str, h: <Request> -> Response) -> void`
Registers a handler function for a specific path. The handler function must take a `Request` and return a `Response`.

### `http.response(status: int, headers: {str:str}, body: str) -> Response`
A helper function to easily create a `Response` object.

### `http.start(s: Server) -> void`
Starts the HTTP server loop. The server will handle requests concurrently using Opo's `go` routines.

## Usage Example

```opo
"std/http" => http: imp

// Define a handler function
<req: http.Request> -> http.Response: home [
    http.response(200, {"Content-Type" => "text/plain"}, "Welcome to Opo HTTP Server!")
]

<req: http.Request> -> http.Response: greet [
    "Hello! You visited " + req.path => msg: str
    http.response(200, {"Content-Type" => "text/plain"}, msg)
]

<> -> void: main [
    // Initialize server on port 8080
    http.server(8080) => s: http.Server
    
    // Register routes
    http.handle(s, "/", home)
    http.handle(s, "/greet", greet)
    
    // Start server
    http.start(s)
]
```

## Performance Features

- **Native Parsing**: Core HTTP parsing logic is implemented in C within the Opo VM, ensuring minimal overhead when handling request headers and bodies.
- **Concurrent Handling**: Each request is automatically processed in a separate lightweight `go` routine, allowing the server to handle multiple simultaneous connections efficiently.
- **Reference Counting**: Uses Opo's deterministic reference counting for efficient memory management of request and response data.
