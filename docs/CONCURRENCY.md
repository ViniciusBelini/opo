# Concurrency in Opo

Opo implements Go-style concurrency using Goroutines and Channels.

## Goroutines

To start a new thread of execution, use the `go` keyword followed by a function call.

```opo
<name: str> -> void: say_hello [
    "Hello " + name !!
]

<> -> void: main [
    go say_hello("World")
    "Main thread" !!
]
```

When the `main` function returns, the program exits immediately, even if other goroutines are still running.

## Channels

Channels are used to communicate between goroutines. They are typed and can be buffered.

### Creation

`chan<type>(capacity)`

```opo
chan<int>(10) => ch: chan<int> # A buffered channel of integers with capacity 10
```

### Sending

`channel <- value`

```opo
ch <- 42
```

The send operation blocks if the channel is full.

### Receiving

`<-channel`

```opo
<-ch => val: int
```

The receive operation blocks if the channel is empty.

### Closing

`close(channel)`

Closes the channel. Subsequent sends will fail. Receives will continue until the buffer is empty, then return `void` (or a way to check if closed is needed in the future).

### Example

```opo
<ch: chan<int>> -> void: producer [
    0 => i: int
    i < 5 @ [
        ch <- i
        i + 1 => i
    ]
    close(ch)
]

<> -> void: main [
    chan<int>(0) => ch: chan<int>
    go producer(ch)
    
    # Receive until closed (simple example)
    <-ch !!
    <-ch !!
    <-ch !!
    <-ch !!
    <-ch !!
]
```
