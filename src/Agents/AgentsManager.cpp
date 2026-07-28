#include "AgentsManager.hpp"
#include "AgentsManager_p.hpp"

#include "Database/MongoDB/MongoDBAgentIdentityStore.hpp"
#include "Task/ReadyTaskState.hpp"

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

bool AgentsManager::saveAgentGUID(std::string_view uuid, std::string_view guid)
{
	return m_p->m_store.saveAgentGUID(uuid, guid);
}

scorch::server::Task<scorch::server::Agent> AgentsManager::findAgent(std::string_view guid) const
{
	std::string uuid;
	if (! m_p->m_store.loadAgentFromGUID(guid, uuid))
		return MakeReadyTask(scorch::server::Agent());

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
	std::string guid;
	std::string uuid = agent.uuid();
	const bool success = m_store.loadAgentGUID(uuid, guid);
	if (success)
		m_logger.info("Agent {} connected (GUID: {})", uuid, guid);
	else
		m_logger.error("Agent {} does not have a valid guid", uuid);
}

void AgentsManagerPrivate::onAgentDisconnected(scorch::server::Agent agent)
{

}
