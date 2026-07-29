#include "ServerConfig.hpp"
#include "ServerConfig_p.hpp"

#include <algorithm>
#include <mongocxx/client.hpp>

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

std::vector<std::unique_ptr<ServerConfig>> ServerConfig::FindAll(const mongocxx::client& client)
{
	std::vector<std::unique_ptr<ServerConfig>> serverConfigs;
	auto serverConfigCursor = client.database(MongoDB::DATABASE_NAME)[Database::Collection].find({});
	for (const auto& doc : serverConfigCursor)
		serverConfigs.emplace_back(new ServerConfig(doc));
	return serverConfigs;
}

ServerConfig::ServerConfig()
	: m_p(std::make_unique<ServerConfigPrivate>())
{

}

ServerConfig::ServerConfig(const bsoncxx::document::view& view)
	: ServerConfig()
{
	if (const auto& guildID = view[Database::GuildID]; guildID)
		m_p->m_guildID = (uint64_t)guildID.get_int64().value;
	if (const auto& channelID = view[Database::ChannelID]; channelID)
		m_p->m_channelID = (int64_t)channelID.get_int64().value;
	if (const auto& servers = view[Database::Servers]; servers)
	{
		for (const auto& serverID : servers.get_array().value)
			m_p->m_serverIDs.push_back((uint64_t)serverID.get_int64().value);
	}
	if (const auto& statusWidget = view[Database::StatusWidget]; statusWidget)
		m_p->m_statusWidget = StatusWidget(statusWidget.get_document());
}

ServerConfig::~ServerConfig() = default;

bsoncxx::document::value ServerConfig::getValue() const
{
	bsoncxx::builder::basic::document document;
	document.append(kvp(Database::GuildID, (int64_t)m_p->m_guildID));
	document.append(kvp(Database::ChannelID, (int64_t)m_p->m_channelID));

	array serverArr;
	for (const uint64_t server : m_p->m_serverIDs)
		serverArr.append((int64_t)server);
	document.append(kvp(Database::Servers, serverArr));
	document.append(kvp(Database::StatusWidget, m_p->m_statusWidget.getValue()));
	return document.extract();
}

void ServerConfig::insertIntoDatabase(const mongocxx::client& client)
{
	client.database(MongoDB::DATABASE_NAME)[Database::Collection].insert_one(getValue());
}

void ServerConfig::removeFromDatabase(const mongocxx::client& client)
{
	client.database(MongoDB::DATABASE_NAME)[Database::Collection].delete_one(make_document(kvp(Database::GuildID, (int64_t)m_p->m_guildID)));
}

void ServerConfig::updateChannelID(const mongocxx::client& client)
{
	client.database(MongoDB::DATABASE_NAME)[Database::Collection].update_one(
		make_document(kvp(Database::GuildID, (int64_t)m_p->m_guildID)),
		make_document(kvp("$set", make_document(kvp(Database::ChannelID, (int64_t)m_p->m_channelID))))
	);
}

void ServerConfig::updateServerIDs(const mongocxx::client& client)
{
	array arr;
	for (const uint64_t serverID : m_p->m_serverIDs)
		arr.append((int64_t)serverID);

	client.database(MongoDB::DATABASE_NAME)[Database::Collection].update_one(
		make_document(kvp(Database::GuildID, (int64_t)m_p->m_guildID)),
		make_document(kvp("$set", make_document(kvp(Database::Servers, arr))))
	);
}

void ServerConfig::updateStatusWidget(const mongocxx::client& client)
{
	client.database(MongoDB::DATABASE_NAME)[Database::Collection].update_one(
		make_document(kvp(Database::GuildID, (int64_t)m_p->m_guildID)),
		make_document(kvp("$set", make_document(kvp(Database::StatusWidget, m_p->m_statusWidget.getValue()))))
	);
}

uint64_t ServerConfig::guildId() const noexcept
{
	return m_p->m_guildID;
}

void ServerConfig::setGuildId(uint64_t guildId) noexcept
{
	m_p->m_guildID = guildId;
}

uint64_t ServerConfig::channelId() const noexcept
{
	return m_p->m_channelID;
}

void ServerConfig::setChannelId(uint64_t channelId) noexcept
{
	m_p->m_channelID = channelId;
}

StatusWidget& ServerConfig::statusWidget() noexcept
{
	return m_p->m_statusWidget;
}

const StatusWidget& ServerConfig::statusWidget() const noexcept
{
	return m_p->m_statusWidget;
}

void ServerConfig::setStatusWidget(StatusWidget statusWidget)
{
	m_p->m_statusWidget = std::move(statusWidget);
}

std::vector<uint64_t>& ServerConfig::serverIds() noexcept
{
	return m_p->m_serverIDs;
}

const std::vector<uint64_t>& ServerConfig::serverIds() const noexcept
{
	return m_p->m_serverIDs;
}

void ServerConfig::setServerIds(std::vector<uint64_t> serverIds)
{
	m_p->m_serverIDs = std::move(serverIds);
}

std::shared_mutex& ServerConfig::mutex() const noexcept
{
	return m_p->m_mutex;
}
