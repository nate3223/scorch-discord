#pragma once

#include "IAgentIdentityStore.hpp"

#include <memory>
#include <string>

class AgentsManagerPrivate;

class AgentsManager
{
public:
	AgentsManager(std::unique_ptr<IAgentIdentityStore> store);
	~AgentsManager();

	void	listen(std::string port);

private:
	std::unique_ptr<AgentsManagerPrivate>	m_p;
};
