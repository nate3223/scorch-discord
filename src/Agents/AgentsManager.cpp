#include "AgentsManager.hpp"
#include "AgentsManager_p.hpp"

#include "Database/MongoDB/MongoDBAgentIdentityStore.hpp"

namespace
{
	constexpr auto kAgentsPort = "3224";
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
		m_logger.info("Agent {} connected (guild ID: {})", uuid, guildId);
	else
		m_logger.error("Agent {} does not have a valid guild ID", uuid);
}

void AgentsManagerPrivate::onAgentDisconnected(scorch::server::Agent agent)
{

}
