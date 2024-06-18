#include "DiscordBot.hpp"

#include "Log.hpp"
#include "LogComponent.hpp"
#include "ServerStatusComponent.hpp"
#include "YippeeComponent.hpp"

#include "MongoDBManager.hpp"

#include <algorithm>
#include <format>
#include <thread>

DiscordBot::DiscordBot(const char* token)
{
	m_bot = std::make_unique<dpp::cluster>(token, dpp::i_all_intents);
	m_bot->on_log(dpp::utility::cout_logger());
	m_bot->on_ready(std::bind_front(&DiscordBot::onReady, this));
	m_bot->on_slashcommand(std::bind_front(&DiscordBot::onSlashCommand, this));
	m_bot->on_button_click(std::bind_front(&DiscordBot::onButtonClick, this));
	m_bot->on_select_click(std::bind_front(&DiscordBot::onSelectClick, this));
	m_bot->on_form_submit(std::bind_front(&DiscordBot::onFormSubmit, this));
	m_bot->on_channel_delete(std::bind_front(&DiscordBot::onChannelDelete, this));
	m_bot->on_message_delete(std::bind_front(&DiscordBot::onMessageDelete, this));

	addComponent<LogComponent>();
	addComponent<YippeeComponent>();
	addComponent<ServerStatusComponent>();
}

void DiscordBot::start()
{
	bool expected = false;
	if (m_running.compare_exchange_strong(expected, true))
	{
		m_bot->start(dpp::st_wait);
	}
}

void DiscordBot::componentLog(const std::shared_ptr<ComponentLogMessage> message)
{
	std::thread t([message, this] {
		for (const auto& component : m_components)
			component->onComponentLog(message.get());
	});
	t.detach();
}

template <typename T, typename... Args>
void DiscordBot::addComponent(Args&&... args)
{
	std::unique_ptr<Component> component(new T(*this, std::forward<Args>(args)...));

	for (SlashCommand& slashCommand : component->getSlashCommands())
	{
		m_bot->log(dpp::loglevel::ll_info, std::format("Adding slash command {}", slashCommand.slashCommand.name));
		if (m_slashCommands.contains(slashCommand.slashCommand.name))
			m_bot->log(dpp::loglevel::ll_error, std::format("Command '{}' is already registered!", slashCommand.slashCommand.name));
		else
			m_slashCommands[slashCommand.slashCommand.name] = slashCommand.slashFunction;
	}

	for (ButtonCommand& buttonCommand : component->getButtonCommands())
	{
		switch (buttonCommand.type)
		{
			case (MatchType::EXACT):
				m_bot->log(dpp::loglevel::ll_info, std::format("Adding button command {}", buttonCommand.id));
				if (m_buttonCommands.contains(buttonCommand.id))
					m_bot->log(dpp::loglevel::ll_error, std::format("Command '{}' is already registered!", buttonCommand.id));
				else
					m_buttonCommands[buttonCommand.id] = buttonCommand.buttonFunction;
				break;
			case (MatchType::PREFIX):
				m_bot->log(dpp::loglevel::ll_info, std::format("Adding button prefix command {}", buttonCommand.id));
				if (auto it = std::find_if(m_buttonPrefixCommands.begin(), m_buttonPrefixCommands.end(), [buttonCommand](const ButtonCommand& command) {
						return command.id.starts_with(buttonCommand.id) || buttonCommand.id.starts_with(command.id);
					}); it != m_buttonPrefixCommands.end()
				)
					m_bot->log(dpp::loglevel::ll_error, std::format("Prefix command '{}' conflicts with '{}'!", buttonCommand.id, it->id));
				else
					m_buttonPrefixCommands.push_back(buttonCommand);
				break;
		}
	}

	for (SelectCommand& selectCommand : component->getSelectCommands())
	{
		switch (selectCommand.type)
		{
			case (MatchType::EXACT):
				m_bot->log(dpp::loglevel::ll_info, std::format("Adding select command {}", selectCommand.id));
				if (m_selectCommands.contains(selectCommand.id))
					m_bot->log(dpp::loglevel::ll_error, std::format("Command '{}' is already registered!", selectCommand.id));
				else
					m_selectCommands[selectCommand.id] = selectCommand.selectFunction;
				break;
			case (MatchType::PREFIX):
				m_bot->log(dpp::loglevel::ll_info, std::format("Adding select prefix command {}", selectCommand.id));
				if (auto it = std::find_if(m_selectPrefixCommands.begin(), m_selectPrefixCommands.end(), [selectCommand](const SelectCommand& command) {
					return command.id.starts_with(selectCommand.id) || selectCommand.id.starts_with(command.id);
					}); it != m_selectPrefixCommands.end()
				)
					m_bot->log(dpp::loglevel::ll_error, std::format("Prefix command '{}' conflicts with '{}'!", selectCommand.id, it->id));
				else
					m_selectPrefixCommands.push_back(selectCommand);
				break;
		}
		
	}

	for (FormCommand& formCommand : component->getFormCommands())
	{
		switch (formCommand.type)
		{
			case (MatchType::EXACT):
				m_bot->log(dpp::loglevel::ll_info, std::format("Adding form command {}", formCommand.id));
				if (m_formCommands.contains(formCommand.id))
					m_bot->log(dpp::loglevel::ll_error, std::format("Command '{}' is already registered!", formCommand.id));
				else
					m_formCommands[formCommand.id] = formCommand.formFunction;
				break;
			case (MatchType::PREFIX):
				m_bot->log(dpp::loglevel::ll_info, std::format("Adding form prefix command {}", formCommand.id));
				if (auto it = std::find_if(m_formPrefixCommands.begin(), m_formPrefixCommands.end(), [formCommand](const FormCommand& command) {
					return command.id.starts_with(formCommand.id) || formCommand.id.starts_with(command.id);
					}); it != m_formPrefixCommands.end()
						)
					m_bot->log(dpp::loglevel::ll_error, std::format("Prefix command '{}' conflicts with '{}'!", formCommand.id, it->id));
				else
					m_formPrefixCommands.push_back(formCommand);
				break;
		}
	}

	m_components.push_back(std::move(component));
}

