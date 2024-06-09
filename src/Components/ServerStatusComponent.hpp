#pragma once

#include "Component.hpp"
#include "Document.hpp"
#include "MongoDBManager.hpp"

#include <boost/unordered/unordered_flat_map.hpp>
#include <map>
#include <memory>
#include <mutex>

class ServerConfig;

class ServerStatusComponent
	: public Component
{
public:
	ServerStatusComponent(DiscordBot& bot);

	void    onSetStatusChannel(const dpp::slashcommand_t& event);

	void	onAddServerCommand(const dpp::slashcommand_t& event);
	void	onAddServerForm(const dpp::form_submit_t& event);

	void	onRemoveServerCommand(const dpp::slashcommand_t& event);
	void	onRemoveServerSelect(const dpp::select_click_t& event);

	void	onSelectServer(const dpp::select_click_t& event);

	dpp::component	getServerSelectMenuComponent(const ServerConfig& config);

	// Component
	void	onChannelDelete(const dpp::channel_delete_t& event) override;
	// /Component

private:
	std::mutex			m_mutex;
	mongocxx::pool&		m_databasePool;
	boost::unordered_flat_map<uint64_t, std::unique_ptr<ServerConfig>>	m_configs;
};

class Server
	: public Document<Server>
{
public:
	Server(const uint64_t id, const std::string& name, const std::string& address)
		: m_id(id)
		, m_name(name)
		, m_address(address)
	{
	}

	Server(const bsoncxx::document::view& view)
	{
		if (const auto& id = view["id"]; id)
			m_id = (uint64_t)view["id"].get_int64().value;
		if (const auto& name = view["name"]; name)
			m_name = std::string(view["name"].get_string().value);
		if (const auto& address = view["address"]; address)
			m_address = std::string(view["address"].get_string().value);
	}

	bsoncxx::document::value getValue() const override
	{
		return make_document(
			kvp("id", (int64_t)m_id),
			kvp("name", m_name.c_str()),
			kvp("address", m_address.c_str())
		);
	}

	uint64_t	m_id;
	std::string	m_name;
	std::string	m_address;
};

class ServerConfig
	: public Document<Server>
{
public:
	ServerConfig() = default;
	ServerConfig(const bsoncxx::document::view& view)
	{
		if (const auto& guildID = view["guildID"]; guildID)
			m_guildID = (uint64_t)view["guildID"].get_int64().value;
		if (const auto& channelID = view["channelID"]; channelID)
			m_channelID = (int64_t)view["channelID"].get_int64().value;
		if (const auto& servers = view["servers"]; servers)
			m_servers = Server::ContructVector(servers.get_array().value);
	}

	bsoncxx::document::value getValue() const override
	{
		return make_document(
			kvp("guildID", (int64_t)m_guildID),
			kvp("channelID", (int64_t)m_channelID),
			kvp("servers", Server::ConstructArray(m_servers))
		);
	}

	uint64_t			m_guildID;
	uint64_t			m_channelID;
	std::vector<Server>	m_servers;
};