# Bytecode Specification

Opo's compiler translates high-level source code into a compact, optimized bytecode format designed for efficient execution on the Opo Virtual Machine.

## Instruction Set

Every Opo bytecode instruction consists of an opcode and an optional argument.

| Opcode | Args | Description |
| :--- | :--- | :--- |
| `HALT` | 0 | Stops the execution of the VM immediately. |
| `PUSH_INT` | 1 | Pushes a 64-bit integer literal to the operand stack. |
| `PUSH_FLT` | 1 | Pushes a 64-bit floating-point literal to the operand stack. |
| `PUSH_STR` | 1 | Pushes the index of a string in the constant pool to the stack. |
| `PUSH_BOOL` | 1 | Pushes a boolean literal (0 for `fls`, 1 for `tru`) to the stack. |
| `ADD` | 0 | Pops two values, adds them, and pushes the result. |
| `SUB` | 0 | Pops two values, subtracts the second from the first, and pushes the result. |
| `MUL` | 0 | Pops two values, multiplies them, and pushes the result. |
| `DIV` | 0 | Pops two values, divides the first by the second, and pushes the result. |
| `MOD` | 0 | Pops two values, calculates the remainder, and pushes the result. |
| `EQ` | 0 | Pops two values, pushes `tru` if they are equal, `fls` otherwise. |
| `LT` | 0 | Pops two values, compares them, and pushes `tru` if the first is less than the second. |
| `GT` | 0 | Pops two values, compares them, and pushes `tru` if the first is greater than the second. |
| `AND` | 0 | Performs a logical AND on two popped boolean values. |
| `OR` | 0 | Performs a logical OR on two popped boolean values. |
| `NOT` | 0 | Inverts the popped boolean value. |
| `PRINT` | 0 | Pops a value and prints its string representation to standard output. |
| `STORE` | 1 | Pops a value and stores it in the specified local variable slot. |
| `LOAD` | 1 | Copies a value from a local variable slot and pushes it to the stack. |
| `JUMP` | 1 | Updates the instruction pointer to the specified address. |
| `JUMP_IF_F` | 1 | Pops a value; if it's `fls`, jumps to the specified address. |
| `CALL` | 1 | Pushes the return address and jumps to the starting instruction of a function. |
| `RET` | 0 | Pops the return value, clears the current stack frame, and jumps back to the return address. |

## Execution Model

Opo's VM is a **stack-based** machine. Most operations work by popping values from the top of the stack, performing a calculation, and pushing the result back onto the stack. This model simplifies the compiler design and results in a clean, predictable instruction set.
