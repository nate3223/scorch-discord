#pragma once

#include "Log.hpp"

#include "AgentIdentityStore.hpp"

#include <scorch/server/AgentListenerHandle.hpp>
#include <scorch/server/IAgentListener.hpp>
#include <scorch/server/Server.hpp>

#include <boost/unordered/unordered_flat_map.hpp>

#include <mutex>
#include <shared_mutex>
#include <string>

class AgentsManagerPrivate final
	: public scorch::server::IAgentListener
{
public:
						AgentsManagerPrivate();

	// scorch::server::IAgentListener i/f:
public:
	void				onAgentConnected(scorch::server::Agent agent) override;
	void				onAgentDisconnected(scorch::server::Agent agent) override;

	void				notifyAgentStatus(std::string_view guildId, const scorch::server::Agent& agent);

public:
	spdlog::logger&						m_logger;
	scorch::server::Server				m_scorchServer;
	scorch::server::AgentListenerHandle	m_listenerHandle;
	AgentIdentityStore&					m_store;

	mutable std::shared_mutex										m_agentsMutex;
	boost::unordered_flat_map<std::string, scorch::server::Agent>	m_connectedAgentsByUUID;
	boost::unordered_flat_map<std::string, scorch::server::Agent>	m_connectedAgentsByGuildId;

	std::mutex														m_subscribersMutex;
	uint64_t														m_nextSubscriberId = 1;
	boost::unordered_flat_map<uint64_t, AgentsManager::AgentStatusCallback> m_subscribers;
};
