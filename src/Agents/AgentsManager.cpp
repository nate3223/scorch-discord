#include "AgentsManager.hpp"
#include "AgentsManager_p.hpp"

#include "Database/MongoDB/MongoDBAgentIdentityStore.hpp"

#include <ranges>
#include <utility>

namespace
{
	constexpr auto kAgentsPort = "3224";
}

AgentStatusSubscription::AgentStatusSubscription(AgentsManager* manager, uint64_t id)
	: m_manager(manager)
	, m_id(id)
{
}

AgentStatusSubscription::~AgentStatusSubscription()
{
	reset();
}

AgentStatusSubscription::AgentStatusSubscription(AgentStatusSubscription&& other) noexcept
	: m_manager(std::exchange(other.m_manager, nullptr))
	, m_id(std::exchange(other.m_id, 0))
{
}

AgentStatusSubscription& AgentStatusSubscription::operator=(AgentStatusSubscription&& other) noexcept
{
	if (this != &other)
	{
		reset();
		m_manager = std::exchange(other.m_manager, nullptr);
		m_id = std::exchange(other.m_id, 0);
	}
	return *this;
}

void AgentStatusSubscription::reset()
{
	if (m_manager)
		m_manager->unsubscribeFromAgentStatus(m_id);
	m_manager = nullptr;
	m_id = 0;
}

AgentsManager& AgentsManager::Instance()
{
	static AgentsManager gManager;
	return gManager;
}

std::shared_ptr<scorch::server::PairingCodeRequest>	AgentsManager::confirmPairing(const std::string& pairingCode, std::string info)
{
	return m_p->m_scorchServer.confirmPairing(pairingCode, std::move(info));
}

bool AgentsManager::saveAgentGuildId(std::string_view uuid, std::string_view guildId)
{
	if (! m_p->m_store.saveAgentGuildId(uuid, guildId))
		return false;

	scorch::server::Agent connectedAgent;
	{
		std::unique_lock lock(m_p->m_agentsMutex);
		if (const auto existing = m_p->m_connectedAgentsByUUID.find(std::string(uuid));
			existing != m_p->m_connectedAgentsByUUID.end() && existing->second)
			connectedAgent = existing->second;

		if (connectedAgent)
			m_p->m_connectedAgentsByGuildId.insert_or_assign(std::string(guildId), connectedAgent);
		else
			m_p->m_connectedAgentsByGuildId.erase(std::string(guildId));
	}

	m_p->notifyAgentStatus(guildId, connectedAgent);

	return true;
}

scorch::server::Task<scorch::server::Agent> AgentsManager::findAgent(std::string_view guildId) const
{
	std::string uuid;
	if (! m_p->m_store.loadAgentFromGuildId(guildId, uuid))
		return scorch::server::Task<scorch::server::Agent>(scorch::server::Agent());

	return m_p->m_scorchServer.findAgent(uuid);
}

scorch::server::Agent AgentsManager::connectedAgent(std::string_view guildId) const
{
	std::shared_lock lock(m_p->m_agentsMutex);
	if (const auto it = m_p->m_connectedAgentsByGuildId.find(std::string(guildId));
		it != m_p->m_connectedAgentsByGuildId.end())
		return it->second;

	return {};
}

scorch::server::Agent AgentsManager::connectedAgentByUUID(std::string_view uuid) const
{
	std::shared_lock lock(m_p->m_agentsMutex);
	if (const auto it = m_p->m_connectedAgentsByUUID.find(std::string(uuid));
		it != m_p->m_connectedAgentsByUUID.end())
		return it->second;

	return {};
}

AgentStatusSubscription AgentsManager::subscribeToAgentStatus(AgentStatusCallback callback)
{
	std::scoped_lock lock(m_p->m_subscribersMutex);
	const uint64_t id = m_p->m_nextSubscriberId++;
	m_p->m_subscribers.emplace(id, std::move(callback));
	return AgentStatusSubscription(this, id);
}

void AgentsManager::unsubscribeFromAgentStatus(uint64_t id)
{
	std::scoped_lock lock(m_p->m_subscribersMutex);
	m_p->m_subscribers.erase(id);
}

AgentsManager::AgentsManager()
	: m_p(std::make_unique<AgentsManagerPrivate>())
{

}

AgentsManager::~AgentsManager() = default;

AgentsManagerPrivate::AgentsManagerPrivate()
	: m_logger(Logger::Agents())
	, m_scorchServer(std::make_unique<MongoDBAgentIdentityStore>())
	, m_store(static_cast<AgentIdentityStore&>(m_scorchServer.getAgentIdentityStore()))
{
	m_listenerHandle = m_scorchServer.addAgentListener(this);
	m_scorchServer.start(::kAgentsPort);
}

void AgentsManagerPrivate::onAgentConnected(scorch::server::Agent agent)
{
	const std::string uuid = agent.uuid();
	{
		std::unique_lock lock(m_agentsMutex);
		m_connectedAgentsByUUID.insert_or_assign(uuid, agent);
	}

	std::vector<std::string> guildIds;
	if (! m_store.loadAgentGuildIds(uuid, guildIds))
	{
		m_logger.info("Agent {} connected without any guild associations", uuid);
		return;
	}

	{
		std::unique_lock lock(m_agentsMutex);
		for (const auto& guildId : guildIds)
			m_connectedAgentsByGuildId.insert_or_assign(guildId, agent);
	}

	m_logger.info("Agent {} connected to {} guilds", uuid, guildIds.size());
	for (const auto& guildId : guildIds)
		notifyAgentStatus(guildId, agent);
}

void AgentsManagerPrivate::onAgentDisconnected(scorch::server::Agent agent)
{
	std::vector<std::string> guildIds;
	const std::string uuid = agent.uuid();
	m_store.loadAgentGuildIds(uuid, guildIds);

	std::vector<std::string> changedGuildIds;
	changedGuildIds.reserve(guildIds.size());
	{
		std::unique_lock lock(m_agentsMutex);

		if (const auto it = m_connectedAgentsByUUID.find(uuid);
			it != m_connectedAgentsByUUID.end() && ! it->second.isConnected())
		{
			m_connectedAgentsByUUID.erase(it);
		}

		for (const auto& guildId : guildIds)
		{
			if (const auto it = m_connectedAgentsByGuildId.find(guildId);
				it != m_connectedAgentsByGuildId.end() &&
				it->second.uuid() == uuid &&
				! it->second.isConnected())
			{
				m_connectedAgentsByGuildId.erase(it);
				changedGuildIds.push_back(guildId);
			}
		}
	}

	m_logger.info("Agent {} disconnected from {} guilds", uuid, changedGuildIds.size());
	for (const auto guildId : changedGuildIds)
		notifyAgentStatus(guildId, {});
}

void AgentsManagerPrivate::notifyAgentStatus(std::string_view guildId, const scorch::server::Agent& agent)
{
	std::scoped_lock lock(m_subscribersMutex);
	for (const auto& callback : m_subscribers | std::views::values)
		callback(guildId, agent);
}
