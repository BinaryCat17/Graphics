#ifndef THREAD_H
#define THREAD_H

#include <stdbool.h>

// Opaque handles
typedef struct Mutex Mutex;
typedef struct Thread Thread;

// --- Mutex ---
/**
 * @brief Creates a new mutex.
 * @return Pointer to the mutex, or NULL on failure.
 */
Mutex* mutex_create(void);

/**
 * @brief Destroys a mutex.
 * @param mutex The mutex to destroy.
 */
void mutex_destroy(Mutex* mutex);

/**
 * @brief Locks the mutex. Blocks until the lock is acquired.
 * @param mutex The mutex to lock.
 */
void mutex_lock(Mutex* mutex);

/**
 * @brief Unlocks the mutex.
 * @param mutex The mutex to unlock.
 */
void mutex_unlock(Mutex* mutex);

// --- Thread ---
typedef int (*ThreadFunction)(void* arg);

/**
 * @brief Creates and starts a new thread.
 * @param func The function to execute.
 * @param arg Argument to pass to the function.
 * @return Pointer to the thread, or NULL on failure.
 */
Thread* thread_create(ThreadFunction func, void* arg);

/**
 * @brief Detaches the thread, allowing it to run independently. 
 * Resources are released automatically when it exits.
 * @param thread The thread to detach.
 */
void thread_detach(Thread* thread);

/**
 * @brief Joins the thread, blocking until it exits.
 * @param thread The thread to join.
 * @return The exit code of the thread function.
 */
int thread_join(Thread* thread);

/**
 * @brief Sleeps the current thread.
 * @param milliseconds Time to sleep in milliseconds.
 */
void thread_sleep(unsigned int milliseconds);

/**
 * @brief Returns the number of concurrent threads supported by hardware.
 * @return Number of threads (e.g., cores).
 */
unsigned int thread_hardware_concurrency(void);

#endif // THREAD_H
