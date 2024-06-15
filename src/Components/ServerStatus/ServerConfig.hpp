#pragma once

#include "Document.hpp"
#include "Server.hpp"
#include "StatusWidget.hpp"

#include <memory>
#include <mongocxx/client.hpp>
#include <shared_mutex>
#include <vector>

class ServerConfig
	: public Document<Server>
{
public:
	static std::vector<std::unique_ptr<ServerConfig>>	FindAll(const mongocxx::client& client);

	ServerConfig() = default;
	ServerConfig(const bsoncxx::document::view& view);

	bsoncxx::document::value getValue() const override;

	void	insertIntoDatabase(const mongocxx::client& client);
	void	removeFromDatabase(const mongocxx::client& client);
	void	updateChannelID(const mongocxx::client& client);
	void	updateServerIDs(const mongocxx::client& client);
	void	updateStatusWidget(const mongocxx::client& client);

	uint64_t				m_guildID{ 0 };
	uint64_t				m_channelID{ 0 };
	StatusWidget			m_statusWidget;
	std::vector<uint64_t>	m_serverIDs;

	std::shared_mutex		m_mutex;
};