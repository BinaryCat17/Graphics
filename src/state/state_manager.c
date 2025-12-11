#include "state_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STATE_MIN_CHUNK_CAPACITY 64
#define STATE_MIN_POOL_CAPACITY 4
#define STATE_MIN_SUBSCRIBER_CAPACITY 4

static char *state_strdup(const char *text) {
    if (!text) {
        return NULL;
    }
    size_t len = strlen(text);
    char *copy = (char *)malloc(len + 1);
    if (!copy) {
        return NULL;
    }
    memcpy(copy, text, len + 1);
    return copy;
}

static void state_chunk_dispose(StateChunk *chunk) {
    if (!chunk) {
        return;
    }
    free(chunk->data);
    if (chunk->keys) {
        for (size_t i = 0; i < chunk->count; ++i) {
            free(chunk->keys[i]);
        }
    }
    free(chunk->keys);
    chunk->data = NULL;
    chunk->keys = NULL;
    chunk->capacity = 0;
    chunk->count = 0;
}

static void state_component_pool_dispose(StateComponentPool *pool) {
    if (!pool) {
        return;
    }
    for (size_t i = 0; i < pool->chunk_count; ++i) {
        state_chunk_dispose(&pool->chunks[i]);
    }
    free(pool->chunks);
    free(pool->name);
    pool->chunks = NULL;
    pool->name = NULL;
    pool->chunk_count = 0;
    pool->chunk_array_capacity = 0;
}

const char *state_manager_result_message(StateManagerResult result) {
    switch (result) {
        case STATE_MANAGER_OK:
            return "ok";
        case STATE_MANAGER_ERROR_INVALID_ARGUMENT:
            return "invalid argument";
        case STATE_MANAGER_ERROR_ALLOCATION_FAILED:
            return "allocation failed";
    }
    return "unknown error";
}

static int state_component_pool_grow(StateComponentPool *pool, size_t component_size) {
    if (pool->chunk_count == pool->chunk_array_capacity) {
        size_t new_capacity = pool->chunk_array_capacity ? pool->chunk_array_capacity * 2 : STATE_MIN_POOL_CAPACITY;
        StateChunk *new_chunks = (StateChunk *)realloc(pool->chunks, new_capacity * sizeof(StateChunk));
        if (!new_chunks) {
            return 0;
        }
        for (size_t i = pool->chunk_array_capacity; i < new_capacity; ++i) {
            new_chunks[i].data = NULL;
            new_chunks[i].keys = NULL;
            new_chunks[i].count = 0;
            new_chunks[i].capacity = 0;
        }
        pool->chunks = new_chunks;
        pool->chunk_array_capacity = new_capacity;
    }

    StateChunk *chunk = &pool->chunks[pool->chunk_count];
    size_t chunk_capacity = pool->per_chunk_capacity ? pool->per_chunk_capacity : STATE_MIN_CHUNK_CAPACITY;
    chunk->capacity = chunk_capacity;
    chunk->count = 0;
    chunk->data = (unsigned char *)calloc(chunk_capacity, component_size);
    chunk->keys = (char **)calloc(chunk_capacity, sizeof(char *));
    if (!chunk->data || !chunk->keys) {
        state_chunk_dispose(chunk);
        return 0;
    }

    pool->chunk_count += 1;
    return 1;
}

static int state_component_pool_reserve(StateComponentPool *pool, size_t component_size) {
    if (pool->chunk_count == 0) {
        return state_component_pool_grow(pool, component_size);
    }

    StateChunk *chunk = &pool->chunks[pool->chunk_count - 1];
    if (chunk->count >= chunk->capacity) {
        return state_component_pool_grow(pool, component_size);
    }
    return 1;
}

