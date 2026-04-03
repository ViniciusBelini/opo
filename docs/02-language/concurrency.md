# Concurrency

Opo implements Go-style concurrency using Goroutines and Channels. This model allows for high-performance concurrent programming that is easier to reason about than traditional threads and locks.

## Goroutines

To start a new thread of execution, use the `go` keyword followed by a function call.

```opo
<name: str> -> void: say_hello [
    "Hello " + name !!
]

<> -> void: main [
    go say_hello("Opo")
    "Main thread" !!
]
```

When the `main` function returns, the program exits immediately, even if other goroutines are still running.

## Channels

Channels are used to communicate and synchronize data between goroutines. They are strictly typed and can be buffered.

### Channel Creation
`chan<type>(capacity) => ch: chan<type>`

- **Type**: The type of values the channel will carry.
- **Capacity**: The size of the internal buffer. A capacity of `0` creates an unbuffered (blocking) channel.

Example:
`chan<int>(10) => ch: chan<int>`

### Sending to a Channel (`<-`)
`channel <- value`

The send operation will block if the channel's buffer is full (or if it is an unbuffered channel and there is no receiver).

```opo
ch <- 42
```

### Receiving from a Channel (`<-`)
`<-channel => val: type`

The receive operation will block if the channel's buffer is empty.

```opo
<-ch => val: int
val !!
```

### Closing a Channel
`close(channel)`

Closing a channel indicates that no more values will be sent. Subsequent sends will fail, but receivers can continue to drain any remaining values in the buffer.

## Example: Producer-Consumer

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
    
    # Receive 5 values
    <-ch !!
    <-ch !!
    <-ch !!
    <-ch !!
    <-ch !!
]
```
