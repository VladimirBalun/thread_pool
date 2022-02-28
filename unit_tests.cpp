#include <iostream>

#include "thread_pool.hpp"

namespace unit_tests {

namespace details {

void check(bool condition, std::string_view text, std::string_view func)
{
    if (condition) {
        std::cout << func << ": " << text << " -> PASSED!\n";
    } else {
        std::cerr << func << ": " << text << "-> FAILED!\n";
    }
}

} // namespace details

#define CHECK(condition, text) \
    details::check((condition), (text), __func__)

void check_thread_pool_initialization()
{
    thread_pool::ThreadPool thread_pool;
    CHECK(thread_pool.get_threads_count() == std::thread::hardware_concurrency(), "check threads count");
    CHECK(thread_pool.get_tasks_total_count() == 0u, "check total tasks count");
    CHECK(thread_pool.get_tasks_queued_count() == 0u, "check tasks queued count");
    CHECK(thread_pool.get_tasks_running_count() == 0u, "check running tasks count");
}

void check_thread_pool_reset()
{
    thread_pool::ThreadPool thread_pool;
    thread_pool.reset(std::thread::hardware_concurrency() / 2);
    CHECK(thread_pool.get_threads_count() == std::thread::hardware_concurrency() / 2, "check reset half count cores");
    thread_pool.reset(std::thread::hardware_concurrency());
    CHECK(thread_pool.get_threads_count() == std::thread::hardware_concurrency(), "check reset full count cores");
}

void check_thread_pool_add_tasks()
{
    thread_pool::ThreadPool thread_pool;
    {
        bool flag = false;
        thread_pool.add_task(
            [&flag]
            {
                flag = true;
            }
        );
        thread_pool.wait_for_tasks();
        CHECK(flag, "check add task without arguments");
    }
    {
        bool flag = false;
        thread_pool.add_task(
            [] (bool *flag)
            {
                *flag = true;
            },
            &flag
        );
        thread_pool.wait_for_tasks();
        CHECK(flag, "check add task with single argument");
    }
    {
        bool flag1 = false;
        bool flag2 = false;
        thread_pool.add_task(
            [] (bool *flag1, bool *flag2)
            {
                *flag1 = *flag2 = true;
            },
            &flag1,
            &flag2
        );
        thread_pool.wait_for_tasks();
        CHECK(flag1 && flag2, "check add task with double arguments");
    }
}

void check_thread_pool_wait_for_tasks()
{
    thread_pool::ThreadPool thread_pool;
    const size_t count_tasks = thread_pool.get_threads_count() * 10;
    std::vector<std::atomic<bool>> flags(count_tasks);

    for (size_t i = 0u; i < count_tasks; ++i) {
        thread_pool.add_task(
            [&flags, i]
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                flags[i] = true;
            }
        );
    }
    thread_pool.wait_for_tasks();

    bool all_flags = true;
    for (size_t i = 0u; i < count_tasks; ++i) {
        all_flags &= flags[i];
    }

    CHECK(all_flags, "check waiting for all tasks");
}

void check_thread_pool_pausing()
{
    const size_t count_tasks = std::min<size_t>(std::thread::hardware_concurrency(), 4u);
    thread_pool::ThreadPool thread_pool(count_tasks);
    thread_pool.set_paused(true);

    for (size_t i = 0u; i < count_tasks * 3u; ++i) {
        thread_pool.add_task(
            []
            {
               std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
        );
    }

    CHECK(thread_pool.get_tasks_total_count() == count_tasks * 3u &&
          thread_pool.get_tasks_running_count() == 0u &&
          thread_pool.get_tasks_queued_count() == count_tasks * 3u,
          "check count tasks in pause");

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    CHECK(thread_pool.get_tasks_total_count() == count_tasks * 3u &&
          thread_pool.get_tasks_running_count() == 0u &&
          thread_pool.get_tasks_queued_count() == count_tasks * 3u,
          "check count tasks in pause after some time");

    thread_pool.set_paused(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    CHECK(thread_pool.get_tasks_total_count() == count_tasks * 2u &&
          thread_pool.get_tasks_running_count() == count_tasks &&
          thread_pool.get_tasks_queued_count() == count_tasks,
          "check count tasks in non pause");

    thread_pool.set_paused(true);
    thread_pool.wait_for_tasks();
    CHECK(thread_pool.get_tasks_total_count() == count_tasks &&
          thread_pool.get_tasks_running_count() == 0u &&
          thread_pool.get_tasks_queued_count() == count_tasks,
          "check count tasks in pause after in non pause");

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    CHECK(thread_pool.get_tasks_total_count() == count_tasks &&
          thread_pool.get_tasks_running_count() == 0u &&
          thread_pool.get_tasks_queued_count() == count_tasks,
          "check count tasks in pause after in non pause after some time");

    thread_pool.set_paused(false);
    thread_pool.wait_for_tasks();
    CHECK(thread_pool.get_tasks_total_count() == 0u &&
          thread_pool.get_tasks_running_count() == 0u &&
          thread_pool.get_tasks_queued_count() == 0u,
          "check count wait for tasks in non pause");
}

} // namespace unit_tests

int main(int argc, char** argv)
{
    unit_tests::check_thread_pool_initialization();
    unit_tests::check_thread_pool_reset();
    unit_tests::check_thread_pool_add_tasks();
    unit_tests::check_thread_pool_wait_for_tasks();
    unit_tests::check_thread_pool_pausing();
    return EXIT_SUCCESS;
}