static void *state_component_pool_append(StateComponentPool *pool, size_t component_size, const char *key) {
    if (!state_component_pool_reserve(pool, component_size)) {
        return NULL;
    }

    StateChunk *chunk = &pool->chunks[pool->chunk_count - 1];
    size_t offset = chunk->count * component_size;
    void *dst = chunk->data + offset;
    chunk->keys[chunk->count] = state_strdup(key);
    if (!chunk->keys[chunk->count]) {
        return NULL;
    }
    chunk->count += 1;
    return dst;
}

static void *state_component_pool_find(StateComponentPool *pool, size_t component_size, const char *key) {
    for (size_t c = 0; c < pool->chunk_count; ++c) {
        StateChunk *chunk = &pool->chunks[c];
        for (size_t i = 0; i < chunk->count; ++i) {
            if (chunk->keys[i] && strcmp(chunk->keys[i], key) == 0) {
                return chunk->data + (i * component_size);
            }
        }
    }
    return NULL;
}

static int state_component_pool_remove(StateComponentPool *pool, size_t component_size, const char *key, StateEvent *evt) {
    for (size_t c = 0; c < pool->chunk_count; ++c) {
        StateChunk *chunk = &pool->chunks[c];
        for (size_t i = 0; i < chunk->count; ++i) {
            if (chunk->keys[i] && strcmp(chunk->keys[i], key) == 0) {
                if (evt && evt->payload && evt->payload_size >= component_size) {
                    memcpy(evt->payload, chunk->data + (i * component_size), component_size);
                }
                free(chunk->keys[i]);
                if (i + 1 < chunk->count) {
                    size_t move_count = chunk->count - i - 1;
                    memmove(chunk->keys + i, chunk->keys + i + 1, move_count * sizeof(char *));
                    memmove(chunk->data + (i * component_size), chunk->data + ((i + 1) * component_size), move_count * component_size);
                }
                chunk->count -= 1;
                return 1;
            }
        }
    }
    return 0;
}

static void state_event_clear(StateEvent *event) {
    if (!event) {
        return;
    }
    free(event->key);
    free(event->type_name);
    free(event->payload);
    event->key = NULL;
    event->type_name = NULL;
    event->payload = NULL;
    event->payload_size = 0;
}

static int state_event_copy(StateEvent *dst, const StateEvent *src) {
    *dst = *src;
    dst->key = state_strdup(src->key);
    dst->type_name = state_strdup(src->type_name);
    dst->payload = NULL;
    if (src->payload && src->payload_size > 0) {
        dst->payload = malloc(src->payload_size);
        if (!dst->payload) {
            free(dst->key);
            free(dst->type_name);
            dst->key = NULL;
            dst->type_name = NULL;
            return 0;
        }
        memcpy(dst->payload, src->payload, src->payload_size);
    }
    return 1;
}

static int state_event_queue_init(StateEventQueue *queue, size_t capacity) {
    queue->capacity = capacity ? capacity : STATE_MIN_CHUNK_CAPACITY;
    queue->events = (StateEvent *)calloc(queue->capacity, sizeof(StateEvent));
    queue->count = 0;
    queue->head = 0;
    queue->tail = 0;
    if (!queue->events) {
        return 0;
    }
    if (mtx_init(&queue->mutex, mtx_plain) != thrd_success) {
        free(queue->events);
        queue->events = NULL;
        return 0;
    }
    if (cnd_init(&queue->cond) != thrd_success) {
        mtx_destroy(&queue->mutex);
        free(queue->events);
        queue->events = NULL;
        return 0;
    }
    return 1;
}

static void state_event_queue_dispose(StateEventQueue *queue) {
    if (!queue) {
        return;
    }
    mtx_lock(&queue->mutex);
    for (size_t i = 0; i < queue->count; ++i) {
        size_t idx = (queue->head + i) % queue->capacity;
        state_event_clear(&queue->events[idx]);
    }
    free(queue->events);
    queue->events = NULL;
    queue->capacity = 0;
    queue->count = 0;
    queue->head = 0;
    queue->tail = 0;
    mtx_unlock(&queue->mutex);
    cnd_destroy(&queue->cond);
    mtx_destroy(&queue->mutex);
}

