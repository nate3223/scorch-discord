#pragma once

#include <cstdint>
#include <memory>
#include <optional>

class Server;

class StatusWidgetPrivate
{
public:
	std::optional<uint64_t>	m_messageID;
	std::optional<uint64_t>	m_agentMessageID;
	std::optional<uint64_t>	m_commandID;	// Do not save to database
	std::shared_ptr<Server>	m_activeServer;	// Do not save to database
	std::optional<uint64_t>	m_activeServerID;
};
