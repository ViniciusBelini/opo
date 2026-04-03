# Arrays in Opo

Opo supports dynamic arrays that are strictly typed.

## Syntax

### Array Type
`[]type`

Example:
`[]int => my_array: []int`

Opo enforces that all elements in the array match the declared element type.

### Array Literal
`[element1, element2, ...]`

Example:
`[1, 2, 3] => numbers: []int`

The compiler infers the element type from the first element if not explicitly declared, and ensures all subsequent elements match.

### Accessing Elements
Opo uses the dot operator followed by an integer literal for indexing (for consistency with struct member access).

`numbers.0 !!` (Prints the first element)

## Error Handling
Accessing an index that is negative or greater than or equal to the array's length will result in a **Runtime Error** with a descriptive message.

## Built-in Functions for Arrays

- `len(arr)`: Returns the number of elements in the array.
- `append(arr, value)`: Appends a value to the array and returns the array. The compiler ensures that `value` matches the array's element type.

## Important Note on Usage
Since `append` modifies the array in-place, you don't necessarily need to assign the result back to the variable, but it's common practice:
`append(my_array, 10) => my_array`

You can access elements using variables or expressions as well:
`1 => i: int`
`my_array.i !!`
`my_array.(i + 1) !!`
