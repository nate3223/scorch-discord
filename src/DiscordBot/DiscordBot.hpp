#pragma once

#include "Component.hpp"

#include <boost/unordered/unordered_flat_map.hpp>
#include <dpp/dpp.h>
#include <memory>
#include <vector>
#include <unordered_map>

class DiscordBot
{
public:
	DiscordBot(const char* token);

	void	start();

private:
	template <typename T, typename... Args>
	void	addComponent(Args&&... args);

	void    onLog(const dpp::log_t& event);
	void	onReady(const dpp::ready_t& event);
	void	onSlashCommand(const dpp::slashcommand_t& event);
	void	onButtonClick(const dpp::button_click_t& event);
	void	onSelectClick(const dpp::select_click_t& event);
	void	onFormSubmit(const dpp::form_submit_t& event);
	void	onChannelDelete(const dpp::channel_delete_t& event);

	std::unique_ptr<dpp::cluster>								m_bot;
	std::vector<std::unique_ptr<Component>>						m_components;
	boost::unordered_flat_map<std::string, SlashFunction>		m_slashCommands;
	boost::unordered_flat_map<std::string, ButtonFunction>		m_buttonCommands;
	boost::unordered_flat_map<std::string, SelectFunction>		m_selectCommands;
	boost::unordered_flat_map<std::string, FormFunction>		m_formCommands;
	boost::unordered_flat_map<dpp::snowflake, dpp::snowflake>	m_serverStatusChannel;
};