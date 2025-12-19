#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// --- Opaque Handle ---
// A pool manages objects of a FIXED size.
// It grows dynamically by allocating "blocks" (pages) from the system/arena.
typedef struct MemoryPool MemoryPool;

// --- API ---

// Create a new pool.
// item_size: Size of each element in bytes.
// block_capacity: How many items to store per block allocation (e.g., 256).
MemoryPool* pool_create(size_t item_size, size_t block_capacity);

// Destroy the pool and free all blocks.
void pool_destroy(MemoryPool* pool);

// Allocate one slot. Returns a pointer to zeroed memory.
// Returns NULL on system OOM.
void* pool_alloc(MemoryPool* pool);

// Return a slot to the pool.
void pool_free(MemoryPool* pool, void* ptr);

// Reset the pool (free all items, but keep blocks allocated for reuse).
// Ideally, this just resets the free list logic.
void pool_clear(MemoryPool* pool);

// Iterator support (Simple linear scan is tricky in pools without a dense index).
// For now, we assume the user maintains pointers or handles to active objects.

#endif // MEMORY_POOL_H
