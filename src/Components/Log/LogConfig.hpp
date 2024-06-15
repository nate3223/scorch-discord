#pragma once

#include "Document.hpp"

#include <mongocxx/client.hpp>
#include <shared_mutex>

class LogConfig
	: public Document<LogConfig>
{
public:
	static std::vector<std::unique_ptr<LogConfig>>	FindAll(const mongocxx::client& client);

	LogConfig() = default;
	LogConfig(const bsoncxx::document::view& view);

	// Document
	bsoncxx::document::value getValue() const override;
	// /Document

	void	insertIntoDatabase(const mongocxx::client& client);
	void	removeFromDatabase(const mongocxx::client& client);
	void	updateChannelID(const mongocxx::client& client);

	uint64_t			m_guildID{0};
	uint64_t			m_channelID{0};

	std::shared_mutex	m_mutex;
};