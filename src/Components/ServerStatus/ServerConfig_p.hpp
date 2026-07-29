#pragma once

#include "StatusWidget.hpp"

#include <cstdint>
#include <shared_mutex>
#include <vector>

class ServerConfigPrivate
{
public:
	uint64_t					m_guildID = 0;
	uint64_t					m_channelID = 0;
	StatusWidget				m_statusWidget;
	std::vector<uint64_t>		m_serverIDs;
	mutable std::shared_mutex	m_mutex;
};
