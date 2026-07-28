#pragma once

#include "Agents/AgentIdentityStore.hpp"

#include <mongocxx/pool-fwd.hpp>

class MongoDBAgentIdentityStore final
	: public AgentIdentityStore
{
public:
	MongoDBAgentIdentityStore();

// AgentIdentityStore i/f:
public:
	bool	loadAgentGUID(std::string_view uuid, std::string& guid) override;
	bool	loadAgentFromGUID(std::string_view guid, std::string& uuid) override;
	bool	saveAgentGUID(std::string_view uuid, std::string_view guid) override;

// scorch::server::IAgentIdentityStore i/f:
public:
	bool	loadPublicKey(std::string_view uuid, std::vector<std::byte>& publicKey) override;
	bool	savePublicKey(std::string_view uuid, std::span<const std::byte>& publicKey) override;

private:
	mongocxx::pool& m_pool;
};
