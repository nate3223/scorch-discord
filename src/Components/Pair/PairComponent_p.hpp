#pragma once

#include <boost/unordered/unordered_flat_map.hpp>

#include <chrono>
#include <mutex>
#include <string>
#include <string_view>

class AgentsManager;

struct ShareCode
{
	std::string								uuid;
	std::chrono::steady_clock::time_point	expiresAt;
};

class PairComponentPrivate
{
public:
								PairComponentPrivate();

	std::string					createShareCode(std::string uuid);
	std::optional<std::string>	consumeShareCode(std::string_view code);

	AgentsManager&										m_agentsManager;
	std::mutex											m_shareCodesMutex;
	boost::unordered_flat_map<std::string, ShareCode>	m_shareCodes;
};