static int state_event_queue_push(StateEventQueue *queue, const StateEvent *event) {
    int result = 0;
    mtx_lock(&queue->mutex);
    if (queue->count == queue->capacity) {
        size_t new_capacity = queue->capacity * 2;
        StateEvent *new_events = (StateEvent *)calloc(new_capacity, sizeof(StateEvent));
        if (!new_events) {
            mtx_unlock(&queue->mutex);
            return 0;
        }
        for (size_t i = 0; i < queue->count; ++i) {
            size_t idx = (queue->head + i) % queue->capacity;
            if (!state_event_copy(&new_events[i], &queue->events[idx])) {
                for (size_t j = 0; j < i; ++j) {
                    state_event_clear(&new_events[j]);
                }
                free(new_events);
                mtx_unlock(&queue->mutex);
                return 0;
            }
            state_event_clear(&queue->events[idx]);
        }
        free(queue->events);
        queue->events = new_events;
        queue->capacity = new_capacity;
        queue->head = 0;
        queue->tail = queue->count;
    }

    size_t idx = queue->tail;
    result = state_event_copy(&queue->events[idx], event);
    if (result) {
        queue->tail = (queue->tail + 1) % queue->capacity;
        queue->count += 1;
        cnd_signal(&queue->cond);
    }
    mtx_unlock(&queue->mutex);
    return result;
}

static int state_event_queue_pop(StateEventQueue *queue, StateEvent *out, int wait_for_event) {
    int got_event = 0;
    mtx_lock(&queue->mutex);
    while (queue->count == 0 && wait_for_event) {
        cnd_wait(&queue->cond, &queue->mutex);
    }

    if (queue->count > 0) {
        if (!state_event_copy(out, &queue->events[queue->head])) {
            mtx_unlock(&queue->mutex);
            return 0;
        }
        state_event_clear(&queue->events[queue->head]);
        queue->head = (queue->head + 1) % queue->capacity;
        queue->count -= 1;
        got_event = 1;
    }

    mtx_unlock(&queue->mutex);
    return got_event;
}

StateManagerResult state_manager_init(StateManager *manager, size_t initial_types, size_t initial_queue_capacity) {
    if (!manager) {
        return STATE_MANAGER_ERROR_INVALID_ARGUMENT;
    }

    manager->pool_capacity = initial_types ? initial_types : STATE_MIN_POOL_CAPACITY;
    manager->pools = (StateComponentPool *)calloc(manager->pool_capacity, sizeof(StateComponentPool));
    manager->pool_count = 0;

    if (!manager->pools) {
        fprintf(stderr, "state_manager_init: failed to allocate %zu pools\n", manager->pool_capacity);
        return STATE_MANAGER_ERROR_ALLOCATION_FAILED;
    }

    manager->subscriber_capacity = STATE_MIN_SUBSCRIBER_CAPACITY;
    manager->subscribers = (StateSubscriber *)calloc(manager->subscriber_capacity, sizeof(StateSubscriber));
    manager->subscriber_count = 0;

    if (!manager->subscribers) {
        fprintf(stderr, "state_manager_init: failed to allocate %zu subscribers\n", manager->subscriber_capacity);
        free(manager->pools);
        manager->pools = NULL;
        manager->pool_capacity = 0;
        manager->pool_count = 0;
        return STATE_MANAGER_ERROR_ALLOCATION_FAILED;
    }

    if (!state_event_queue_init(&manager->event_queue, initial_queue_capacity)) {
        fprintf(stderr, "state_manager_init: failed to initialize event queue\n");
        free(manager->subscribers);
        free(manager->pools);
        manager->subscribers = NULL;
        manager->pools = NULL;
        manager->subscriber_capacity = 0;
        manager->pool_capacity = 0;
        manager->subscriber_count = 0;
        manager->pool_count = 0;
        memset(&manager->event_queue, 0, sizeof(StateEventQueue));
        return STATE_MANAGER_ERROR_ALLOCATION_FAILED;
    }

    return STATE_MANAGER_OK;
}

