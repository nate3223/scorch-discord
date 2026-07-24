#include "AgentsManager.hpp"
#include "AgentsManager_p.hpp"

#include "Log.hpp"

#include <format>

AgentsManager::AgentsManager()
	: m_p(std::make_unique<AgentsManagerPrivate>())
{

}

AgentsManager::~AgentsManager() = default;

void AgentsManager::listen(std::string port)
{
	asio::post(
		m_p->m_ioContext,
		[this, port = std::move(port)]() mutable {
			m_p->listen(std::move(port));
		}
	);
}

AgentsManagerPrivate::AgentsManagerPrivate()
	: m_logger(Logger::Agents())
	, m_sslContext(asio::ssl::context::tls_server)
	, m_workGuard(asio::make_work_guard(m_ioContext))
	, m_listenSignal(m_ioContext, asio::steady_timer::clock_type::time_point::max())
	, m_ioThread([this](std::stop_token) {
		asio::co_spawn(m_ioContext, run(), asio::detached);
		m_ioContext.run();
	})
{
}

AgentsManagerPrivate::~AgentsManagerPrivate()
{
	m_ioContext.stop();
}

asio::awaitable<void> AgentsManagerPrivate::run()
{
	boost::system::error_code ec;

	co_await m_listenSignal.async_wait(
		asio::redirect_error(asio::use_awaitable, ec)
	);

	if (ec != asio::error::operation_aborted)
	{
		co_return;
	}

	m_logger.info(std::format("Listening on port {}...", m_port));

	co_return;
}

void AgentsManagerPrivate::listen(std::string port)
{
	asio::post(
		m_ioContext,
		[this, port = std::move(port)]() mutable {
			m_port = std::move(port);
			m_listenSignal.cancel();
		}
	);
}
