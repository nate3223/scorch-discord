#pragma once

#include "IAgentIdentityStore.hpp"

#include <memory>
#include <optional>
#include <string>

class AgentsManagerPrivate;
struct PairingCodeRequest;

class AgentsManager
{
public:
	explicit							AgentsManager(std::unique_ptr<IAgentIdentityStore> store);
										~AgentsManager();

	void								listen(std::string port);
	std::shared_ptr<PairingCodeRequest>	confirmPairing(const std::string& pairingCode, std::string info);

private:
	std::unique_ptr<AgentsManagerPrivate>	m_p;
};
