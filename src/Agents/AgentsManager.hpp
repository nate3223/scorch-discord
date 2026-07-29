#pragma once

#include <scorch/server/Agent.hpp>
#include <scorch/server/Task.hpp>

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace scorch
{
	namespace server
	{
		class PairingCodeRequest;
	}
}

class AgentIdentityStore;
class AgentsManager;
class AgentsManagerPrivate;

class AgentStatusSubscription
{
public:
								AgentStatusSubscription() = default;
								~AgentStatusSubscription();

								AgentStatusSubscription(const AgentStatusSubscription&) = delete;
	AgentStatusSubscription&	operator=(const AgentStatusSubscription&) = delete;

								AgentStatusSubscription(AgentStatusSubscription&& other) noexcept;
	AgentStatusSubscription&	operator=(AgentStatusSubscription&& other) noexcept;

	void						reset();

private:
								AgentStatusSubscription(AgentsManager* manager, uint64_t id);

	AgentsManager*	m_manager = nullptr;
	uint64_t		m_id = 0;

	friend class AgentsManager;
};

class AgentsManager
{
public:
	using AgentStatusCallback = std::function<void(std::string_view guildId, scorch::server::Agent agent)>;

	static AgentsManager&	Instance();

	std::shared_ptr<scorch::server::PairingCodeRequest>	confirmPairing(const std::string& pairingCode, std::string info);
	bool												saveAgentGuildId(std::string_view uuid, std::string_view guildId);

	scorch::server::Task<scorch::server::Agent>			findAgent(std::string_view guildId) const;
	scorch::server::Agent								connectedAgent(std::string_view guildId) const;
	AgentStatusSubscription								subscribeToAgentStatus(AgentStatusCallback callback);

private:
							AgentsManager();
							~AgentsManager();
	void					unsubscribeFromAgentStatus(uint64_t id);

	friend class AgentStatusSubscription;
private:
	std::unique_ptr<AgentsManagerPrivate>	m_p;
};
