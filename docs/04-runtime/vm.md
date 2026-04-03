# The Opo Virtual Machine (VM)

The Opo Virtual Machine is a state-of-the-art, stack-based execution environment optimized for performance and safety. Written in C, the VM is designed to run Opo's custom bytecode with minimal overhead.

## Architecture

Opo's VM uses a **stack-based execution model**. This means that most instructions (like addition or comparison) work on the operand stack, popping their inputs and pushing their results back. This architecture is clean, simple, and avoids the complexity of register-based machines.

### 1. The Operand Stack
The central component of the VM is a stack of `Value` objects. A `Value` in Opo can be an integer, float, boolean, or a pointer to a heap-resident object. The stack is used for temporary data and intermediate calculations.

### 2. Call Frames
Whenever a function is called, the VM pushes a new `CallFrame` onto the frame stack. Each frame contains:
- **Function Index**: The index of the function being executed.
- **Instruction Pointer (IP)**: The address of the next bytecode instruction to run.
- **Base Pointer (BP)**: A pointer to the start of the current function's local variables on the overall stack.

### 3. Local Variables
Local variables are stored directly on the stack, making access fast and avoiding additional heap allocations. When a function returns, its frame is popped, and its local variables are automatically cleared.

## Concurrency and Parallelism

The Opo VM has built-in support for concurrency through **Goroutines**.

- **Lightweight Threads**: Every `go` call spawns a new execution thread. Under the hood, these use POSIX threads (`pthreads`) for true parallelism on multi-core systems.
- **Shared Memory**: Goroutines share the same global constant pool, while maintaining their own private operand and frame stacks.
- **Synchronization**: The VM provides thread-safe primitives (Channels) with internal locking and condition variables to ensure safe communication between concurrent routines.

## Performance Features

### 1. Fast Opcode Dispatch
The VM uses an optimized loops-and-switch dispatch mechanism for its instruction loop, ensuring that the next instruction is executed as quickly as possible.

### 2. Native C Integration
Critical functions (such as string manipulation, math, and HTTP parsing) are implemented as "Native Functions" in C. When Opo code calls these, the VM executes the C logic directly, providing near-native performance for intensive tasks.

### 3. Efficient Object Layout
Objects on the heap are laid out for optimal cache performance and reference counting speed. Atomic operations are used for shared object reference counting to ensure thread safety without degrading performance.
