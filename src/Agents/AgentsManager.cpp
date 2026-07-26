#include "AgentsManager.hpp"
#include "AgentsManager_p.hpp"

#include "AgentConnection.hpp"
#include "Log.hpp"

#include <format>

AgentsManager::AgentsManager(std::unique_ptr<IAgentIdentityStore> store)
	: m_p(std::make_unique<AgentsManagerPrivate>(std::move(store)))
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

AgentsManagerPrivate::AgentsManagerPrivate(std::unique_ptr<IAgentIdentityStore> store)
	: m_store(std::move(store))
	, m_logger(Logger::Agents())
	, m_sslContext(asio::ssl::context::sslv23_server)
	, m_acceptor(m_ioContext)
	, m_workGuard(asio::make_work_guard(m_ioContext))
	, m_ioThread([this](std::stop_token) {
		m_ioContext.run();
	})
{
	try
	{
		m_sslContext.set_options(
			asio::ssl::context::default_workarounds |
			asio::ssl::context::no_sslv2 |
			asio::ssl::context::no_sslv3 |
			asio::ssl::context::single_dh_use
		);

		m_sslContext.use_certificate_chain_file("server.crt");
		m_sslContext.use_private_key_file("server.key", asio::ssl::context::pem);
	}
	catch (const std::exception& e)
	{
		m_logger.error(std::format("Failed to load certificates: {}", e.what()));
	}
}

AgentsManagerPrivate::~AgentsManagerPrivate()
{
	m_ioContext.stop();
}

void AgentsManagerPrivate::listen(std::string port)
{
	asio::post(
		m_ioContext,
		[this, port = std::move(port)]() mutable {
			m_port = std::move(port);
			asio::co_spawn(m_ioContext, run(), asio::detached);
		}
	);
}

asio::awaitable<void> AgentsManagerPrivate::run()
{
	m_acceptor.open(tcp::v4());
	m_acceptor.set_option(tcp::acceptor::reuse_address(true));
	m_acceptor.bind({ tcp::v4(), static_cast<unsigned short>(std::stoi(m_port)) });

	m_acceptor.listen();

	m_logger.info(std::format("Listening on port {}...", m_port));

	for (;;)
	{
		auto socket = co_await m_acceptor.async_accept(asio::use_awaitable);
		asio::co_spawn(m_ioContext, acceptClient(std::move(socket)), asio::detached);
	}

	co_return;
}

asio::awaitable<void> AgentsManagerPrivate::acceptClient(tcp::socket&& client)
{
	const auto endpoint	= client.remote_endpoint();
	const auto address	= endpoint.address().to_string();
	const auto port		= endpoint.port();

	m_logger.info(std::format("Client {}{} connected", address, port));

	try
	{
		AgentConnection agentConnection(std::move(client), m_sslContext, *m_store.get());
		co_await agentConnection.run();
	}
	catch (const boost::system::system_error& e)
	{
		m_logger.info(std::format("Client {}{} disconnected: {}", address, port, e.what()));
	}
	catch (const std::exception& e)
	{
		m_logger.error(e.what());
	}

	co_return;
}