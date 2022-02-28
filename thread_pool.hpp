#pragma once

#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <queue>

namespace details {

struct NonCopyable
{
    NonCopyable() = default;
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator = (const NonCopyable&) = delete;
};

struct NonMoveable
{
    NonMoveable() = default;
    NonMoveable(NonMoveable&&) = delete;
    NonMoveable& operator = (NonMoveable&&) = delete;
};

} // namespace details

namespace thread_pool {

class ThreadPool : details::NonCopyable, details::NonMoveable
{
public:
    explicit ThreadPool(size_t threads_count = std::thread::hardware_concurrency())
        : m_threads_count(threads_count ? threads_count : std::thread::hardware_concurrency()),
          m_threads(threads_count ? threads_count : std::thread::hardware_concurrency())
    {
        create_threads();
    }

    ~ThreadPool()
    {
        wait_for_tasks();
        m_running = false;
        join_threads();
    }

    void set_paused(bool paused)
    {
    	m_paused = paused;
    }

    size_t get_tasks_queued_count() const
    {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        return m_tasks.size();
    }

    size_t get_tasks_running_count() const
    {
        return m_tasks_total_count - get_tasks_queued_count();
    }

    size_t get_tasks_total_count() const
    {
        return m_tasks_total_count;
    }

    size_t get_threads_count() const
    {
        return m_threads_count;
    }

    template<typename F>
    void add_task(const F& task)
    {
        ++m_tasks_total_count;
        {
            std::lock_guard<std::mutex> lock(m_queue_mutex);
            m_tasks.push(std::function<void()>(task));
        }
    }

    template<typename F, typename... T>
    void add_task(const F& task, T&&... args)
    {
        add_task(
           [task, args...]
           {
               task(args...);
           }
        );
    }

    void wait_for_tasks()
    {
        while (true) {
            if (!m_paused) {
                if (m_tasks_total_count == 0) {
                    break;
                }
            } else {
                if (get_tasks_running_count() == 0) {
                    break;
                }
            }
            std::this_thread::yield();
        }
    }

    void reset(size_t threads_count = std::thread::hardware_concurrency())
    {
        const bool was_paused = m_paused;
        m_paused = true;
        wait_for_tasks();
        m_running = false;
        join_threads();

        m_threads_count = threads_count ? threads_count : std::thread::hardware_concurrency();
        m_threads = std::vector<std::thread>(threads_count);
        m_paused = was_paused;
        m_running = true;

        create_threads();
    }

private:
    void create_threads()
    {
        for (size_t i = 0u; i < m_threads_count; ++i) {
            m_threads[i] = std::thread(&ThreadPool::worker, this);
        }
    }

    void join_threads()
    {
        for (size_t i = 0u; i < m_threads_count; ++i) {
            if (m_threads[i].joinable()) {
                m_threads[i].join();
            }
        }
    }

    bool pop_task(std::function<void()>& task)
    {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        if (!m_tasks.empty()) {
            task = std::move(m_tasks.front());
            m_tasks.pop();
            return true;
        } else {
            return false;
        }
    }

    void worker()
    {
        while (m_running) {
            std::function<void()> task;
            if (!m_paused && pop_task(task)) {
                task();
                --m_tasks_total_count;
            } else {
                std::this_thread::yield();
            }
        }
    }

private:
	std::queue<std::function<void()>> m_tasks;
	std::vector<std::thread> m_threads;
	mutable std::mutex m_queue_mutex;

	size_t m_threads_count;

	std::atomic<size_t> m_tasks_total_count = 0u;
	std::atomic<bool> m_running = true;
	std::atomic<bool> m_paused = false;
};

} // namespace thread_pool