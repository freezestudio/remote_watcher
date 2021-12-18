#ifndef SERVICE_ASYNC_H
#define SERVICE_ASYNC_H

#include <coroutine>
#include <stdexcept>
#include <concepts>
#include <cassert>

#define DECLARE_FUNCTION
#define CO_RETURN(type) type
#define CO_AWAIT(type) type

namespace freeze
{
    template <typename T>
    concept Awaitor = requires(T t)
    {
        {
            t.await_ready()
            } -> std::convertible_to<bool>;
        t.await_suspend(std::coroutine_handle<void>{});
        t.await_resume();
    };

    template <typename T>
    concept Awaitable =
        requires(T t)
    {
        {
            t.operator co_await()
            } -> Awaitor;
    }
    || requires(T t)
    {
        {
            operator co_await(t)
            } -> Awaitor;
    }
    || Awaitor<T>;
}

namespace freeze
{
	template<Awaitable T>
	struct awaitor
	{
		T awaitable;
		awaitor(T awaitable) noexcept
			: awaitable{ awaitable }
		{

		}

		bool await_ready() noexcept
		{
			return awaitable.await_ready();
		}

		auto await_suspend(std::coroutine_handle<> h) noexcept
		{
			return awaitable.await_suspend(h);
		}

		auto await_resume() noexcept
		{
			return awaitable.await_resume();
		}
	};

	template<Awaitable T>
	auto operator co_await(T&& awaitable)
	{
		return static_cast<awaitor<T>&&>(awaitable);
	}
}

namespace freeze
{
	struct task_t_error : std::runtime_error
	{
		task_t_error()
			: std::runtime_error("watcher task error")
		{
		}
	};

	template<typename = void>
	struct task_t;

	class task_promise_base;

	template<typename T>
	class task_promise_t;
}

namespace freeze
{
	struct suspend_promise
	{
		bool await_ready() const noexcept
		{
			return false;
		}

		template<typename Promise>
			requires std::same_as<std::coroutine_handle<>, decltype(std::declval<Promise>().coro_handle)>
		std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> coro_handle) noexcept
		{
			auto previous = coro_handle.promise().coro_handle;
			if (previous)
			{
				return previous;
			}
			return std::noop_coroutine();
		}

		void await_resume() const noexcept
		{
			// empty.
		}
	};

	class task_promise_base
	{
	public:
		task_promise_base(std::coroutine_handle<> handle = nullptr) noexcept
			: coro_handle{ handle }
		{

		}

	public:
		CO_AWAIT(auto) initial_suspend() const noexcept
		{
			return std::suspend_always{};
		}
		CO_AWAIT(auto) final_suspend() const noexcept
		{
			return suspend_promise{};
		}

	public:
		std::coroutine_handle<> coro_handle;
	};

	template<typename T>
	class task_promise_t final : public task_promise_base
	{
	public:
		task_promise_t(std::coroutine_handle<> handle = nullptr) noexcept
			: task_promise_base(handle)
			, value{ T{} }
		{

		}

		~task_promise_t() noexcept
		{
			switch (result_value_type)
			{
			case result_type::value:
				this->value.~T();
				break;
			case result_type::exception:
				this->exception_ptr.~exception_ptr();
				break;
			case result_type::empty:
				break;
			default: break;
			}
		}

	public:
		DECLARE_FUNCTION task_t<T> get_return_object() /*const*/ noexcept;
	public:
		template<typename V>
			requires std::convertible_to<V, T>
		CO_RETURN(void) return_value(V&& value) noexcept
		{
			::new(static_cast<void*>(std::addressof(this->value))) T(std::forward<V>(value));
			result_value_type = result_type::value;
		}

		void unhandled_exception() noexcept
		{
			::new(static_cast<void*>(std::addressof(this->exception_ptr))) std::exception_ptr(std::current_exception());
			result_value_type = result_type::exception;
		}
	public:
		decltype(auto) result() const
		{
			if (result_value_type == result_type::exception)
			{
				std::rethrow_exception(this->exception_ptr);
			}
			assert(result_value_type == result_type::value);
			return this->value;
		}
	private:
		enum class result_type : uint32_t
		{
			empty, value, exception,
		};
	private:
		result_type result_value_type = result_type::empty;
		union
		{
			T value;
			std::exception_ptr exception_ptr;
		};
	};

	template<>
	class task_promise_t<void> : public task_promise_base
	{
	public:
		task_promise_t(std::coroutine_handle<> handle = nullptr) noexcept
			: task_promise_base(handle)
		{

		}
	public:
		DECLARE_FUNCTION task_t<void> get_return_object() /*const*/ noexcept;
	public:
		CO_RETURN(void) return_void() noexcept
		{
		}
		void unhandled_exception() noexcept
		{
			exception_ptr = std::current_exception();
		}
	public:
		void result() const
		{
			if (exception_ptr)
			{
				std::rethrow_exception(exception_ptr);
			}
		}
	private:
		std::exception_ptr exception_ptr;
	};

	template<typename T>
	class task_promise_t<T&> : public task_promise_base
	{
	public:
		task_promise_t(std::coroutine_handle<> handle = nullptr) noexcept
			: task_promise_base(handle)
		{

		}
	public:
		DECLARE_FUNCTION task_t<T&> get_return_object() /*const*/ noexcept;
	public:
		CO_RETURN(void) return_value(T& value) noexcept
		{
			this->value = std::addressof(value);
		}
		void unhandled_exception() noexcept
		{
			exception_ptr = std::current_exception();
		}
	public:
		T& result() const
		{
			if (exception_ptr)
			{
				std::rethrow_exception(exception_ptr);
			}
			return *value;
		}
	private:
		T* value = nullptr;
		std::exception_ptr exception_ptr;
	};
}

namespace freeze
{
	template<typename T>
	struct [[nodiscard]] task_t
	{
		using promise_type = task_promise_t<T>;
		using void_handle_t = std::coroutine_handle<void>;
		using promise_handle_t = std::coroutine_handle<promise_type>;

		task_t(promise_handle_t handle = nullptr) noexcept
			: task_coro{ handle }
		{

		}

		~task_t() noexcept
		{
			if (task_coro)
			{
				task_coro.destroy();
			}
		}

		task_t(task_t&&) = delete;
		task_t& operator=(task_t&&) = delete;

		task_t(task_t const&) = default;
		task_t& operator=(task_t const&) = default;

		task_t& operator()() /*const*/ noexcept
		{
			if (task_coro)
			{
				task_coro.resume();
			}
			return *this;
		}

		bool await_ready() const noexcept
		{
			return false;
		}

		std::coroutine_handle<> await_suspend(std::coroutine_handle<> h) noexcept
		{
			if (!task_coro)
			{
				return std::noop_coroutine();
			}

			task_coro.promise().coro_handle = h;
			return task_coro;
		}

		decltype(auto) await_resume() const noexcept
		{
			return task_coro.promise().result();
		}
		promise_handle_t task_coro;
	};
}

namespace freeze
{
	template<typename T>
	task_t<T> task_promise_t<T>::get_return_object() /*const*/ noexcept
	{
		return task_t<T>{std::coroutine_handle<task_promise_t<T>>::from_promise(*this)};
	}

	inline task_t<void> task_promise_t<void>::get_return_object() /*const*/ noexcept
	{
		return task_t<void>{std::coroutine_handle<task_promise_t<void>>::from_promise(*this)};
	}

	template<typename T>
	task_t<T&> task_promise_t<T&>::get_return_object() /*const*/ noexcept
	{
		return task_t<T&>{std::coroutine_handle<task_promise_t<T&>>::from_promise(*this)};
	}
}

#endif
