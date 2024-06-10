#pragma once

#include "Component.hpp"
#include "Document.hpp"
#include "MongoDBManager.hpp"

#include <boost/unordered/unordered_flat_map.hpp>
#include <map>
#include <memory>
#include <mutex>

class Server;
class ServerConfig;

class ServerStatusComponent
	: public Component
{
public:
	ServerStatusComponent(DiscordBot& bot);

	void    onSetStatusChannel(const dpp::slashcommand_t& event);

	void	onAddServerCommand(const dpp::slashcommand_t& event);
	void	onAddServerButton(const dpp::button_click_t& event);
	void	onAddServerForm(const dpp::form_submit_t& event);

	void	onRemoveServerCommand(const dpp::slashcommand_t& event);
	void	onRemoveServerButton(const dpp::button_click_t& event);
	void	onRemoveServerSelect(const dpp::select_click_t& event);

	void	onSelectServer(const dpp::select_click_t& event);

	void	onServerRestartButton(const dpp::button_click_t& event);
	void	onServerStartButton(const dpp::button_click_t& event);
	void	onServerStopButton(const dpp::button_click_t& event);
	void	onServerSettingsButton(const dpp::button_click_t& event);
	void	onPinnedServerSelect(const dpp::select_click_t& event);
	void	onSelectQueryServer(const dpp::select_click_t& event);

protected:
	void					updateServerStatusWidget(const ServerConfig& config);

	static dpp::interaction_modal_response	getAddServerModal();
	static dpp::component	getRemoveServerComponent(const ServerConfig& config);
	static dpp::component	getServerSelectMenuComponent(const ServerConfig& config);
	static dpp::embed		getServerStatusEmbed(const Server& server);
	static dpp::message		getServerStatusWidget(const ServerConfig& config);

	// Component
	void	onChannelDelete(const dpp::channel_delete_t& event) override;
	void	onMessageDelete(const dpp::message_delete_t& event) override;
	// /Component

private:
	std::recursive_mutex	m_mutex;
	mongocxx::pool&			m_databasePool;
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

	uint64_t	m_id{0};
	std::string	m_name;
	std::string	m_address;
};

class StatusWidget
	: public Document<StatusWidget>
{
public:
	StatusWidget() = default;
	StatusWidget(const bsoncxx::document::view& view)
	{
		if (const auto& messageID = view["messageID"]; messageID)
			m_messageID = (uint64_t)view["messageID"].get_int64().value;
		if (const auto& activeServerID = view["activeServerID"]; activeServerID)
			m_activeServerID = (uint64_t)view["activeServerID"].get_int64().value;
	}

	bsoncxx::document::value getValue() const override
	{
		bsoncxx::builder::basic::document document;
		if (m_messageID.has_value())
			document.append(kvp("messageID", (int64_t)*m_messageID));
		if (m_activeServerID.has_value())
			document.append(kvp("activeServerID", (int64_t)*m_activeServerID));
		return document.extract();
	}

	std::optional<uint64_t>	m_messageID;
	std::optional<uint64_t>	m_commandID;	// Do not save to database
	Server*					m_activeServer;	// Do not save to database
	std::optional<uint64_t>	m_activeServerID;
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
			m_servers = Server::ContructUniqueVector(servers.get_array().value);
		if (const auto& statusWidget = view["statusWidget"]; statusWidget)
			m_statusWidget = StatusWidget(statusWidget.get_document());
	}

	bsoncxx::document::value getValue() const override
	{
		bsoncxx::builder::basic::document document;
		document.append(kvp("guildID", (int64_t)m_guildID));
		document.append(kvp("channelID", (int64_t)m_channelID));
		document.append(kvp("servers", Server::ConstructArray(m_servers)));
		document.append(kvp("statusWidget", m_statusWidget.getValue()));
		return document.extract();
	}

	uint64_t			m_guildID{0};
	uint64_t			m_channelID{0};
	StatusWidget		m_statusWidget;
	std::vector<std::unique_ptr<Server>>	m_servers;
};