#pragma once

#include <scorch/server/IAgentIdentityStore.hpp>

#include <string>
#include <string_view>

class AgentIdentityStore
	: public scorch::server::IAgentIdentityStore
{
public:
	virtual bool	loadAgentGUID(std::string_view uuid, std::string& guid) = 0;
	virtual bool	loadAgentFromGUID(std::string_view guid, std::string& uuid) = 0;
	virtual bool	saveAgentGUID(std::string_view uuid, std::string_view guid) = 0;
};
