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
	bool	loadAgentGuildId(std::string_view uuid, std::string& guildId) override;
	bool	loadAgentFromGuildId(std::string_view guildId, std::string& uuid) override;
	bool	saveAgentGuildId(std::string_view uuid, std::string_view guildId) override;

// scorch::server::IAgentIdentityStore i/f:
public:
	bool	loadPublicKey(std::string_view uuid, std::vector<std::byte>& publicKey) override;
	bool	savePublicKey(std::string_view uuid, std::span<const std::byte>& publicKey) override;

private:
	mongocxx::pool& m_pool;
};
