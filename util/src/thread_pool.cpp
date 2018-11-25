#include "util/thread_pool.h"

using namespace util::misc;

ThreadPool::ThreadPool(unsigned int max_tasks, unsigned int max_threads) noexcept
    : m_queue {max_tasks}, m_max_threads {max_threads}
{
    // NOTE: two options: start all threads here OR start threads gradually in add_task()

    //    for (size_t i = 0; i < num_threads; ++i)
    //    {
    //        m_threads.emplace_back(&Queue::run, &m_queue);
    //    }
}

ThreadPool::~ThreadPool() noexcept
{
    stop();
}

void ThreadPool::try_add_task(Task task) noexcept
{
    // add a new thread when one of these conditions is met
	// 1, pool is empty or 2, pool is not at full capacity AND the queue is saturated
    if (m_threads.empty() || (m_threads.size() < m_max_threads && m_queue.full()))
    {
        m_threads.emplace_back(&Queue::run, &m_queue);
    }

    m_queue.enqueue(task);
}

void ThreadPool::stop() noexcept
{
    m_queue.stop();
    for (auto& t: m_threads)
    {
        if (t.joinable())
        {
            t.join();
        }
    }
}

ThreadPool::Queue::~Queue() noexcept
{
    if (m_continue)
    {
        this->stop();
    }
}

ThreadPool::Queue::Queue(unsigned int max_tasks) noexcept
    : m_max_tasks {max_tasks}
{
}

void ThreadPool::Queue::enqueue(Task task) noexcept
{
    // doesn't really need sync with the current use case
    if (full())
    {
        std::unique_lock lk {m_condition_mutex};
        m_condition.wait(lk, [this] { return !full(); });
    }
    else
    {
        std::lock_guard g {m_mutex};
        m_tasks.push(task);
    }

    m_condition.notify_one();
}

bool ThreadPool::Queue::empty() const noexcept
{
    std::lock_guard g {m_mutex};
    return m_tasks.empty();
}

bool ThreadPool::Queue::full() const noexcept
{
    std::lock_guard g {m_mutex};
    return m_tasks.size() >= m_max_tasks;
}

void ThreadPool::Queue::stop() noexcept
{
    // block until queue is empty
    if (!empty() && m_continue)
    {
        std::unique_lock lk {m_condition_mutex};
        m_condition.wait(lk, [this] { return empty(); });
    }

    m_continue = false;
    m_condition.notify_all();
}

void ThreadPool::Queue::run() noexcept
{
    while (m_continue)
    {
        std::unique_lock lk {m_condition_mutex};
        m_condition.wait(lk, [this] { return !empty() || !m_continue; });
        lk.unlock();

        // not RAII but avoids std::optional/nullptr/default constructed
        // and doesn't lock during execution of task
        std::unique_lock g {m_mutex};
        if (!m_tasks.empty())
        {
            auto task = m_tasks.front();
            m_tasks.pop();
            g.unlock();

            m_condition.notify_all();
            task();
        }
    }
}
