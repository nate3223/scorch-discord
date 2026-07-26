#pragma once

#include "IAgentIdentityStore.hpp"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <memory>

class AgentConnectionPrivate;

class AgentConnection
{
public:
									AgentConnection(boost::asio::ip::tcp::socket&& socket, boost::asio::ssl::context& context, IAgentIdentityStore& store);
									~AgentConnection();

	boost::asio::awaitable<void>	run();

private:
	std::unique_ptr<AgentConnectionPrivate>	m_p;
};