void state_manager_dispose(StateManager *manager) {
    if (!manager) {
        return;
    }
    for (size_t i = 0; i < manager->pool_count; ++i) {
        state_component_pool_dispose(&manager->pools[i]);
    }
    free(manager->pools);
    manager->pools = NULL;
    manager->pool_count = 0;
    manager->pool_capacity = 0;

    for (size_t i = 0; i < manager->subscriber_count; ++i) {
        free(manager->subscribers[i].key);
        manager->subscribers[i].key = NULL;
    }
    free(manager->subscribers);
    manager->subscribers = NULL;
    manager->subscriber_capacity = 0;
    manager->subscriber_count = 0;

    state_event_queue_dispose(&manager->event_queue);
}

StateManagerResult state_manager_register_type(StateManager *manager, const char *name, size_t component_size,
                                               size_t chunk_capacity, int *out_type_id) {
    if (!manager || !out_type_id || component_size == 0) {
        fprintf(stderr, "state_manager_register_type: invalid arguments\n");
        return STATE_MANAGER_ERROR_INVALID_ARGUMENT;
    }

    if (manager->pool_count == manager->pool_capacity) {
        size_t new_capacity = manager->pool_capacity * 2;
        StateComponentPool *new_pools =
            (StateComponentPool *)realloc(manager->pools, new_capacity * sizeof(StateComponentPool));
        if (!new_pools) {
            fprintf(stderr, "state_manager_register_type: failed to grow pool array to %zu entries\n", new_capacity);
            return STATE_MANAGER_ERROR_ALLOCATION_FAILED;
        }
        for (size_t i = manager->pool_capacity; i < new_capacity; ++i) {
            memset(&new_pools[i], 0, sizeof(StateComponentPool));
        }
        manager->pools = new_pools;
        manager->pool_capacity = new_capacity;
    }

    char *name_copy = state_strdup(name);
    if (!name_copy) {
        fprintf(stderr, "state_manager_register_type: failed to duplicate name '%s'\n", name ? name : "(null)");
        return STATE_MANAGER_ERROR_ALLOCATION_FAILED;
    }

    StateComponentPool *pool = &manager->pools[manager->pool_count];
    pool->name = name_copy;
    pool->component_size = component_size;
    pool->per_chunk_capacity = chunk_capacity ? chunk_capacity : STATE_MIN_CHUNK_CAPACITY;
    pool->chunks = NULL;
    pool->chunk_count = 0;
    pool->chunk_array_capacity = 0;

    *out_type_id = (int)manager->pool_count;
    manager->pool_count += 1;
    return STATE_MANAGER_OK;
}

void *state_manager_write(StateManager *manager, int type_id, const char *key, const void *data) {
    if (!manager || type_id < 0 || (size_t)type_id >= manager->pool_count || !key) {
        return NULL;
    }
    StateComponentPool *pool = &manager->pools[type_id];
    void *existing = state_component_pool_find(pool, pool->component_size, key);
    StateEvent event;
    event.kind = existing ? STATE_EVENT_COMPONENT_UPDATED : STATE_EVENT_COMPONENT_ADDED;
    event.type_id = type_id;
    event.payload_size = pool->component_size;
    event.payload = malloc(pool->component_size);
    event.key = state_strdup(key);
    event.type_name = state_strdup(pool->name);
    if (!event.payload || !event.key || !event.type_name) {
        state_event_clear(&event);
        return NULL;
    }

    void *target = existing;
    if (!target) {
        target = state_component_pool_append(pool, pool->component_size, key);
    }
    if (!target) {
        state_event_clear(&event);
        return NULL;
    }

    memcpy(target, data, pool->component_size);
    memcpy(event.payload, data, pool->component_size);
    state_event_queue_push(&manager->event_queue, &event);
    state_event_clear(&event);
    return target;
}

