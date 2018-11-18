#include "util/thread_pool.h"

using namespace util::misc;

ThreadPool::ThreadPool(unsigned int num_threads)
{
    for (size_t i = 0; i < num_threads; ++i)
    {
        m_threads.emplace_back(&Queue::run, &m_queue);
    }
}

ThreadPool::~ThreadPool()
{
    stop();
}

void ThreadPool::add_task(Task task)
{
    m_queue.enqueue(task);
}

void ThreadPool::stop()
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

void ThreadPool::Queue::enqueue(Task task)
{
    {
        std::lock_guard g {m_mutex};
        m_tasks.push(task);
    }

    m_condition.notify_one();
}

ThreadPool::Task ThreadPool::Queue::dequeue()
{
    std::lock_guard g {m_mutex};
    auto task = m_tasks.front();
    m_tasks.pop();

    return task;
}

bool ThreadPool::Queue::empty()
{
    std::lock_guard g {m_mutex};
    return m_tasks.empty();
}

void ThreadPool::Queue::stop()
{
    m_continue = false;
    m_condition.notify_all();
}

void ThreadPool::Queue::run()
{
    while (m_continue)
    {
        std::unique_lock g {m_condition_mutex};
        m_condition.wait(g, [this] { return !empty() || !m_continue; });
        g.unlock();

        if (!empty())
        {
            auto task = dequeue();
            task();
        }
    }
}
