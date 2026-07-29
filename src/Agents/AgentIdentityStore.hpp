#pragma once

#include <scorch/server/IAgentIdentityStore.hpp>

#include <string>
#include <string_view>
#include <vector>

class AgentIdentityStore
	: public scorch::server::IAgentIdentityStore
{
public:
	virtual bool	loadAgentGuildIds(std::string_view uuid, std::vector<std::string>& guildIds) = 0;
	virtual bool	loadAgentFromGuildId(std::string_view guildId, std::string& uuid) = 0;
	virtual bool	saveAgentGuildId(std::string_view uuid, std::string_view guildId) = 0;
};
