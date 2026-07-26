#pragma once

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

class IAgentIdentityStore
{
public:
	virtual ~IAgentIdentityStore() = default;

	virtual bool	loadPublicKey(std::string_view uuid, std::vector<std::byte>& publicKey) = 0;
	virtual bool	savePublicKey(std::string_view uuid, std::span<const std::byte>& publicKey) = 0;
};
