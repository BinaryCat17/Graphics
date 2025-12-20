#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L // NOLINT(bugprone-reserved-identifier)
#endif

#include "thread.h"
#include <stdlib.h>
#include <stdint.h>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <process.h>

    struct Mutex {
        SRWLOCK handle;
    };

    struct Thread {
        HANDLE handle;
        unsigned int id;
        int exit_code; // To store result if needed, though GetExitCodeThread works too
    };

    Mutex* mutex_create(void) {
        Mutex* m = (Mutex*)malloc(sizeof(Mutex));
        if (m) {
            InitializeSRWLock(&m->handle);
        }
        return m;
    }

    void mutex_destroy(Mutex* mutex) {
        if (mutex) {
            // SRW locks don't need explicit destruction in Win32
            free(mutex);
        }
    }

    void mutex_lock(Mutex* mutex) {
        if (mutex) {
            AcquireSRWLockExclusive(&mutex->handle);
        }
    }

    void mutex_unlock(Mutex* mutex) {
        if (mutex) {
            ReleaseSRWLockExclusive(&mutex->handle);
        }
    }

    // Thread wrapper to match signature
    typedef struct {
        ThreadFunction func;
        void* arg;
        Thread* thread_struct; // Back pointer if needed? Not strictly for now.
    } ThreadStartInfo;

    static unsigned __stdcall win32_thread_entry(void* arg) {
        ThreadStartInfo* info = (ThreadStartInfo*)arg;
        int result = info->func(info->arg);
        free(info);
        return (unsigned)result;
    }

    Thread* thread_create(ThreadFunction func, void* arg) {
        Thread* t = (Thread*)malloc(sizeof(Thread));
        if (!t) return NULL;

        ThreadStartInfo* info = (ThreadStartInfo*)malloc(sizeof(ThreadStartInfo));
        if (!info) {
            free(t);
            return NULL;
        }
        info->func = func;
        info->arg = arg;
        
        uintptr_t handle = _beginthreadex(NULL, 0, win32_thread_entry, info, 0, &t->id);
        if (handle == 0) {
            free(info);
            free(t);
            return NULL;
        }

        t->handle = (HANDLE)handle;
        return t;
    }

    void thread_detach(Thread* thread) {
        if (thread) {
            CloseHandle(thread->handle);
            free(thread);
        }
    }

    int thread_join(Thread* thread) {
        if (!thread) return 0;
        
        WaitForSingleObject(thread->handle, INFINITE);
        
        DWORD exit_code = 0;
        GetExitCodeThread(thread->handle, &exit_code);
        
        CloseHandle(thread->handle);
        free(thread);
        
        return (int)exit_code;
    }

    void thread_sleep(unsigned int milliseconds) {
        Sleep(milliseconds);
    }

    unsigned int thread_hardware_concurrency(void) {
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        return sysinfo.dwNumberOfProcessors;
    }

#else
    #include <pthread.h>
    #include <unistd.h>
    #include <time.h>

    struct Mutex {
        pthread_mutex_t handle;
    };

    struct Thread {
        pthread_t handle;
    };

    // Pthread wrapper
    typedef struct {
        ThreadFunction func;
        void* arg;
    } ThreadStartInfo;

    static void* pthread_thread_entry(void* arg) {
        ThreadStartInfo* info = (ThreadStartInfo*)arg;
        ThreadFunction func = info->func;
        void* user_arg = info->arg;
        free(info);
        
        intptr_t result = func(user_arg);
        return (void*)result; // NOLINT(performance-no-int-to-ptr)
    }

    Mutex* mutex_create(void) {
        Mutex* m = (Mutex*)malloc(sizeof(Mutex));
        if (m) {
            pthread_mutex_init(&m->handle, NULL);
        }
        return m;
    }

    void mutex_destroy(Mutex* mutex) {
        if (mutex) {
            pthread_mutex_destroy(&mutex->handle);
            free(mutex);
        }
    }

    void mutex_lock(Mutex* mutex) {
        if (mutex) {
            pthread_mutex_lock(&mutex->handle);
        }
    }

    void mutex_unlock(Mutex* mutex) {
        if (mutex) {
            pthread_mutex_unlock(&mutex->handle);
        }
    }

    Thread* thread_create(ThreadFunction func, void* arg) {
        Thread* t = (Thread*)malloc(sizeof(Thread));
        if (!t) return NULL;

        ThreadStartInfo* info = (ThreadStartInfo*)malloc(sizeof(ThreadStartInfo));
        if (!info) {
            free(t);
            return NULL;
        }
        info->func = func;
        info->arg = arg;

        if (pthread_create(&t->handle, NULL, pthread_thread_entry, info) != 0) {
            free(info);
            free(t);
            return NULL;
        }

        return t;
    }

    void thread_detach(Thread* thread) {
        if (thread) {
            pthread_detach(thread->handle);
            free(thread);
        }
    }

    int thread_join(Thread* thread) {
        if (!thread) return 0;
        
        void* result = NULL;
        pthread_join(thread->handle, &result);
        
        free(thread);
        return (int)(intptr_t)result; // NOLINT(performance-no-int-to-ptr)
    }

    void thread_sleep(unsigned int milliseconds) {
        struct timespec ts;
        ts.tv_sec = milliseconds / 1000;
        ts.tv_nsec = (long)(milliseconds % 1000) * 1000000;
        nanosleep(&ts, NULL);
    }

    unsigned int thread_hardware_concurrency(void) {
        long nprocs = -1;
        #ifdef _SC_NPROCESSORS_ONLN
            nprocs = sysconf(_SC_NPROCESSORS_ONLN);
        #endif
        if (nprocs < 1) return 1;
        return (unsigned int)nprocs;
    }

#endif

