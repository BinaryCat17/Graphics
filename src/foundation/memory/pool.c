#include "pool.h"
#include <stdlib.h>
#include <string.h>

// --- Internal Types ---

typedef struct PoolBlock {
    void* memory;               // The raw memory chunk
    struct PoolBlock* next;     // Next block in chain
} PoolBlock;

struct MemoryPool {
    size_t item_size;
    size_t block_capacity;
    size_t block_bytes;         // Precomputed: item_size * block_capacity
    
    PoolBlock* head_block;      // Linked list of allocated blocks
    
    void* free_list;            // Head of the free list (points to a free slot)
};

// --- Implementation ---

MemoryPool* pool_create(size_t item_size, size_t block_capacity) {
    if (item_size == 0 || block_capacity == 0) return NULL;
    
    // Ensure item_size is at least large enough to hold a pointer (for the free list)
    if (item_size < sizeof(void*)) {
        item_size = sizeof(void*);
    }
    
    MemoryPool* pool = (MemoryPool*)malloc(sizeof(MemoryPool));
    if (!pool) return NULL;
    
    pool->item_size = item_size;
    pool->block_capacity = block_capacity;
    pool->block_bytes = item_size * block_capacity;
    pool->head_block = NULL;
    pool->free_list = NULL;
    
    return pool;
}

static void pool_add_block(MemoryPool* pool) {
    // 1. Allocate block metadata
    PoolBlock* block = (PoolBlock*)malloc(sizeof(PoolBlock));
    if (!block) return;
    
    // 2. Allocate data memory
    block->memory = malloc(pool->block_bytes);
    if (!block->memory) {
        free(block);
        return;
    }
    
    // 3. Link block
    block->next = pool->head_block;
    pool->head_block = block;
    
    // 4. Initialize free list inside the new block
    // We link all new slots together.
    uint8_t* ptr = (uint8_t*)block->memory;
    for (size_t i = 0; i < pool->block_capacity - 1; ++i) {
        void* current = ptr + (i * pool->item_size);
        void* next = ptr + ((i + 1) * pool->item_size);
        *(void**)current = next; // Store pointer to next
    }
    
    // Last item points to the *existing* free list (growing the stack)
    void* last = ptr + ((pool->block_capacity - 1) * pool->item_size);
    *(void**)last = pool->free_list;
    
    // Update head
    pool->free_list = ptr; // Start of this new block is the new free head
}

void pool_destroy(MemoryPool* pool) {
    if (!pool) return;
    
    PoolBlock* current = pool->head_block;
    while (current) {
        PoolBlock* next = current->next;
        free(current->memory);
        free(current);
        current = next;
    }
    
    free(pool);
}

void* pool_alloc(MemoryPool* pool) {
    if (!pool) return NULL;
    
    if (!pool->free_list) {
        pool_add_block(pool);
    }
    
    // Still empty? OOM.
    if (!pool->free_list) return NULL;
    
    // Pop from free list
    void* slot = pool->free_list;
    pool->free_list = *(void**)slot; // Next free slot is stored inside
    
    // Zero memory for safety (optional, but good for "Professional" reliability)
    memset(slot, 0, pool->item_size);
    
    return slot;
}

void pool_free(MemoryPool* pool, void* ptr) {
    if (!pool || !ptr) return;
    
    // Push back to free list
    *(void**)ptr = pool->free_list;
    pool->free_list = ptr;
}

void pool_clear(MemoryPool* pool) {
    if (!pool) return;
    
    // We need to rebuild the free list across ALL blocks.
    pool->free_list = NULL;
    
    PoolBlock* block = pool->head_block;
    while (block) {
        uint8_t* ptr = (uint8_t*)block->memory;
        
        // Link internal items
        for (size_t i = 0; i < pool->block_capacity - 1; ++i) {
            void* current = ptr + (i * pool->item_size);
            void* next = ptr + ((i + 1) * pool->item_size);
            *(void**)current = next;
        }
        
        // Last item links to current global free list
        void* last = ptr + ((pool->block_capacity - 1) * pool->item_size);
        *(void**)last = pool->free_list;
        
        // Update global head to start of this block
        pool->free_list = ptr;
        
        block = block->next;
    }
}
