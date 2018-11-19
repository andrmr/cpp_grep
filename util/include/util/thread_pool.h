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
    using Ptr  = std::shared_ptr<ThreadPool>; //!< Helper alias for passing around ThreadPool pointers.
    using Task = std::function<void()>;       //!< Type of object queued and processed by the pool.

    /// Constructs a thread pool, with hardware_concurrency() as default number of threads.
    /// @param num_threads - number of threads
    explicit ThreadPool(unsigned int num_threads = std::thread::hardware_concurrency());

    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool* operator=(const ThreadPool&) = delete;

    ThreadPool(ThreadPool&&) = delete;
    ThreadPool* operator=(ThreadPool&&) = delete;

    /// Queues a task.
    /// @param task - object to be queued and processed
    void add_task(Task task);

    /// Stops the thread pool.
    void stop();

private:
    class Queue
    {
        std::queue<Task> m_tasks {};
        std::mutex m_mutex {};
        std::mutex m_condition_mutex {};
        std::condition_variable m_condition {};
        std::atomic_bool m_continue {true};

    public:
        ~Queue();

        void enqueue(Task task);
        bool empty();
        void stop();
        void run();
    };

    Queue m_queue {};
    std::vector<std::thread> m_threads;
};

} // namespace util::misc
