#pragma once

#include "Log.hpp"

#include "AgentIdentityStore.hpp"

#include <scorch/server/AgentListenerHandle.hpp>
#include <scorch/server/IAgentListener.hpp>
#include <scorch/server/Server.hpp>

class AgentsManagerPrivate final
	: public scorch::server::IAgentListener
{
public:
						AgentsManagerPrivate();

	// scorch::server::IAgentListener i/f:
public:
	void				onAgentConnected(scorch::server::Agent agent) override;
	void				onAgentDisconnected(scorch::server::Agent agent) override;

public:
	spdlog::logger&						m_logger;
	scorch::server::Server				m_scorchServer;
	scorch::server::AgentListenerHandle	m_listenerHandle;
	AgentIdentityStore&					m_store;
};
