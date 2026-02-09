# Arrays in Opo

Opo supports dynamic arrays that can hold any type of value (though currently they are loosely typed at runtime).

## Syntax

### Array Type
`[]type`

Example:
`[]int => my_array: []int`

### Array Literal
`[element1, element2, ...]`

Example:
`[1, 2, 3] => numbers: []int`

### Accessing Elements
Opo uses the dot operator followed by an integer literal for indexing (for consistency with struct member access).

`numbers.0 !!` (Prints the first element)

## Built-in Functions for Arrays

- `len(arr)`: Returns the number of elements in the array.
- `append(arr, value)`: Appends a value to the array and returns the array.
