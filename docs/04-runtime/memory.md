# Memory Management

Opo uses a deterministic **Reference Counting** mechanism for memory management. This ensures that memory is freed as soon as it is no longer needed, providing predictable performance and reducing the overhead associated with traditional garbage collection.

## Reference Counting

Every heap-allocated object in Opo (such as strings, arrays, maps, and structs) keeps track of how many references are pointing to it.

- **`retain`**: When a new reference to an object is created (e.g., assigning it to a variable or passing it to a function), its reference count is incremented.
- **`release`**: When a reference goes out of scope or is reassigned, the object's reference count is decremented.

When an object's reference count reaches zero, it is immediately deallocated, and any objects it referenced are also released.

## Heap Objects

The following types are managed on the heap:

1.  **Strings (`OBJ_STRING`)**: Immutable sequences of characters.
2.  **Arrays (`OBJ_ARRAY`)**: Dynamic collections of values.
3.  **Maps (`OBJ_MAP`)**: Hash tables for key-value pairs.
4.  **Structs (`OBJ_STRUCT`)**: Grouped collection of named values.
5.  **Enums (`OBJ_ENUM`)**: Tagged unions with optional payloads.
6.  **Channels (`OBJ_CHAN`)**: Synchronization primitives for concurrency.

## Safety and Performance

### No Manual Free
Developers do not need to manually free memory. The reference counting system handles it automatically and safely.

### Predictable Latency
Unlike mark-and-sweep garbage collectors, Opo's memory management does not have "stop-the-world" pauses. Memory is reclaimed in small, predictable increments.

### Circular References
Currently, Opo's reference counting does not automatically handle circular references (e.g., Object A referencing Object B, and Object B referencing Object A). Developers should be mindful of these patterns to avoid memory leaks.

## Optimization: Atomic Counting
For concurrent environments, Opo uses atomic operations for reference counting on shared objects (like items sent through channels), ensuring thread safety without heavy global locks.
