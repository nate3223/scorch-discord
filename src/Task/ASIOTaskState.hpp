#pragma once

#include <scorch/server/Task.hpp>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/any_io_executor.hpp>

#include <memory>

template <typename T>
class AsioTaskState final
	: public scorch::server::TaskState<T>
	, public std::enable_shared_from_this<AsioTaskState<T>>
{
public:
	using TaskState = scorch::server::TaskState;

	explicit AsioTaskState(boost::asio::any_io_executor executor)
		: m_executor(std::move(executor))
	{
	}

	bool await_ready() const noexcept override
	{
		return m_done;
	}

	void await_suspend(std::coroutine_handle<> continuation) override
	{
		m_continuation = continuation;
	}

	T await_resume() override
	{
		if (m_exception)
			std::rethrow_exception(m_exception);

		return std::move(*m_result);
	}

	template <typename Function>
	void start(Function&& function)
	{
		auto self = this->shared_from_this();

		boost::asio::co_spawn(
			m_executor,
			std::forward<Function>(function),
			[self](std::exception_ptr exception, T result) mutable {
				self->complete(std::move(result), std::move(exception));
			}
		);
	}

private:
	void complete(T result, std::exception_ptr exception)
	{
		m_result = std::move(result);
		m_exception = std::move(exception);
		m_done = true;

		if (m_continuation)
			m_continuation.resume();
	}

private:
	boost::asio::any_io_executor	m_executor;
	std::optional<T>				m_result;
	std::exception_ptr				m_exception;
	std::coroutine_handle<>			m_continuation;
	bool							m_done = false;
};

template <>
class AsioTaskState<void> final
	: public TaskState<void>
	, public std::enable_shared_from_this<AsioTaskState<void>>
{
public:
	explicit AsioTaskState(boost::asio::any_io_executor executor)
		: m_executor(std::move(executor))
	{
	}

	bool await_ready() const noexcept override
	{
		return m_done;
	}

	void await_suspend(std::coroutine_handle<> continuation) override
	{
		m_continuation = continuation;
	}

	void await_resume() override
	{
		if (m_exception)
			std::rethrow_exception(m_exception);
	}

	template <typename Function>
	void start(Function&& function)
	{
		auto self = this->shared_from_this();

		boost::asio::co_spawn(
			m_executor,
			std::forward<Function>(function),
			[self](std::exception_ptr exception){
				self->complete(std::move(exception));
			}
		);
	}

private:
	void complete(std::exception_ptr exception)
	{
		m_exception = std::move(exception);
		m_done = true;

		if (m_continuation)
			m_continuation.resume();
	}

private:
	boost::asio::any_io_executor	m_executor;
	std::exception_ptr				m_exception;
	std::coroutine_handle<>			m_continuation;
	bool							m_done = false;
};
