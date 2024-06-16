#pragma once

#include "Cache.hpp"
#include "Component.hpp"
#include "Document.hpp"
#include "MongoDBManager.hpp"
#include "Server.hpp"
#include "ServerConfig.hpp"

#include <boost/unordered/unordered_flat_map.hpp>
#include <map>
#include <memory>
#include <shared_mutex>

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

	void	onServerCustomButton(const dpp::button_click_t& event);
	void	onServerSettingsButton(const dpp::button_click_t& event);
	void	onPinnedServerSelect(const dpp::select_click_t& event);
	void	onSelectQueryServer(const dpp::select_click_t& event);

protected:
	void			updateServerStatusWidget(const ServerConfig& config);

	dpp::interaction_modal_response	getAddServerModal();
	dpp::component	getRemoveServerComponent(const ServerConfig& config);
	dpp::component	getServerSelectMenuComponent(const ServerConfig& config);
	dpp::message	getServerStatusWidget(const ServerConfig& config);

	// Component
	void	onChannelDelete(const dpp::channel_delete_t& event) override;
	void	onMessageDelete(const dpp::message_delete_t& event) override;
	// /Component

private:
	mongocxx::pool&			m_databasePool;
	Cache<ServerConfig>		m_configs;
	Cache<Server>			m_servers;
};
