# Keywords and Operators

Opo uses a minimalist set of keywords and a rich set of symbolic operators to define the language's structure and behavior.

## Keywords

The following words are reserved by the Opo language and cannot be used as identifiers (variable or function names).

| Keyword | Description |
| :--- | :--- |
| `pub` | Marks a function or struct as public (exported) in a module. |
| `imp` | Used in import declarations to specify the import type. |
| `type` | Used when defining new types (structs, enums, or type aliases). |
| `struct` | Starts a struct definition. |
| `enum` | Starts an enum definition. |
| `match` | Starts a pattern matching block. |
| `go` | Spawns a new goroutine for concurrent execution. |
| `chan` | Used to declare or create a channel. |
| `as` | Performs an explicit type cast. |
| `tru` | Boolean literal for true. |
| `fls` | Boolean literal for false. |
| `some` | Wraps a value in an `Option`. |
| `none` | Represents an empty `Option`. |
| `ok` | Variant for a successful `Result`. |
| `err` | Variant for a failed `Result`. |
| `try` | (Reserved for future Use) Exception handling. |
| `catch` | (Reserved for future Use) Exception handling. |
| `throw` | (Reserved for future Use) Exception handling. |

## Operators

Opo relies heavily on symbolic operators for a concise and expressive syntax.

### Assignment and Declaration
- `=>`: Used for both declaring new variables and reassigning existing ones.

### Arithmetic
- `+`, `-`, `*`, `/`, `%`

### Comparison
- `==`, `!=`, `<`, `>`, `<=`, `>=`

### Logic
- `&&`: Logical AND
- `||`: Logical OR
- `!`: Logical NOT

### Control Flow Symbols
- `?`: "If" separator
- `:`: "Else" separator
- `@`: While loop symbol
- `.`: Break symbol
- `..`: Continue symbol
- `^`: Return symbol

### Access and Delimiters
- `( )`: Used for function calls, grouping expressions, and parameter lists.
- `[ ]`: Used for code blocks, array literals, and struct/enum definitions.
- `{ }`: Used for map literals and map types.
- `< >`: Used for function parameter definitions and generic type arguments.
- `.` : Used for accessing struct fields, map values, array elements, and module members.
- `!!`: Postfix operator for printing to standard output.
- `<-`: Send/Receive operator for channels.
- `->`: Return type separator in function definitions.
