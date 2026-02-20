# Maps in Opo

Opo supports hash maps (dictionaries) that associate keys with values. Maps are strictly typed.

## Syntax

### Map Type
`{key_type:value_type}`

Example:
`{str:int} => ages: {str:int}`

### Map Literal
`{ key1 => value1, key2 => value2, ... }`

Example:
`{ "Alice" => 30, "Bob" => 25 } => ages: {str:int}`

## Accessing and Updating

### Getting a Value
Use the dot operator with the key. If the key is a string literal, you can use `"key"`. If it's a variable or expression, use parentheses.

`ages."Alice" !!`
`"Alice" => name: str`
`ages.name !!`

### Setting a Value
`31 => ages."Alice"`
`26 => ages.name`

## Error Handling
Accessing a key that does not exist in the map will result in a **Runtime Error**.

To safely check if a key exists, use the `has(map, key)` built-in function.

## Built-in Functions for Maps

- `len(map)`: Returns the number of entries in the map.
- `has(map, key)`: Returns `tru` if the key exists in the map, `fls` otherwise.
- `keys(map)`: Returns an array containing all keys in the map.
- `delete(map, key)`: Removes the entry with the specified key from the map.

## Type Safety
The Opo compiler enforces that:
- All keys in a map literal match the declared `key_type`.
- All values in a map literal match the declared `value_type`.
- Any value assigned to a map entry matches the `value_type`.
- Any key used to index the map matches the `key_type`.
