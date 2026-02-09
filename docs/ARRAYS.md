# Arrays in Opo

Opo supports dynamic arrays that can hold any type of value (though currently they are loosely typed at runtime).

## Syntax

### Array Type
`[]type`

Example:
`[]int => my_array: []int`
Note: Currently the element type in the declaration is for documentation and future use; arrays are dynamically typed at runtime.

### Array Literal
`[element1, element2, ...]`

Example:
`[1, 2, 3] => numbers: []int`

### Accessing Elements
Opo uses the dot operator followed by an integer literal for indexing (for consistency with struct member access).

`numbers.0 !!` (Prints the first element)

## Built-in Functions for Arrays

- `len(arr)`: Returns the number of elements in the array.
- `append(arr, value)`: Appends a value to the array and returns the array. The array is modified in-place.

## Important Note on Usage
Since `append` modifies the array in-place, you don't necessarily need to assign the result back to the variable, but it's common practice:
`append(my_array, 10) => my_array`

You can access elements using variables or expressions as well:
`1 => i: int`
`my_array.i !!`
`my_array.(i + 1) !!`
