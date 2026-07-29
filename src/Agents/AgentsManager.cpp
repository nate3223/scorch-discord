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
	return m_p->m_store.saveAgentGuildId(uuid, guildId);
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
	if (const auto it = m_p->m_connectedAgents.find(std::string(guildId)); it != m_p->m_connectedAgents.end())
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
	std::string guildId;
	std::string uuid = agent.uuid();
	const bool success = m_store.loadAgentGuildId(uuid, guildId);
	if (success)
	{
		{
			std::unique_lock lock(m_agentsMutex);
			m_connectedAgents.erase(guildId);
			m_connectedAgents.emplace(guildId, agent);
		}
		m_logger.info("Agent {} connected (guild ID: {})", uuid, guildId);
		notifyAgentStatus(guildId, agent);
	}
	else
		m_logger.error("Agent {} does not have a valid guild ID", uuid);
}

void AgentsManagerPrivate::onAgentDisconnected(scorch::server::Agent agent)
{
	std::string guildId;
	const std::string uuid = agent.uuid();
	if (! m_store.loadAgentGuildId(uuid, guildId))
		return;

	bool cacheChanged = false;
	{
		std::unique_lock lock(m_agentsMutex);
		if (const auto it = m_connectedAgents.find(guildId);
			it != m_connectedAgents.end() && it->second.uuid() == uuid)
		{
			m_connectedAgents.erase(it);
			cacheChanged = true;
		}
	}

	m_logger.info("Agent {} disconnected (guild ID: {})", uuid, guildId);
	if (cacheChanged)
		notifyAgentStatus(guildId, {});
}

void AgentsManagerPrivate::notifyAgentStatus(std::string_view guildId, const scorch::server::Agent& agent)
{
	std::scoped_lock lock(m_subscribersMutex);
	for (const auto& callback : m_subscribers | std::views::values)
		callback(guildId, agent);
}
