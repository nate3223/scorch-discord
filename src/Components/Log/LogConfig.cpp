#include "LogConfig.hpp"

namespace
{
	namespace Database
	{
		constexpr char Collection[]	= "LogConfig";
		constexpr char GuildID[]	= "guildID";
		constexpr char ChannelID[]	= "channelID";
	}
}

std::vector<std::unique_ptr<LogConfig>> LogConfig::FindAll(const mongocxx::client& client)
{
	std::vector<std::unique_ptr<LogConfig>> logConfigs;
	auto logConfigCursor = client.database(MongoDB::DATABASE_NAME)[Database::Collection].find({});
	for (const auto& doc : logConfigCursor)
		logConfigs.emplace_back(new LogConfig(doc));
	return logConfigs;
}

LogConfig::LogConfig(const bsoncxx::document::view& view)
{
	if (const auto& guildID = view[Database::GuildID]; guildID)
		m_guildID = (uint64_t)guildID.get_int64().value;
	if (const auto& channelID = view[Database::ChannelID]; channelID)
		m_channelID = (int64_t)channelID.get_int64().value;
}

bsoncxx::document::value LogConfig::getValue() const
{
	return make_document(
		kvp(Database::GuildID, (int64_t)m_guildID),
		kvp(Database::ChannelID, (int64_t)m_channelID)
	);
}

void LogConfig::insertIntoDatabase(const mongocxx::client& client)
{
	client.database(MongoDB::DATABASE_NAME)[Database::Collection].insert_one(getValue());
}

void LogConfig::removeFromDatabase(const mongocxx::client& client)
{
	client.database(MongoDB::DATABASE_NAME)[Database::Collection].delete_one(make_document(kvp(Database::GuildID, (int64_t)m_guildID)));
}

void LogConfig::updateChannelID(const mongocxx::client& client)
{
	client.database(MongoDB::DATABASE_NAME)[Database::Collection].update_one(
		make_document(kvp(Database::GuildID, (int64_t)m_guildID)),
		make_document(kvp("$set", make_document(kvp(Database::ChannelID, (int64_t)m_channelID))))
	);
}
