#pragma once

#include "Log.hpp"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/system/error_code.hpp>

#include <string>
#include <thread>

namespace asio = boost::asio;

class AgentsManagerPrivate
{
public:
			AgentsManagerPrivate();
			~AgentsManagerPrivate();

	asio::awaitable<void>	run();
	void					listen(std::string port);

	using WorkGuard = asio::executor_work_guard<asio::io_context::executor_type>;

	spdlog::logger&		m_logger;
	std::string			m_port;
	asio::io_context	m_ioContext;
	asio::ssl::context	m_sslContext;
	WorkGuard			m_workGuard;
	asio::steady_timer	m_listenSignal;
	std::jthread		m_ioThread;
};
