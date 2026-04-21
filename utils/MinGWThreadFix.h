#ifndef MINGW_THREAD_FIX_H
#define MINGW_THREAD_FIX_H

#if defined(__MINGW32__) && !defined(_GLIBCXX_HAS_GTHREADS)

#include <windows.h>
#include <vector>
#include <functional>
#include <chrono>

namespace std {

    // A simple mutex implementation using Win32 Critical Sections
    class mutex {
    public:
        mutex() { InitializeCriticalSection(&cs_); }
        ~mutex() { DeleteCriticalSection(&cs_); }
        void lock() { EnterCriticalSection(&cs_); }
        void unlock() { LeaveCriticalSection(&cs_); }
        bool try_lock() { return TryEnterCriticalSection(&cs_); }

        // Non-copyable
        mutex(const mutex&) = delete;
        mutex& operator=(const mutex&) = delete;

    private:
        CRITICAL_SECTION cs_;
    };

    // Simple lock_guard implementation if not already defined
#if __cplusplus < 201103L || !defined(_GLIBCXX_HAS_GTHREADS)
    // Note: Some MinGW versions define lock_guard even if they don't have mutex.
    // We try to detect if it's already there by including <mutex> before this.
#endif
    // If you get a redefinition error, comment out this block.
    /*
    template <typename Mutex>
    class lock_guard {
    public:
        explicit lock_guard(Mutex& m) : m_(m) { m_.lock(); }
        ~lock_guard() { m_.unlock(); }

        lock_guard(const lock_guard&) = delete;
        lock_guard& operator=(const lock_guard&) = delete;

    private:
        Mutex& m_;
    };
    */

    // A very basic thread wrapper for Win32 threads
    class thread {
    public:
        thread() : handle_(NULL), id_(0) {}
        
        template <typename Callable, typename... Args>
        explicit thread(Callable&& f, Args&&... args) {
            auto task = new std::function<void()>(std::bind(std::forward<Callable>(f), std::forward<Args>(args)...));
            handle_ = CreateThread(NULL, 0, thread_proxy, task, 0, &id_);
        }

        ~thread() {
            if (handle_) CloseHandle(handle_);
        }

        void join() {
            if (handle_) {
                WaitForSingleObject(handle_, INFINITE);
                CloseHandle(handle_);
                handle_ = NULL;
            }
        }

        bool joinable() const { return handle_ != NULL; }

        thread(thread&& other) noexcept : handle_(other.handle_), id_(other.id_) {
            other.handle_ = NULL;
        }

        thread& operator=(thread&& other) noexcept {
            if (this != &other) {
                if (handle_) CloseHandle(handle_);
                handle_ = other.handle_;
                id_ = other.id_;
                other.handle_ = NULL;
            }
            return *this;
        }

    private:
        HANDLE handle_;
        DWORD id_;

        static DWORD WINAPI thread_proxy(LPVOID param) {
            auto task = static_cast<std::function<void()>*>(param);
            (*task)();
            delete task;
            return 0;
        }
    };

    namespace this_thread {
        inline void sleep_for(const std::chrono::milliseconds& ms) {
            Sleep((DWORD)ms.count());
        }
    }
}

#endif // __MINGW32__

#endif // MINGW_THREAD_FIX_H
