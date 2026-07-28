#pragma once

#include <scorch/server/Agent.hpp>
#include <scorch/server/Task.hpp>

#include <string_view>

namespace scorch
{
	namespace server
	{
		class PairingCodeRequest;
	}
}

class AgentIdentityStore;
class AgentsManagerPrivate;

class AgentsManager
{
public:
	static AgentsManager&	Instance();

	std::shared_ptr<scorch::server::PairingCodeRequest>	confirmPairing(const std::string& pairingCode, std::string info);
	bool												saveAgentGUID(std::string_view uuid, std::string_view guid);

	scorch::server::Task<scorch::server::Agent>			findAgent(std::string_view guid) const;

private:
							AgentsManager();
							~AgentsManager();
private:
	std::unique_ptr<AgentsManagerPrivate>	m_p;
};