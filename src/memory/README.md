# Memory helpers

`memory/buffer.*` centralizes the dynamic buffer growth logic used across rendering utilities. Buffers grow by doubling their
capacity (`MEM_BUFFER_GROWTH_DOUBLE`) until they satisfy the requested size, returning `-1` on allocation or overflow failures.

Use `MEM_BUFFER_DECLARE(Type, DefaultCapacity)` to generate `*_mem_init`, `*_mem_dispose`, and `*_mem_reserve` helpers for buffer
structs that expose `data`, `count`, and `capacity` fields. Call these from public buffer APIs to avoid repeating allocation
patterns while keeping consistent error handling and initialization defaults.
