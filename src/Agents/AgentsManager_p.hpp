#pragma once

#include "IAgentIdentityStore.hpp"
#include "Log.hpp"
#include "PairingCodeManager.hpp"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/system/error_code.hpp>

#include <string>
#include <thread>

namespace asio = boost::asio;

using tcp = asio::ip::tcp;

class AgentsManagerPrivate
{
public:
			AgentsManagerPrivate(std::unique_ptr<IAgentIdentityStore> store);
			~AgentsManagerPrivate();

	void					listen(std::string port);
	asio::awaitable<void>	run();
	asio::awaitable<void>	acceptClient(tcp::socket&& client);

	using WorkGuard = asio::executor_work_guard<asio::io_context::executor_type>;

	std::unique_ptr<IAgentIdentityStore>	m_store;
	
	spdlog::logger&		m_logger;
	std::string			m_port;
	asio::io_context	m_ioContext;
	asio::ssl::context	m_sslContext;

	tcp::acceptor		m_acceptor;
	PairingCodeManager	m_pairingCodeManager;

	WorkGuard			m_workGuard;
	std::jthread		m_ioThread;
};