void DiscordBot::onLog(const dpp::log_t& event)
{
	spdlog::logger& log = Logger::getInstance();
	switch (event.severity) {
	case dpp::ll_trace:
		log.trace("{}", event.message);
		break;
	case dpp::ll_debug:
		log.debug("{}", event.message);
		break;
	case dpp::ll_info:
		log.info("{}", event.message);
		break;
	case dpp::ll_warning:
		log.warn("{}", event.message);
		break;
	case dpp::ll_error:
		log.error("{}", event.message);
		break;
	case dpp::ll_critical:
	default:
		log.critical("{}", event.message);
		break;
	}
}

void DiscordBot::onReady(const dpp::ready_t& event)
{
	std::vector<dpp::slashcommand> slashCommands;
	auto backInserter = std::back_inserter(slashCommands);
	if (dpp::run_once<struct register_bot_commands>()) {
		for (auto& component : m_components)
		{
			const auto& componentSlashCommands = component->getSlashCommands();
			std::transform(componentSlashCommands.begin(), componentSlashCommands.end(), std::back_inserter(slashCommands), [](const SlashCommand& command){ return command.slashCommand; });
		}
	}
	m_bot->global_bulk_command_create(slashCommands);
}

void DiscordBot::onSlashCommand(const dpp::slashcommand_t& event)
{
	const auto& commandName = event.command.get_command_name();
	if (auto slashCommand = m_slashCommands.find(commandName); slashCommand != m_slashCommands.end())
		slashCommand->second(event);
	else
		m_bot->log(dpp::loglevel::ll_error, std::format("Unknown slash command '{}'!", commandName));
}

void DiscordBot::onButtonClick(const dpp::button_click_t& event)
{
	const auto& buttonID = event.custom_id;
	if (auto buttonCommand = m_buttonCommands.find(buttonID); buttonCommand != m_buttonCommands.end())
		buttonCommand->second(event);
	else if (auto buttonCommand = std::find_if(m_buttonPrefixCommands.begin(), m_buttonPrefixCommands.end(), [buttonID](const auto& command) {
			return buttonID.starts_with(command.id);
		}); buttonCommand != m_buttonPrefixCommands.end()
	)
		buttonCommand->buttonFunction(event);
	else
		m_bot->log(dpp::loglevel::ll_error, std::format("Unknown button command '{}'!", buttonID));
}

void DiscordBot::onSelectClick(const dpp::select_click_t& event)
{
	const auto& selectID = event.custom_id;
	if (auto selectCommand = m_selectCommands.find(selectID); selectCommand != m_selectCommands.end())
		selectCommand->second(event);
	else if (auto selectCommand = std::find_if(m_selectPrefixCommands.begin(), m_selectPrefixCommands.end(), [selectID](const auto& command) {
			return selectID.starts_with(command.id);
		}); selectCommand != m_selectPrefixCommands.end()
	)
		selectCommand->selectFunction(event);
	else
		m_bot->log(dpp::loglevel::ll_error, std::format("Unknown select command '{}'!", selectID));
}

void DiscordBot::onFormSubmit(const dpp::form_submit_t& event)
{
	const auto& formID = event.custom_id;
	if (auto formCommand = m_formCommands.find(formID); formCommand != m_formCommands.end())
		formCommand->second(event);
	else if (auto formCommand = std::find_if(m_formPrefixCommands.begin(), m_formPrefixCommands.end(), [formID](const auto& command) {
			return formID.starts_with(command.id);
		}); formCommand != m_formPrefixCommands.end()
			)
		formCommand->formFunction(event);
	else
		m_bot->log(dpp::loglevel::ll_error, std::format("Unknown form command '{}'!", formID));
}

void DiscordBot::onChannelDelete(const dpp::channel_delete_t& event)
{
	for (auto& component : m_components)
		component->onChannelDelete(event);
}

void DiscordBot::onMessageDelete(const dpp::message_delete_t& event)
{
	for (auto& component : m_components)
		component->onMessageDelete(event);
}

