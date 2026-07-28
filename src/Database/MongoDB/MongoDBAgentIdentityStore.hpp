#pragma once

#include <scorch/server/IAgentIdentityStore.hpp>

#include <mongocxx/pool-fwd.hpp>

class MongoDBAgentIdentityStore
	: public scorch::server::IAgentIdentityStore
{
public:
	MongoDBAgentIdentityStore();

// scorch::server::IAgentIdentityStore i/f:
public:
	bool	loadPublicKey(std::string_view uuid, std::vector<std::byte>& publicKey) override;
	bool	savePublicKey(std::string_view uuid, std::span<const std::byte>& publicKey) override;

private:
	mongocxx::pool& m_pool;
};
