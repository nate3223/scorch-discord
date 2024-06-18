#include "ServerConfig.hpp"

#include <algorithm>

namespace
{
	namespace Database
	{
		constexpr char Collection[]		= "ServerConfig";
		constexpr char GuildID[]		= "guildID";
		constexpr char ChannelID[]		= "channelID";
		constexpr char Servers[]		= "servers";
		constexpr char StatusWidget[]	= "statusWidget";
	}
}

Cache<ServerConfig> ServerConfigs::g_cache;

std::vector<std::unique_ptr<ServerConfig>> ServerConfig::FindAll(const mongocxx::client& client)
{
	std::vector<std::unique_ptr<ServerConfig>> serverConfigs;
	auto serverConfigCursor = client.database(MongoDB::DATABASE_NAME)[Database::Collection].find({});
	for (const auto& doc : serverConfigCursor)
		serverConfigs.emplace_back(new ServerConfig(doc));
	return serverConfigs;
}

ServerConfig::ServerConfig(const bsoncxx::document::view& view)
{
	if (const auto& guildID = view[Database::GuildID]; guildID)
		m_guildID = (uint64_t)guildID.get_int64().value;
	if (const auto& channelID = view[Database::ChannelID]; channelID)
		m_channelID = (int64_t)channelID.get_int64().value;
	if (const auto& servers = view[Database::Servers]; servers)
	{
		for (const auto& serverID : servers.get_array().value)
			m_serverIDs.push_back((uint64_t)serverID.get_int64().value);
	}
	if (const auto& statusWidget = view[Database::StatusWidget]; statusWidget)
		m_statusWidget = StatusWidget(statusWidget.get_document());
}

bsoncxx::document::value ServerConfig::getValue() const
{
	bsoncxx::builder::basic::document document;
	document.append(kvp(Database::GuildID, (int64_t)m_guildID));
	document.append(kvp(Database::ChannelID, (int64_t)m_channelID));

	array serverArr;
	for (const uint64_t server : m_serverIDs)
		serverArr.append((int64_t)server);
	document.append(kvp(Database::Servers, serverArr));
	document.append(kvp(Database::StatusWidget, m_statusWidget.getValue()));
	return document.extract();
}

void ServerConfig::insertIntoDatabase(const mongocxx::client& client)
{
	client.database(MongoDB::DATABASE_NAME)[Database::Collection].insert_one(getValue());
}

void ServerConfig::removeFromDatabase(const mongocxx::client& client)
{
	client.database(MongoDB::DATABASE_NAME)[Database::Collection].delete_one(make_document(kvp(Database::GuildID, (int64_t)m_guildID)));
}

void ServerConfig::updateChannelID(const mongocxx::client& client)
{
	client.database(MongoDB::DATABASE_NAME)[Database::Collection].update_one(
		make_document(kvp(Database::GuildID, (int64_t)m_guildID)),
		make_document(kvp("$set", make_document(kvp(Database::ChannelID, (int64_t)m_channelID))))
	);
}

void ServerConfig::updateServerIDs(const mongocxx::client& client)
{
	array arr;
	for (const uint64_t serverID : m_serverIDs)
		arr.append((int64_t)serverID);

	client.database(MongoDB::DATABASE_NAME)[Database::Collection].update_one(
		make_document(kvp(Database::GuildID, (int64_t)m_guildID)),
		make_document(kvp("$set", make_document(kvp(Database::Servers, arr))))
	);
}

void ServerConfig::updateStatusWidget(const mongocxx::client& client)
{
	client.database(MongoDB::DATABASE_NAME)[Database::Collection].update_one(
		make_document(kvp(Database::GuildID, (int64_t)m_guildID)),
		make_document(kvp("$set", make_document(kvp(Database::StatusWidget, m_statusWidget.getValue()))))
	);
}
