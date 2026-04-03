# Opo Bytecode Specification

## Instructions

| Instruction | Args | Description |
|-------------|------|-------------|
| HALT        | 0    | Stop execution |
| PUSH_INT    | 1    | Push a 64-bit integer to stack |
| PUSH_FLT    | 1    | Push a 64-bit float to stack |
| PUSH_STR    | 1    | Push string index to stack |
| PUSH_BOOL   | 1    | Push boolean (0 or 1) to stack |
| ADD         | 0    | Pop b, pop a, push a + b |
| SUB         | 0    | Pop b, pop a, push a - b |
| MUL         | 0    | Pop b, pop a, push a * b |
| DIV         | 0    | Pop b, pop a, push a / b |
| MOD         | 0    | Pop b, pop a, push a % b |
| EQ          | 0    | Pop b, pop a, push a == b |
| LT          | 0    | Pop b, pop a, push a < b |
| GT          | 0    | Pop b, pop a, push a > b |
| AND         | 0    | Pop b, pop a, push a && b |
| OR          | 0    | Pop b, pop a, push a \|\| b |
| NOT         | 0    | Pop a, push !a |
| PRINT       | 0    | Pop a, print its value |
| STORE       | 1    | Pop a, store in local variable slot |
| LOAD        | 1    | Load from local variable slot, push to stack |
| JUMP        | 1    | Unconditional jump to address |
| JUMP_IF_F   | 1    | Pop a, if false, jump to address |
| CALL        | 1    | Call function at address |
| RET         | 0    | Return from function |
