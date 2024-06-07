#include "DiscordBot.hpp"

#include "Log.hpp"
#include "ServerStatusComponent.hpp"
#include "YippeeComponent.hpp"

#include "MongoDBManager.hpp"

#include <format>

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

	addComponent<YippeeComponent>();
	addComponent<ServerStatusComponent>();
}

void DiscordBot::start()
{
	m_bot->start(dpp::st_wait);
}

template <typename T, typename... Args>
void DiscordBot::addComponent(Args&&... args)
{
	std::unique_ptr<Component> component(new T(*m_bot, std::forward<Args>(args)...));

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
		m_bot->log(dpp::loglevel::ll_info, std::format("Adding button command {}", buttonCommand.id));
		if (m_buttonCommands.contains(buttonCommand.id))
			m_bot->log(dpp::loglevel::ll_error, std::format("Command '{}' is already registered!", buttonCommand.id));
		else
			m_buttonCommands[buttonCommand.id] = buttonCommand.buttonFunction;
	}

	for (SelectCommand& selectCommand : component->getSelectCommands())
	{
		m_bot->log(dpp::loglevel::ll_info, std::format("Adding select command {}", selectCommand.id));
		if (m_selectCommands.contains(selectCommand.id))
			m_bot->log(dpp::loglevel::ll_error, std::format("Command '{}' is already registered!", selectCommand.id));
		else
			m_selectCommands[selectCommand.id] = selectCommand.selectFunction;
	}

	for (FormCommand& formCommand : component->getFormCommands())
	{
		m_bot->log(dpp::loglevel::ll_info, std::format("Adding form command {}", formCommand.id));
		if (m_formCommands.contains(formCommand.id))
			m_bot->log(dpp::loglevel::ll_error, std::format("Command '{}' is already registered!", formCommand.id));
		else
			m_formCommands[formCommand.id] = formCommand.formFunction;
	}

	m_components.push_back(std::move(component));
}

void DiscordBot::onLog(const dpp::log_t& event)
{
	auto log = Logger::getInstance();
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
	if (dpp::run_once<struct register_bot_commands>()) {
		for (auto& component : m_components)
		{
			for (const SlashCommand& slashCommand : component->getSlashCommands())
				m_bot->global_command_create(slashCommand.slashCommand);
		}
	}
}

void DiscordBot::onSlashCommand(const dpp::slashcommand_t& event)
{
	const auto& commandName = event.command.get_command_name();
	if (m_slashCommands.contains(commandName))
		m_slashCommands[commandName](event);
	else
		m_bot->log(dpp::loglevel::ll_error, std::format("Unknown slash command '{}'!", commandName));
}

void DiscordBot::onButtonClick(const dpp::button_click_t& event)
{
	const auto& buttonID = event.custom_id;
	if (m_buttonCommands.contains(buttonID))
		m_buttonCommands[buttonID](event);
	else
		m_bot->log(dpp::loglevel::ll_error, std::format("Unknown button command '{}'!", buttonID));
}

void DiscordBot::onSelectClick(const dpp::select_click_t& event)
{
	const auto& selectID = event.custom_id;
	if (m_selectCommands.contains(selectID))
		m_selectCommands[selectID](event);
	else
		m_bot->log(dpp::loglevel::ll_error, std::format("Unknown select command '{}'!", selectID));
}

void DiscordBot::onFormSubmit(const dpp::form_submit_t& event)
{
	const auto& formID = event.custom_id;
	if (m_formCommands.contains(formID))
		m_formCommands[formID](event);
	else
		m_bot->log(dpp::loglevel::ll_error, std::format("Unknown form command '{}'!", formID));
}

void DiscordBot::onChannelDelete(const dpp::channel_delete_t& event)
{
	for (auto& component : m_components)
		component->onChannelDelete(event);
}

