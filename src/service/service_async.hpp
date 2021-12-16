#ifndef SERVICE_ASYNC_H
#define SERVICE_ASYNC_H

#include <coroutine>

namespace freeze
{
    struct watcher_task_error : std::runtime_error
    {
        watcher_task_error()
            : std::runtime_error("watcher task error")
        {
        }
    };

    struct watcher_task
    {
        using void_handle = std::coroutine_handle<>;
        struct promise_type
        {
            struct suspend_final
            {
                constexpr bool await_ready() const noexcept
                {
                    return false;
                }

                auto await_suspend(std::coroutine_handle<promise_type> h) const noexcept
                {
                    auto coro = static_cast<promise_type>(h.promise()).coro;
                    return coro ? coro : std::noop_coroutine();
                }

                constexpr void await_resume() const noexcept
                {
                }
            };
            watcher_task get_object_result() noexcept
            {
                return watcher_task{ std::coroutine_handle<promise_type>::from_promise(*this) };
            }
            auto initial_suspend() noexcept
            {
                return std::suspend_always{};
            }
            auto final_suspend() noexcept
            {
                return suspend_final{};
            }
            void unhandled_exception()
            {
                auto ptr = std::make_exception_ptr(watcher_task_error{});
                std::rethrow_exception(ptr);
            }
            void return_void() noexcept
            {

            }
            void_handle coro;
        };
        using this_handle = std::coroutine_handle<promise_type>;

        watcher_task()
            :task_coro{nullptr}
        {

        }

        watcher_task(this_handle handle)
            :task_coro{ handle }
        {

        }

        watcher_task(watcher_task&&) = delete;
        watcher_task& operator=(watcher_task&&) = delete;

        ~watcher_task()
        {
            if (!task_coro.done())
            {
                task_coro.resume();
            }
            task_coro.destroy();
        }

        void operator()()
        {
            task_coro.resume();
        }

        bool await_ready() noexcept
        {
            return false;
        }
        auto await_suspend(std::coroutine_handle<> h) noexcept
        {
            auto premise = static_cast<promise_type>(task_coro.promise());
            premise.coro = h;
            return task_coro;
        }
        void await_resume() noexcept
        {

        }

        this_handle task_coro;
    };
}

#endif
