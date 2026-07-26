#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <memory>

class IAgentIdentityStore;
class PairingCodeManager;

class AgentConnectionPrivate;

class AgentConnection
{
public:
									AgentConnection(
										boost::asio::ip::tcp::socket&& socket,
										boost::asio::ssl::context& context,
										IAgentIdentityStore& store,
										PairingCodeManager& pairingCodeManager
									);
									~AgentConnection();

	boost::asio::awaitable<void>	run();

private:
	std::unique_ptr<AgentConnectionPrivate>	m_p;
};
