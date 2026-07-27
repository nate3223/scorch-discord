#pragma once

#include "AgentsManager.hpp"
#include "Component.hpp"

#include <atomic>
#include <boost/asio/io_context.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <dpp/dpp.h>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace asio = boost::asio;

class DiscordBot;

class DiscordBotPrivate
{
public:
					DiscordBotPrivate(DiscordBot& owner, const std::string& token);

	void			init();

	template <typename T, typename... Args>
	void			addComponent(Args&&... args);

	void			componentLog(std::unique_ptr<ComponentLogMessage> message);

	void			onLog(const dpp::log_t& event);
	void			onReady(const dpp::ready_t& event);
	dpp::task<void>	onSlashCommand(const dpp::slashcommand_t& event);
	dpp::task<void>	onButtonClick(const dpp::button_click_t& event);
	dpp::task<void>	onSelectClick(const dpp::select_click_t& event);
	dpp::task<void>	onFormSubmit(const dpp::form_submit_t& event);
	void			onChannelDelete(const dpp::channel_delete_t& event);
	void			onMessageDelete(const dpp::message_delete_t& event);

	DiscordBot&														m_owner;
	std::atomic_bool												m_running{ false };
	std::unique_ptr<dpp::cluster>									m_bot;
	std::vector<std::unique_ptr<Component>>							m_components;
	boost::unordered_flat_map<std::string, SlashCommand::Handler>	m_slashCommands;
	boost::unordered_flat_map<std::string, ButtonCommand::Handler>	m_buttonCommands;
	std::vector<ButtonCommand>										m_buttonPrefixCommands;
	boost::unordered_flat_map<std::string, SelectCommand::Handler>	m_selectCommands;
	std::vector<SelectCommand>										m_selectPrefixCommands;
	boost::unordered_flat_map<std::string, FormCommand::Handler>	m_formCommands;
	std::vector<FormCommand>										m_formPrefixCommands;
	boost::unordered_flat_map<dpp::snowflake, dpp::snowflake>		m_serverStatusChannel;

	AgentsManager													m_agentsManager;

	class ComponentLogger
	{
	public:
		ComponentLogger(std::vector<std::unique_ptr<Component>>& components);
		~ComponentLogger();

		void	log(std::unique_ptr<ComponentLogMessage> message);

	private:
		using WorkGuard = asio::executor_work_guard<asio::io_context::executor_type>;

		asio::io_context	m_ioContext;

		std::vector<std::unique_ptr<Component>>&	m_components;

		WorkGuard			m_workGuard;
		std::jthread		m_ioThread;
	};

	ComponentLogger												m_componentLogger;
};