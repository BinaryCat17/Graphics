#pragma once

#include <stddef.h>
#include <threads.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Simple chunked array that stores a contiguous run of components. The
 * manager allocates multiple chunks to avoid expensive reallocations while
 * keeping cache-friendly layout for Structure-of-Arrays consumers.
 */
typedef struct StateChunk {
    unsigned char *data;
    char **keys;
    size_t count;
    size_t capacity;
} StateChunk;

/**
 * Runtime registration of a component type.
 *  - name: human readable identifier for debugging / introspection
 *  - component_size: size in bytes for a single component
 *  - chunk_capacity: per-chunk capacity; chunked growth avoids moving
 *    existing data that might be referenced by asynchronous readers.
 */
typedef struct StateComponentPool {
    char *name;
    size_t component_size;
    size_t per_chunk_capacity;
    StateChunk *chunks;
    size_t chunk_count;
    size_t chunk_array_capacity;
} StateComponentPool;

/** Event kinds published to subscribers. */
typedef enum StateEventKind {
    STATE_EVENT_COMPONENT_ADDED = 0,
    STATE_EVENT_COMPONENT_UPDATED = 1,
    STATE_EVENT_COMPONENT_REMOVED = 2,
} StateEventKind;

/**
 * A subscription event. Payload is an owned copy; listeners are free to keep
 * the pointer until they call state_event_dispose().
 */
typedef struct StateEvent {
    StateEventKind kind;
    int type_id;
    char *type_name;
    char *key;
    void *payload;
    size_t payload_size;
} StateEvent;

/**
 * Thread-safe ring buffer used to coordinate asynchronous services that
 * listen to component mutations.
 */
typedef struct StateEventQueue {
    StateEvent *events;
    size_t capacity;
    size_t count;
    size_t head;
    size_t tail;
    mtx_t mutex;
    cnd_t cond;
} StateEventQueue;

/** Function signature invoked for each event delivered to a subscriber. */
typedef void (*StateEventHandler)(const StateEvent *event, void *user_data);

/** Subscriber filtered by component type and optional key. */
typedef struct StateSubscriber {
    int type_id;
    char *key; // optional filter; NULL means receive all keys for the type
    StateEventHandler handler;
    void *user_data;
} StateSubscriber;

/** Central registry and SoA storage for shared state. */
typedef struct StateManager {
    StateComponentPool *pools;
    size_t pool_count;
    size_t pool_capacity;

    StateSubscriber *subscribers;
    size_t subscriber_count;
    size_t subscriber_capacity;

    StateEventQueue event_queue;
} StateManager;

void state_manager_init(StateManager *manager, size_t initial_types, size_t initial_queue_capacity);
void state_manager_dispose(StateManager *manager);

/** Register a new component pool. Returns a type id or -1 on error. */
int state_manager_register_type(StateManager *manager, const char *name, size_t component_size, size_t chunk_capacity);

/**
 * Allocate or update a component instance addressed by (type_id, key).
 * If the key already exists its contents are overwritten; otherwise a new
 * component is appended without moving existing chunks.
 */
void *state_manager_write(StateManager *manager, int type_id, const char *key, const void *data);

/** Lookup an existing component by key. Returns NULL when missing. */
void *state_manager_get(StateManager *manager, int type_id, const char *key);

/** Remove a component by key. Returns 1 if removed, 0 if not found. */
int state_manager_remove(StateManager *manager, int type_id, const char *key);

/** Register a subscriber filtered by type and optional key. */
int state_manager_subscribe(StateManager *manager, int type_id, const char *key, StateEventHandler handler, void *user_data);

/** Drain the event queue and synchronously notify subscribers. */
void state_manager_dispatch(StateManager *manager, int wait_for_event);

/** Manually publish an event for already-populated payloads. */
int state_manager_publish(StateManager *manager, StateEventKind kind, int type_id, const char *key, const void *payload, size_t payload_size);

/** Release owned allocations in an event object. */
void state_event_dispose(StateEvent *event);

/**
 * Integration guidance:
 *  - Scene loading code should register a component type for parsed scenes
 *    (e.g., type name "scene") and publish STATE_EVENT_COMPONENT_ADDED
 *    after loading instead of handing raw pointers to modules.
 *  - Future systems can subscribe to type/key pairs (for example, physics
 *    requests updates for "part" components) and receive immutable payload
 *    copies through the event queue, decoupling producer and consumer threads.
 */

#ifdef __cplusplus
}
#endif

