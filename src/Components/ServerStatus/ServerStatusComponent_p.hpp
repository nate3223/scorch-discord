#pragma once

#include "Agents/AgentsManager.hpp"

#include <dpp/dpp.h>
#include <mongocxx/pool.hpp>

#include <atomic>

class ServerStatusComponentPrivate
{
public:
	ServerStatusComponentPrivate(AgentsManager& agentsManager, mongocxx::pool& databasePool)
		: m_agentsManager(agentsManager)
		, m_databasePool(databasePool)
	{
	}

	AgentsManager&			m_agentsManager;
	mongocxx::pool&			m_databasePool;
	AgentStatusSubscription	m_agentStatusSubscription;
	dpp::timer				m_agentStatusTimer;
	std::atomic_bool		m_agentStatusUpdateRunning{ false };
};
