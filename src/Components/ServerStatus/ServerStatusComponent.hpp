#pragma once

#include "Components/Component.hpp"

#include <memory>
#include <string_view>

namespace scorch::server
{
	class Agent;
}

class ServerConfig;


class ServerStatusComponentPrivate;
class ServerStatusComponent final
	: public Component
{
public:
	explicit		ServerStatusComponent(DiscordBot& bot);
					~ServerStatusComponent() override;

	dpp::task<void>	onSetStatusChannel(const dpp::slashcommand_t& event);

	void			onAddServerCommand(const dpp::slashcommand_t& event);
	void			onAddServerButton(const dpp::button_click_t& event);
	void			onAddServerForm(const dpp::form_submit_t& event);

	void			onRemoveServerCommand(const dpp::slashcommand_t& event);
	void			onRemoveServerButton(const dpp::button_click_t& event);
	void			onRemoveServerSelect(const dpp::select_click_t& event);

	void			onServerCustomButton(const dpp::button_click_t& event);
	void			onWidgetSettingsButton(const dpp::button_click_t& event);
	void			onSelectQueryServer(const dpp::select_click_t& event);

	void			onPinnedServerSelect(const dpp::select_click_t& event);

	void			onServerSettingsButton(const dpp::button_click_t& event);

	void			onAddCustomServerButtonButton(const dpp::button_click_t& event);
	void			onAddCustomServerButtonForm(const dpp::form_submit_t& event);
	void			onRemoveCustomServerButtonButton(const dpp::button_click_t& event);
	void			onRemoveCustomServerButtonSelect(const dpp::select_click_t& event);

// Component i/f:
public:
	dpp::task<void>	onChannelDelete(const dpp::channel_delete_t& event) override;
	dpp::task<void>	onMessageDelete(const dpp::message_delete_t& event) override;

private:
	void							updateServerStatusWidget(const ServerConfig& config);
	void							updateAgentStatusWidget(std::string_view guildId, const scorch::server::Agent& agent);
	dpp::task<void>					updateAgentStatusWidgets();

	dpp::interaction_modal_response	getAddServerModal();
	dpp::component					getRemoveServerComponent(const ServerConfig& config);
	dpp::component					getServerSelectMenuComponent(const ServerConfig& config);
	dpp::message					getAgentStatusWidget(const ServerConfig& config, const scorch::server::Agent& agent);
	dpp::message					getServerStatusWidget(const ServerConfig& config);

private:
	std::unique_ptr<ServerStatusComponentPrivate>	m_p;
};
