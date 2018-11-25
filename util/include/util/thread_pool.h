#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace util::misc {

/// Manages a given number of threads and runs a task queue.
/// Simpler version of my other implementation: https://github.com/andrmr/cpp_thread_pool
class ThreadPool
{
public:
    using Ptr  = std::shared_ptr<ThreadPool>; //!< Alias for passing around ThreadPool pointers.
    using Task = std::function<void()>;       //!< Type of object queued and processed by the pool.

    /// Constructs a thread pool, with hardware_concurrency() as default number of threads.
    /// @param max_threads - number of threads
    explicit ThreadPool(unsigned int max_tasks, unsigned int max_threads = std::thread::hardware_concurrency()) noexcept;

    ~ThreadPool() noexcept;

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool* operator=(const ThreadPool&) = delete;

    // TODO: implement move
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool* operator=(ThreadPool&&) = delete;

    /// Queues a task if the queue is not full. Otherwise, blocks until the queue has slots.
    /// @param task - object to be queued and processed
    void try_add_task(Task task) noexcept;

    /// Stops the thread pool.
    void stop() noexcept;

private:
    class Queue
    {
        std::queue<Task> m_tasks {};
        mutable std::mutex m_mutex {};
        std::mutex m_condition_mutex {};
        std::condition_variable m_condition {};
        std::atomic_bool m_continue {true};
        unsigned int m_max_tasks {0};

    public:
        ~Queue() noexcept;

        Queue(unsigned int max_tasks) noexcept;

        Queue(const Queue&) = delete;
        Queue& operator=(const Queue&) = delete;

        // TODO: implement move
        Queue(Queue&&) = delete;
        Queue& operator=(Queue&&) = delete;


        void enqueue(Task task) noexcept;
        bool empty() const noexcept;
        bool full() const noexcept;
        void stop() noexcept;
        void run() noexcept;
    };

    Queue m_queue;
    std::vector<std::thread> m_threads;
    unsigned int m_max_threads {0};
};

} // namespace util::misc