void *state_manager_get(StateManager *manager, int type_id, const char *key) {
    if (!manager || type_id < 0 || (size_t)type_id >= manager->pool_count || !key) {
        return NULL;
    }
    StateComponentPool *pool = &manager->pools[type_id];
    return state_component_pool_find(pool, pool->component_size, key);
}

int state_manager_remove(StateManager *manager, int type_id, const char *key) {
    if (!manager || type_id < 0 || (size_t)type_id >= manager->pool_count || !key) {
        return 0;
    }
    StateComponentPool *pool = &manager->pools[type_id];

    StateEvent event;
    event.kind = STATE_EVENT_COMPONENT_REMOVED;
    event.type_id = type_id;
    event.type_name = state_strdup(pool->name);
    event.key = state_strdup(key);
    event.payload_size = pool->component_size;
    event.payload = malloc(pool->component_size);
    if (!event.type_name || !event.key || !event.payload) {
        state_event_clear(&event);
        return 0;
    }

    int removed = state_component_pool_remove(pool, pool->component_size, key, &event);
    if (removed) {
        state_event_queue_push(&manager->event_queue, &event);
    }
    state_event_clear(&event);
    return removed;
}

int state_manager_subscribe(StateManager *manager, int type_id, const char *key, StateEventHandler handler, void *user_data) {
    if (!manager || !handler) {
        return 0;
    }

    if (manager->subscriber_count == manager->subscriber_capacity) {
        size_t new_capacity = manager->subscriber_capacity * 2;
        StateSubscriber *new_subscribers = (StateSubscriber *)realloc(manager->subscribers, new_capacity * sizeof(StateSubscriber));
        if (!new_subscribers) {
            return 0;
        }
        for (size_t i = manager->subscriber_capacity; i < new_capacity; ++i) {
            memset(&new_subscribers[i], 0, sizeof(StateSubscriber));
        }
        manager->subscribers = new_subscribers;
        manager->subscriber_capacity = new_capacity;
    }

    StateSubscriber *subscriber = &manager->subscribers[manager->subscriber_count];
    subscriber->type_id = type_id;
    subscriber->key = state_strdup(key);
    subscriber->handler = handler;
    subscriber->user_data = user_data;

    if (key && !subscriber->key) {
        return 0;
    }

    manager->subscriber_count += 1;
    return 1;
}

void state_manager_dispatch(StateManager *manager, int wait_for_event) {
    if (!manager) {
        return;
    }

    StateEvent event;
    while (state_event_queue_pop(&manager->event_queue, &event, wait_for_event)) {
        for (size_t i = 0; i < manager->subscriber_count; ++i) {
            StateSubscriber *subscriber = &manager->subscribers[i];
            if (subscriber->type_id != event.type_id) {
                continue;
            }
            if (subscriber->key && event.key && strcmp(subscriber->key, event.key) != 0) {
                continue;
            }
            subscriber->handler(&event, subscriber->user_data);
        }
        state_event_dispose(&event);
        wait_for_event = 0; // only block for the first iteration when requested
    }
}

int state_manager_publish(StateManager *manager, StateEventKind kind, int type_id, const char *key, const void *payload, size_t payload_size) {
    if (!manager) {
        return 0;
    }

    StateEvent event;
    event.kind = kind;
    event.type_id = type_id;
    event.key = state_strdup(key);
    event.payload_size = payload_size;
    event.payload = NULL;
    event.type_name = NULL;

    if (type_id >= 0 && (size_t)type_id < manager->pool_count) {
        event.type_name = state_strdup(manager->pools[type_id].name);
    }

    if (payload_size > 0 && payload) {
        event.payload = malloc(payload_size);
        if (!event.payload) {
            state_event_clear(&event);
            return 0;
        }
        memcpy(event.payload, payload, payload_size);
    }

    int pushed = state_event_queue_push(&manager->event_queue, &event);
    state_event_clear(&event);
    return pushed;
}

void state_event_dispose(StateEvent *event) {
    state_event_clear(event);
}

