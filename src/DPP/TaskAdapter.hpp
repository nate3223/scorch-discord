#pragma once

#include "dpp/coro/task.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>

#include <optional>

namespace dpp
{
	template <typename ReturnValue, typename Executor = boost::asio::any_io_executor>
	struct Awaiter
	{
		boost::asio::awaitable<ReturnValue, Executor>	awaitable;
		Executor										executor;
		std::optional<ReturnValue>						result;
		std::exception_ptr								exception;

		bool await_ready() const noexcept { return false; }

		void await_suspend(std::coroutine_handle<> handle)
		{
			boost::asio::co_spawn(
				executor,
				std::move(awaitable),
				[this, handle](std::exception_ptr ep, ReturnValue res) mutable
				{
					this->exception = ep;
					if (! ep)
						this->result.emplace(std::move(res));
					
					handle.resume();
				}
			);
		}

		ReturnValue await_resume()
		{
			if (exception)
				std::rethrow_exception(exception);
			
			return std::move(*result);
		}
	};

	template <typename Executor>
	struct Awaiter<void, Executor>
	{
		boost::asio::awaitable<void, Executor> awaitable;
		std::exception_ptr exception;

		bool await_ready() const noexcept { return false; }

		void await_suspend(std::coroutine_handle<> handle)
		{
			auto exec = awaitable.get_executor();

			boost::asio::co_spawn(
				exec,
				std::move(awaitable),
				[this, handle](std::exception_ptr ep) mutable
				{
					this->exception = ep;
					handle.resume();
				}
			);
		}

		void await_resume()
		{
			if (exception)
				std::rethrow_exception(exception);
		}
	};

	template <typename ReturnValue, typename Executor>
	class TaskAdapter<boost::asio::awaitable<ReturnValue, Executor>>
	{
	public:
		static dpp::task<ReturnValue> Await(boost::asio::awaitable<ReturnValue, Executor> awaitable, Executor executor)
		{
			co_return co_await Awaiter<ReturnValue, Executor>{ std::move(awaitable), std::move(executor) };
		}
	};
}
