#include "DiscordBot.hpp"
#include "DiscordBot_p.hpp"

#include "Log.hpp"
#include "LogComponent.hpp"
#include "PairComponent.hpp"
#include "ServerStatusComponent.hpp"
#include "YippeeComponent.hpp"

#include "MongoDBManager.hpp"
#include "MongoDBAgentIdentityStore.hpp"

#include <boost/asio/post.hpp>

#include <algorithm>
#include <format>
#include <ranges>
#include <thread>

namespace
{
	constexpr auto kAgentsPort = "3224";
}

DiscordBot::DiscordBot(const std::string& token)
{
	m_p = std::make_unique<DiscordBotPrivate>(*this, token);
	m_p->init();
}

DiscordBot::~DiscordBot() = default;

void DiscordBot::start()
{
	bool expected = false;
	if (m_p->m_running.compare_exchange_strong(expected, true))
	{
		m_p->m_bot->start(dpp::st_wait);
	}
}

void DiscordBot::componentLog(std::unique_ptr<ComponentLogMessage> message)
{
	m_p->componentLog(std::move(message));
}

scorch::server::AgentsManager& DiscordBot::getAgentsManager()
{
	return m_p->m_agentsManager;
}

dpp::cluster& DiscordBot::operator*() const
{
	return *m_p->m_bot;
}

dpp::cluster* DiscordBot::operator->() const
{
	return m_p->m_bot.get();
}

DiscordBotPrivate::DiscordBotPrivate(DiscordBot& owner, const std::string& token)
	: m_owner(owner)
	, m_agentsManager(std::make_unique<MongoDBAgentIdentityStore>())
	, m_componentLogger(m_components)
{
	m_bot = std::make_unique<dpp::cluster>(token, dpp::i_all_intents);
}

void DiscordBotPrivate::init()
{
	m_bot->on_log(std::bind_front(&DiscordBotPrivate::onLog, this));
	m_bot->on_ready(std::bind_front(&DiscordBotPrivate::onReady, this));
	m_bot->on_slashcommand(std::bind_front(&DiscordBotPrivate::onSlashCommand, this));
	m_bot->on_button_click(std::bind_front(&DiscordBotPrivate::onButtonClick, this));
	m_bot->on_select_click(std::bind_front(&DiscordBotPrivate::onSelectClick, this));
	m_bot->on_form_submit(std::bind_front(&DiscordBotPrivate::onFormSubmit, this));
	m_bot->on_channel_delete(std::bind_front(&DiscordBotPrivate::onChannelDelete, this));
	m_bot->on_message_delete(std::bind_front(&DiscordBotPrivate::onMessageDelete, this));

	addComponent<LogComponent>();
	addComponent<PairComponent>();
	addComponent<YippeeComponent>();
	addComponent<ServerStatusComponent>();

	m_agentsManager.listen(::kAgentsPort);
}

