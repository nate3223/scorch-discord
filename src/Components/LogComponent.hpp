#pragma once

#include "Component.hpp"
#include "Document.hpp"
#include "MongoDBManager.hpp"

#include <boost/unordered/unordered_flat_map.hpp>
#include <memory>
#include <mutex>

class LogConfig;

class LogComponent
	: public Component
{
public:
	LogComponent(DiscordBot& bot);

	void	onSetLogChannel(const dpp::slashcommand_t& event);

	// Component
	void	onChannelDelete(const dpp::channel_delete_t& event) override;
	void	onComponentLog(const ComponentLogMessage* message) override;
	// /Component

private:
	std::mutex		m_mutex;
	mongocxx::pool& m_databasePool;
	boost::unordered_flat_map<uint64_t, std::unique_ptr<LogConfig>>	m_configs;
};

class LogConfig
	: public Document<LogConfig>
{
public:
	LogConfig() = default;
	LogConfig(const bsoncxx::document::view& view)
	{
		if (const auto& guildID = view["guildID"]; guildID)
			m_guildID = (uint64_t)view["guildID"].get_int64().value;
		if (const auto& channelID = view["channelID"]; channelID)
			m_channelID = (int64_t)view["channelID"].get_int64().value;
	}

	bsoncxx::document::value getValue() const override
	{
		return make_document(
			kvp("guildID", (int64_t)m_guildID),
			kvp("channelID", (int64_t)m_channelID)
		);
	}

	uint64_t			m_guildID;
	uint64_t			m_channelID;
};