template <typename T, typename... Args>
void DiscordBotPrivate::addComponent(Args&&... args)
{
	std::unique_ptr<Component> component(new T(m_owner, std::forward<Args>(args)...));

	for (SlashCommand& slashCommand : component->getSlashCommands())
	{
		m_bot->log(dpp::loglevel::ll_info, std::format("Adding slash command {}", slashCommand.slashCommand.name));
		if (m_slashCommands.contains(slashCommand.slashCommand.name))
			m_bot->log(dpp::loglevel::ll_error, std::format("Command '{}' is already registered!", slashCommand.slashCommand.name));
		else
			m_slashCommands[slashCommand.slashCommand.name] = slashCommand.handler;
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
					m_buttonCommands[buttonCommand.id] = buttonCommand.handler;
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
					m_selectCommands[selectCommand.id] = selectCommand.handler;
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
					m_formCommands[formCommand.id] = formCommand.handler;
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

void DiscordBotPrivate::componentLog(std::unique_ptr<ComponentLogMessage> message)
{
	m_componentLogger.log(std::move(message));
}

void DiscordBotPrivate::onLog(const dpp::log_t& event)
{
	spdlog::logger& log = Logger::DPP();
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

void DiscordBotPrivate::onReady(const dpp::ready_t& event)
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

dpp::task<void> DiscordBotPrivate::onSlashCommand(const dpp::slashcommand_t& event)
{
	const auto& commandName = event.command.get_command_name();
	if (auto slashCommand = m_slashCommands.find(commandName); slashCommand != m_slashCommands.end())
	{
		co_await std::visit([&event](auto& handler) -> dpp::task<void> {
			using Handler = std::decay_t<decltype(handler)>;
			if constexpr (std::is_same_v<Handler, SlashCommand::RegularHandler>)
				handler(event);
			else
				co_await handler(event);

			co_return;
		}, slashCommand->second);
	}
	else
		m_bot->log(dpp::loglevel::ll_error, std::format("Unknown slash command '{}'!", commandName));
}

dpp::task<void> DiscordBotPrivate::onButtonClick(const dpp::button_click_t& event)
{
	const auto& buttonID = event.custom_id;
	ButtonCommand::Handler* handler = nullptr;

	if (auto buttonCommand = m_buttonCommands.find(buttonID); buttonCommand != m_buttonCommands.end())
		handler = &buttonCommand->second;
	else if (
		auto buttonCommand = std::ranges::find_if(m_buttonPrefixCommands, [buttonID](const auto& command) { return buttonID.starts_with(command.id); });
		buttonCommand != m_buttonPrefixCommands.end()
	)
		handler = &buttonCommand->handler;

	if (handler)
	{
		co_await std::visit([&event](auto& handler) -> dpp::task<void> {
			using Handler = std::decay_t<decltype(handler)>;
			if constexpr (std::is_same_v<Handler, ButtonCommand::RegularHandler>)
				handler(event);
			else
				co_await handler(event);

			co_return;
		}, *handler);
	}
	else
		m_bot->log(dpp::loglevel::ll_error, std::format("Unknown button command '{}'!", buttonID));
}

dpp::task<void> DiscordBotPrivate::onSelectClick(const dpp::select_click_t& event)
{
	const auto& selectID = event.custom_id;
	SelectCommand::Handler* handler = nullptr;

	if (auto selectCommand = m_selectCommands.find(selectID); selectCommand != m_selectCommands.end())
		handler = &selectCommand->second;
	else if (
		auto selectCommand = std::ranges::find_if(m_selectPrefixCommands, [selectID](const auto& command) { return selectID.starts_with(command.id); });
		selectCommand != m_selectPrefixCommands.end()
	)
		handler = &selectCommand->handler;

	if (handler)
	{
		co_await std::visit([&event](auto& handler) -> dpp::task<void> {
			using Handler = std::decay_t<decltype(handler)>;
			if constexpr (std::is_same_v<Handler, SelectCommand::RegularHandler>)
				handler(event);
			else
				co_await handler(event);

			co_return;
		}, *handler);
	}
	else
		m_bot->log(dpp::loglevel::ll_error, std::format("Unknown select command '{}'!", selectID));
}

dpp::task<void> DiscordBotPrivate::onFormSubmit(const dpp::form_submit_t& event)
{
	const auto& formID = event.custom_id;
	FormCommand::Handler* handler = nullptr;

	if (auto formCommand = m_formCommands.find(formID); formCommand != m_formCommands.end())
		handler = &formCommand->second;
	else if (
		auto formCommand = std::ranges::find_if(m_formPrefixCommands, [formID](const auto& command) { return formID.starts_with(command.id); });
		formCommand != m_formPrefixCommands.end()
	)
		handler = &formCommand->handler;

	if (handler)
	{
		co_await std::visit([&event](auto& handler) -> dpp::task<void> {
			using Handler = std::decay_t<decltype(handler)>;
			if constexpr (std::is_same_v<Handler, FormCommand::RegularHandler>)
				handler(event);
			else
				co_await handler(event);

			co_return;
		}, *handler);
	}
	else
		m_bot->log(dpp::loglevel::ll_error, std::format("Unknown form command '{}'!", formID));
}

void DiscordBotPrivate::onChannelDelete(const dpp::channel_delete_t& event)
{
	for (auto& component : m_components)
		component->onChannelDelete(event);
}

void DiscordBotPrivate::onMessageDelete(const dpp::message_delete_t& event)
{
	for (auto& component : m_components)
		component->onMessageDelete(event);
}

DiscordBotPrivate::ComponentLogger::ComponentLogger(std::vector<std::unique_ptr<Component>>& components)
	: m_workGuard(asio::make_work_guard(m_ioContext))
	, m_components(components)
	, m_ioThread([this] {
		m_ioContext.run();
	})
{

}

DiscordBotPrivate::ComponentLogger::~ComponentLogger()
{
	m_workGuard.reset();
	m_ioContext.stop();
}

void DiscordBotPrivate::ComponentLogger::log(std::unique_ptr<ComponentLogMessage> message)
{
	asio::post(m_ioContext, [this, message = std::move(message)]() mutable {
		for (const auto& component : m_components)
			component->onComponentLog(message.get());
	});
}
