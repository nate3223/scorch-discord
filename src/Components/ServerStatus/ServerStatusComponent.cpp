#include "ServerStatusComponent.hpp"
#include "ServerStatusComponent_p.hpp"

#include "Agents/AgentsManager.hpp"
#include "Database/MongoDB/MongoDBManager.hpp"
#include "Log.hpp"
#include "Server.hpp"
#include "ServerConfig.hpp"

#include <scorch/server/Agent.hpp>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <ctime>
#include <dpp/unicode_emoji.h>
#include <format>
#include <functional>
#include <regex>
#include <thread>

namespace
{
	namespace SetStatusChannel
	{
		constexpr auto Channel		= "channel";
	}

	namespace ServerSelect
	{
		constexpr auto MenuOption	= "ServerStatusSelectMenuOption";
		constexpr auto MaxServers	= 25;
	}

	namespace AddServer
	{
		constexpr auto Command		= "addserver";
		constexpr auto Button		= "AddServerButton";
		constexpr auto Form			= "AddServerModal";
		constexpr auto ServerName	= "AddServerModalServerName";
		constexpr auto Address		= "AddServerModalAddress";
		constexpr auto URL			= "AddServerURL";
		constexpr auto Emoji		= "AddServerModalEmoji";
		constexpr auto Emojis		= { dpp::unicode_emoji::globe_with_meridians, dpp::unicode_emoji::rainbow_flag, dpp::unicode_emoji::tada, dpp::unicode_emoji::clown, dpp::unicode_emoji::hearts, dpp::unicode_emoji::sauropod, dpp::unicode_emoji::goose, dpp::unicode_emoji::four_leaf_clover, dpp::unicode_emoji::lotus, dpp::unicode_emoji::star2, dpp::unicode_emoji::snowflake, dpp::unicode_emoji::fire, dpp::unicode_emoji::hamburger, dpp::unicode_emoji::champagne_glass, dpp::unicode_emoji::island, dpp::unicode_emoji::rocket, dpp::unicode_emoji::night_with_stars, dpp::unicode_emoji::gem, dpp::unicode_emoji::pushpin, dpp::unicode_emoji::mirror_ball, dpp::unicode_emoji::lock, dpp::unicode_emoji::heart_on_fire, dpp::unicode_emoji::sos, dpp::unicode_emoji::warning };
		constexpr auto NoChannel	= "You must set a status channel before adding a server!";
	}

	namespace RemoveServer
	{
		constexpr auto Command		= "removeserver";
		constexpr auto Button		= "RemoveServerButton";
		constexpr auto SelectOption	= "RemoveServerSelectOption";
		constexpr auto Placeholder	= "Select servers to remove";
		constexpr auto NoChannel	= "You must set a status channel before removing a server!";
	}

	namespace ServerStatusWidget
	{
		constexpr auto WidgetSettings	= "StatusWidgetSettingsButton";
		constexpr auto QueryServer		= "StatusWidgetQueryServerOption";
		constexpr auto PinnedServer		= "StatusWidgetSettingPinnedServerOption";
		constexpr auto NoChannel		= "No status channel is set for this guild. You shouldn't be able to press this.";
	}

	namespace ServerSettings
	{
		constexpr auto Button				= "ServerSettingsButton";
		constexpr auto AddCustomButton		= "ServerSettingsAddCustomButton";
		constexpr auto RemoveCustomButton	= "ServerSettingsRemoveCustomButton";
	}

	constexpr auto kButtonServerParseError	= "Could not parse the server ID from the button!";
	constexpr auto kButtonMissingServer		= "Could not find the (possibly deleted) server corresponding to that button!";
	constexpr auto kAgentStatusUpdateInterval = std::chrono::seconds(30);
}

ServerStatusComponent::ServerStatusComponent(DiscordBot& bot)
	: Component(bot)
	, m_p(std::make_unique<ServerStatusComponentPrivate>(
		AgentsManager::Instance(),
		DatabaseManager::GetInstance<MongoDBManager>()->getPool()
	))
{
	// Server Status Channel Widget
	m_slashCommands.emplace_back(
		SlashCommand::TaskHandler{[this](const dpp::slashcommand_t& event) -> dpp::task<void> {
			co_await onSetStatusChannel(event);
		}},
		dpp::slashcommand("setstatuschannel", "Sets the channel where the server status is displayed.", m_bot->me.id)
			.add_option(
				dpp::command_option(dpp::co_channel, SetStatusChannel::Channel, "Channel to display server status", true)
					.add_channel_type(dpp::CHANNEL_TEXT)
			)
			.set_default_permissions(0)
	);
	
	// Status widget items
	m_buttonCommands.emplace_back(
		Server::CustomButton::ButtonPrefix,
		std::bind_front(&ServerStatusComponent::onServerCustomButton, this),
		MatchType::PREFIX
	);
	m_buttonCommands.emplace_back(
		ServerStatusWidget::WidgetSettings,
		std::bind_front(&ServerStatusComponent::onWidgetSettingsButton, this)
	);
	m_selectCommands.emplace_back(
		ServerStatusWidget::PinnedServer,
		std::bind_front(&ServerStatusComponent::onPinnedServerSelect, this)
	);
	m_selectCommands.emplace_back(
		ServerStatusWidget::QueryServer,
		std::bind_front(&ServerStatusComponent::onSelectQueryServer, this)
	);

	// Server query items
	m_buttonCommands.emplace_back(
		Server::Settings::ButtonPrefix,
		std::bind_front(&ServerStatusComponent::onServerSettingsButton, this),
		MatchType::PREFIX
	);

	// Server settings items
	m_buttonCommands.emplace_back(
		Server::AddCustomButton::ButtonPrefix,
		std::bind_front(&ServerStatusComponent::onAddCustomServerButtonButton, this),
		MatchType::PREFIX
	);
	m_formCommands.emplace_back(
		Server::AddCustomButton::FormPrefix,
		std::bind_front(&ServerStatusComponent::onAddCustomServerButtonForm, this),
		MatchType::PREFIX
	);
	m_buttonCommands.emplace_back(
		Server::RemoveCustomButton::ButtonPrefix,
		std::bind_front(&ServerStatusComponent::onRemoveCustomServerButtonButton, this),
		MatchType::PREFIX
	);
	m_selectCommands.emplace_back(
		Server::RemoveCustomButton::OptionPrefix,
		std::bind_front(&ServerStatusComponent::onRemoveCustomServerButtonSelect, this),
		MatchType::PREFIX
	);

	// Widget settings items
	m_buttonCommands.emplace_back(
		AddServer::Button,
		std::bind_front(&ServerStatusComponent::onAddServerButton, this)
	);
	m_buttonCommands.emplace_back(
		RemoveServer::Button,
		std::bind_front(&ServerStatusComponent::onRemoveServerButton, this)
	);
	m_slashCommands.emplace_back(
		std::bind_front(&ServerStatusComponent::onAddServerCommand, this),
		dpp::slashcommand(AddServer::Command, "Adds a server to be tracked.", m_bot->me.id)
			.set_default_permissions(0)
	);
	m_formCommands.emplace_back(
		AddServer::Form,
		std::bind_front(&ServerStatusComponent::onAddServerForm, this)
	);
	m_slashCommands.emplace_back(
		std::bind_front(&ServerStatusComponent::onRemoveServerCommand, this),
		dpp::slashcommand(RemoveServer::Command, "Removes tracked servers", m_bot->me.id)
			.set_default_permissions(0)
	);
	m_selectCommands.emplace_back(
		RemoveServer::SelectOption,
		std::bind_front(&ServerStatusComponent::onRemoveServerSelect, this)
	);

	{
		auto client = m_p->m_databasePool.acquire();
		for (auto& server : Server::FindAll(*client))
		{
			const auto id = server->m_id;
			Servers::store(id, std::move(server));
		}
		for (auto& config : ServerConfig::FindAll(*client))
		{	
			const auto guildID = config->guildId();
			if (config->statusWidget().activeServerID())
				config->statusWidget().setActiveServer(Servers::find(*config->statusWidget().activeServerID()));
			ServerConfigs::store(guildID, std::move(config));
		}
	}

	m_p->m_agentStatusSubscription = m_p->m_agentsManager.subscribeToAgentStatus(
		[this](std::string_view guildId, scorch::server::Agent agent)
		{
			updateAgentStatusWidget(guildId, agent);
		}
	);

	m_p->m_agentStatusTimer = m_bot->start_timer(
		[this]([[maybe_unused]] dpp::timer timer) -> dpp::task<void> {
			if (m_p->m_agentStatusUpdateRunning.exchange(true))
				co_return;

			try
			{
				co_await updateAgentStatusWidgets();
			}
			catch (const std::exception& error)
			{
				Logger::App().error("Failed to update agent status widgets: {}", error.what());
			}

			m_p->m_agentStatusUpdateRunning.store(false);
		},
		kAgentStatusUpdateInterval.count()
	);
}

ServerStatusComponent::~ServerStatusComponent()
{
	m_p->m_agentStatusSubscription.reset();
	m_bot->stop_timer(m_p->m_agentStatusTimer);
}

dpp::task<void> ServerStatusComponent::onSetStatusChannel(const dpp::slashcommand_t& event)
{
	event.thinking(true);
	const dpp::snowflake channel = std::get<dpp::snowflake>(
		event.get_parameter(SetStatusChannel::Channel)
	);
	const auto channelResponse = co_await m_bot->co_channel_get(channel);
	if (channelResponse.is_error())
	{
		co_await event.co_edit_original_response(
			dpp::message("Cannot see channel. Try checking the channel permissions.")
		);
		co_return;
	}

	const uint64_t guild = event.command.guild_id;
	const uint64_t commandID = event.command.id;
	uint64_t previousChannel = 0;
	std::optional<uint64_t> previousAgentMessageID;
	std::optional<uint64_t> previousServerMessageID;
	std::shared_ptr<ServerConfig> config = ServerConfigs::find(guild);
	if (! config)
	{
		config = std::make_shared<ServerConfig>();
		{
			std::unique_lock lock(config->mutex());
			config->setGuildId(guild);
			config->setChannelId(channel);
			config->statusWidget().setCommandID(commandID);

			auto client = m_p->m_databasePool.acquire();
			config->insertIntoDatabase(*client);
		}
		ServerConfigs::store(guild, config);
	}
	else
	{
		std::unique_lock lock(config->mutex());
		previousChannel = config->channelId();
		previousAgentMessageID = config->statusWidget().agentMessageID();
		previousServerMessageID = config->statusWidget().messageID();

		config->setChannelId(channel);
		config->statusWidget().agentMessageID().reset();
		config->statusWidget().messageID().reset();
		config->statusWidget().setCommandID(commandID);

		auto client = m_p->m_databasePool.acquire();
		config->updateChannelID(*client);
		config->updateStatusWidget(*client);
	}

	if (previousAgentMessageID)
		co_await m_bot->co_message_delete(*previousAgentMessageID, previousChannel);
	if (previousServerMessageID)
		co_await m_bot->co_message_delete(*previousServerMessageID, previousChannel);

	const auto agent = m_p->m_agentsManager.connectedAgent(std::to_string(guild));
	dpp::message agentWidget;
	{
		std::shared_lock lock(config->mutex());
		agentWidget = getAgentStatusWidget(*config, agent);
	}

	const auto agentResponse = co_await m_bot->co_message_create(agentWidget);
	if (agentResponse.is_error())
	{
		{
			std::unique_lock lock(config->mutex());
			if (config->statusWidget().commandID() == commandID)
				config->statusWidget().commandID().reset();
		}
		co_await event.co_edit_original_response(
			dpp::message("Could not create the agent status widget.")
		);
		co_return;
	}

	const dpp::message agentMessage = agentResponse.get<dpp::message>();
	dpp::message serverWidget;
	{
		std::unique_lock lock(config->mutex());
		if (config->statusWidget().commandID() != commandID)
		{
			m_bot->message_delete(agentMessage.id, agentMessage.channel_id);
			co_return;
		}

		config->statusWidget().agentMessageID() = agentMessage.id;
		serverWidget = getServerStatusWidget(*config);
	}

	const auto serverResponse = co_await m_bot->co_message_create(serverWidget);
	if (serverResponse.is_error())
	{
		m_bot->message_delete(agentMessage.id, agentMessage.channel_id);
		{
			std::unique_lock lock(config->mutex());
			if (config->statusWidget().commandID() == commandID)
			{
				config->statusWidget().agentMessageID().reset();
				config->statusWidget().commandID().reset();
			}
		}
		co_await event.co_edit_original_response(
			dpp::message("Could not create the server status widget.")
		);
		co_return;
	}

	const dpp::message serverMessage = serverResponse.get<dpp::message>();
	{
		std::unique_lock lock(config->mutex());
		if (config->statusWidget().commandID() != commandID)
		{
			m_bot->message_delete(agentMessage.id, agentMessage.channel_id);
			m_bot->message_delete(serverMessage.id, serverMessage.channel_id);
			co_return;
		}

		config->statusWidget().messageID() = serverMessage.id;
		config->statusWidget().commandID().reset();

		auto client = m_p->m_databasePool.acquire();
		config->updateStatusWidget(*client);
	}

	const std::string reply = std::format(
		"Server status channel changed to <#{}>",
		channel.str()
	);
	co_await event.co_edit_original_response(dpp::message(reply));

	auto logMessage = std::make_unique<GuildEmbedMessage>(reply, guild);
	logMessage->user = event.command.usr;
	m_bot.componentLog(std::move(logMessage));
}

void ServerStatusComponent::onAddServerCommand(const dpp::slashcommand_t& event)
{
	const auto guild = (uint64_t)event.command.guild_id;
	if (! ServerConfigs::contains(guild))
	{
		event.reply(dpp::message(AddServer::NoChannel).set_flags(dpp::m_ephemeral));
		return;
	}

	event.dialog(getAddServerModal());
}

void ServerStatusComponent::onAddServerButton(const dpp::button_click_t& event)
{
	const auto guild = (uint64_t)event.command.guild_id;

	if (! ServerConfigs::contains(guild))
	{
		event.reply(dpp::message(RemoveServer::NoChannel).set_flags(dpp::m_ephemeral));
		return;
	}

	dpp::channel* channel = dpp::find_channel(event.command.channel_id);
	if (channel == nullptr || !channel->get_user_permissions(&event.command.usr).can(dpp::p_administrator))
	{
		event.reply(dpp::message("You do not have permission to add servers!").set_flags(dpp::m_ephemeral));
		return;
	}

	event.dialog(getAddServerModal());
}

void ServerStatusComponent::onAddServerForm(const dpp::form_submit_t& event)
{
	const auto guild = (uint64_t)event.command.guild_id;

	std::shared_ptr<ServerConfig> config;
	if (config = ServerConfigs::find(guild), !config)
	{
		event.reply(dpp::message(AddServer::NoChannel).set_flags(dpp::m_ephemeral));
		return;
	}

	std::unique_lock lock(config->mutex());

	dpp::channel* channel = dpp::find_channel(event.command.channel_id);
	if (channel == nullptr || !channel->get_user_permissions(&event.command.usr).can(dpp::p_administrator))
	{
		event.reply(dpp::message("You do not have permission to add servers!").set_flags(dpp::m_ephemeral));
		return;
	}
	
	if (config->serverIds().size() >= ServerSelect::MaxServers)
	{
		event.reply(dpp::message(std::format("Exceeded the maximum number of servers ({})!", ServerSelect::MaxServers)).set_flags(dpp::m_ephemeral));
		return;
	}

	const auto id = (uint64_t)event.command.id;
	const std::string& serverName = std::get<std::string>(event.components[0].value);
	const std::string& address = std::get<std::string>(event.components[1].value);
	const std::string& url = std::get <std::string>(event.components[2].value);

	auto server = std::make_shared<Server>(id, serverName, address, guild, url);
	Servers::store(id, server);

	config->serverIds().push_back(id);

	{
		auto client = m_p->m_databasePool.acquire();
		config->updateServerIDs(*client);
		server->insertIntoDatabase(*client);
	}

	updateServerStatusWidget(*config);

	event.reply(dpp::message("Server added successfully!").set_flags(dpp::m_ephemeral));
	auto logMessage = std::make_unique<GuildEmbedMessage>("Added new server", config->guildId());
	logMessage->user = event.command.usr;
	logMessage->fields.emplace_back("Name", serverName);
	logMessage->fields.emplace_back("Address", address);
	m_bot.componentLog(std::move(logMessage));
}

void ServerStatusComponent::onRemoveServerCommand(const dpp::slashcommand_t& event)
{
	const auto guild = (uint64_t)event.command.guild_id;

	std::shared_ptr<ServerConfig> config;
	if (config = ServerConfigs::find(guild); !config)
	{
		event.reply(dpp::message(RemoveServer::NoChannel).set_flags(dpp::m_ephemeral));
		return;
	}

	std::shared_lock lock(config->mutex());

	if (config->serverIds().empty())
	{
		event.reply(dpp::message("No servers to remove!").set_flags(dpp::m_ephemeral));
		return;
	}

	event.reply(dpp::message()
		.set_flags(dpp::m_ephemeral)
		.add_component(getRemoveServerComponent(*config))
	);
}

void ServerStatusComponent::onRemoveServerButton(const dpp::button_click_t& event)
{
	const auto guild = (uint64_t)event.command.guild_id;

	std::shared_ptr<ServerConfig> config;
	if (config = ServerConfigs::find(guild); !config)
	{
		event.reply(dpp::message(RemoveServer::NoChannel).set_flags(dpp::m_ephemeral));
		return;
	}

	std::shared_lock lock(config->mutex());

	dpp::channel* channel = dpp::find_channel(event.command.channel_id);
	if (channel == nullptr || !channel->get_user_permissions(&event.command.usr).can(dpp::p_administrator))
	{
		event.reply(dpp::message("You do not have permission to remove servers!").set_flags(dpp::m_ephemeral));
		return;
	}

	if (config->serverIds().empty())
	{
		event.reply(dpp::message("No servers to remove!").set_flags(dpp::m_ephemeral));
		return;
	}

	event.reply(dpp::message()
		.set_flags(dpp::m_ephemeral)
		.add_component(getRemoveServerComponent(*config))
	);
}

void ServerStatusComponent::onRemoveServerSelect(const dpp::select_click_t& event)
{
	std::vector<uint64_t> activeServers;
	std::vector<std::string> serversToDelete(event.values);
	std::string deletedServers;
	const auto guild = event.command.guild_id;
	
	{
		std::shared_ptr<ServerConfig> config;
		if (config = ServerConfigs::find(guild); !config)
		{
			event.reply(dpp::message(RemoveServer::NoChannel).set_flags(dpp::m_ephemeral));
			return;
		}

		std::unique_lock lock(config->mutex());

		dpp::channel* channel = dpp::find_channel(event.command.channel_id);
		if (channel == nullptr || !channel->get_user_permissions(&event.command.usr).can(dpp::p_administrator))
		{
			event.reply(dpp::message("You do not have permission to remove servers!").set_flags(dpp::m_ephemeral));
			return;
		}

		bool isPinnedRemoved = false;
		std::vector<uint64_t> deletedIDs(event.values.size());
		std::copy_if(config->serverIds().begin(), config->serverIds().end(), std::back_inserter(activeServers), [&serversToDelete, &deletedServers, config, &isPinnedRemoved, &deletedIDs, this](const auto serverID){
			if (const auto it = std::find(serversToDelete.begin(), serversToDelete.end(), std::to_string(serverID)); it != serversToDelete.end())
			{
				if (config->statusWidget().activeServerID() && serverID == *config->statusWidget().activeServerID())
					isPinnedRemoved = true;
					
				serversToDelete.erase(it);
				deletedIDs.push_back(serverID);
				deletedServers += std::format("  - {}\n", Servers::find(serverID)->m_name);
				return false;
			}
		
			return true;
		});

		config->setServerIds(std::move(activeServers));

		{
			auto client = m_p->m_databasePool.acquire();
			config->updateServerIDs(*client);
			if (isPinnedRemoved)
			{
				config->statusWidget().activeServerID().reset();
				config->statusWidget().setActiveServer(nullptr);
				config->updateStatusWidget(*client);
			}
			Server::BulkRemoveFromDatabase(deletedIDs, *client);
			Servers::bulkRemove(deletedIDs);
		}
		
		updateServerStatusWidget(*config);
	}

	std::string notFoundServers;
	for (const auto& server : serversToDelete)
	{
		if (const auto it = std::find_if(event.command.msg.components[0].options.begin(), 
										 event.command.msg.components[0].options.end(),
										 [server](const dpp::select_option& option) { return option.value == server; });
			it != event.command.msg.components[0].options.end()
		)
			notFoundServers += std::format("  - {}\n", it->label);
	}

	std::string message("```\n");
	if (!deletedServers.empty())
		message += std::format("Removed servers:\n{}", deletedServers);
	if (!notFoundServers.empty())
		message += std::format("Could not find the following (possibly deleted) servers:\n{}", notFoundServers);
	message += "```";

	event.reply(dpp::message(message).set_flags(dpp::m_ephemeral));
	auto logMessage = std::make_unique<GuildEmbedMessage>(message, guild);
	logMessage->user = event.command.usr;
	m_bot.componentLog(std::move(logMessage));
}

void ServerStatusComponent::onServerCustomButton(const dpp::button_click_t& event)
{
	const auto guild = (uint64_t)event.command.guild_id;

	std::shared_ptr<ServerConfig> config;
	if (config = ServerConfigs::find(guild); !config)
	{
		event.reply(dpp::message(ServerStatusWidget::NoChannel).set_flags(dpp::m_ephemeral));
		return;
	}

	std::shared_lock lock(config->mutex());

	std::smatch matches;
	const auto serverID = Server::ParseServerIDFromComponentID(event.custom_id, Server::CustomButton::ButtonPattern, matches);
	if (!serverID.has_value())
	{
		event.reply(dpp::message(kButtonServerParseError).set_flags(dpp::m_ephemeral));
		return;
	}

	std::shared_ptr<Server> server;
	if (server = Servers::find(*serverID); !server)
	{
		event.reply(dpp::message(kButtonMissingServer).set_flags(dpp::m_ephemeral));
		return;
	}

	auto serverButton = server->getServerButton(matches);
	if (!serverButton.has_value())
	{
		event.reply(dpp::message("Could not find the (possibly deleted) button!").set_flags(dpp::m_ephemeral));
		return;
	}

	m_bot->request(serverButton->endpoint(), dpp::m_get, [event](const dpp::http_request_completion_t& callback) {
		if (callback.status < 200 || callback.status >= 300)
		{
			return;
		}

		event.reply();
	});
}

void ServerStatusComponent::onWidgetSettingsButton(const dpp::button_click_t& event)
{
	const auto guild = (uint64_t)event.command.guild_id;

	std::shared_ptr<ServerConfig> config;
	if (config = ServerConfigs::find(guild); !config)
	{
		event.reply(dpp::message(ServerStatusWidget::NoChannel).set_flags(dpp::m_ephemeral));
		return;
	}

	std::shared_lock lock(config->mutex());

	dpp::channel* channel = dpp::find_channel(event.command.channel_id);
	if (channel == nullptr || !channel->get_user_permissions(&event.command.usr).can(dpp::p_administrator))
	{
		event.reply(dpp::message("You do not have permission to change the server status widget!").set_flags(dpp::m_ephemeral));
		return;
	}

	dpp::message msg = dpp::message().set_flags(dpp::m_ephemeral);
	if (!config->serverIds().empty())
	{
		msg.add_component(
			dpp::component().add_component(
				getServerSelectMenuComponent(*config)
				.set_id(ServerStatusWidget::PinnedServer)
				.set_placeholder("Select a server to pin")
			)
		);
	}
	msg.add_component(
		dpp::component().add_component(
			dpp::component()
				.set_label("Add server")
				.set_id(AddServer::Button)
				.set_type(dpp::cot_button)
		).add_component(
			dpp::component()
				.set_label("Remove servers")
				.set_id(RemoveServer::Button)
				.set_type(dpp::cot_button)
		)
	);
	event.reply(msg);
}

void ServerStatusComponent::onPinnedServerSelect(const dpp::select_click_t& event)
{
	const auto guild = (uint64_t)event.command.guild_id;

	std::shared_ptr<ServerConfig> config;
	if (config = ServerConfigs::find(guild); !config)
	{
		event.reply(dpp::message("You must set a status channel before selecting a pinned server!").set_flags(dpp::m_ephemeral));
		return;
	}

	std::unique_lock lock(config->mutex());

	dpp::channel* channel = dpp::find_channel(event.command.channel_id);
	if (channel == nullptr || !channel->get_user_permissions(&event.command.usr).can(dpp::p_administrator))
	{
		event.reply(dpp::message("You do not have permission to change the pinned server!").set_flags(dpp::m_ephemeral));
		return;
	}

	if (config->statusWidget().commandID().has_value())
	{
		event.reply(dpp::message("Server status widget is still building, please try again!").set_flags(dpp::m_ephemeral));
		return;
	}

	uint64_t serverID;
	try
	{
		serverID = std::stoull(event.values[0]);
	}
	catch (const std::exception& e)
	{
		event.reply(dpp::message("Failed to parse server ID!").set_flags(dpp::m_ephemeral));
		return;
	}

	if (config->statusWidget().activeServerID() && serverID == *config->statusWidget().activeServerID())
	{
		event.reply(dpp::message("Server is already pinned!").set_flags(dpp::m_ephemeral));
		return;
	}

	std::shared_ptr<Server> server;
	if (server = Servers::find(serverID); !server)
	{
		event.reply(dpp::message("Could not find selected server, it was probably deleted!").set_flags(dpp::m_ephemeral));
		return;
	}

	config->statusWidget().setActiveServerID(server->m_id);
	config->statusWidget().setActiveServer(server);

	{
		auto client = m_p->m_databasePool.acquire();
		config->updateStatusWidget(*client);
	}

	updateServerStatusWidget(*config);

	event.reply();
}

void ServerStatusComponent::onSelectQueryServer(const dpp::select_click_t& event)
{
	if (event.values.empty())
	{
		event.reply();
		return;
	}

	const auto guild = (uint64_t)event.command.guild_id;

	std::shared_ptr<ServerConfig> config;
	if (config = ServerConfigs::find(guild); !config)
	{
		event.reply(dpp::message("You must set a status channel before querying a server!").set_flags(dpp::m_ephemeral));
		return;
	}

	std::shared_lock lock(config->mutex());

	uint64_t serverID;
	try
	{
		serverID = std::stoull(event.values[0]);
	}
	catch (const std::exception& e)
	{
		event.reply(dpp::message("Failed to parse server ID!").set_flags(dpp::m_ephemeral));
		return;
	}

	std::shared_ptr<Server> server;
	if (server = Servers::find(serverID); !server)
	{
		event.reply(dpp::message("Could not find selected server, it was probably deleted!").set_flags(dpp::m_ephemeral));
		return;
	}

	dpp::message message = dpp::message();
	message.add_embed(server->getEmbed());
	for (const auto& buttonRow : server->getButtonRows())
		message.add_component(buttonRow);

	dpp::component settingsButton = server->getSettingsButton();

	if (message.components.empty() || message.components.back().components.size() % Server::CustomButton::ButtonsPerRow == 0)
		message.add_component(dpp::component().add_component(std::move(settingsButton)));
	else
		message.components.back().add_component(std::move(settingsButton));

	event.reply(message.set_flags(dpp::m_ephemeral));
}

void ServerStatusComponent::onServerSettingsButton(const dpp::button_click_t& event)
{
	const auto guild = (uint64_t)event.command.guild_id;

	std::shared_ptr<ServerConfig> config;
	if (config = ServerConfigs::find(guild); !config)
	{
		event.reply(dpp::message("You must set a status channel before changing a server's settings!").set_flags(dpp::m_ephemeral));
		return;
	}

	std::shared_lock lock(config->mutex());

	dpp::channel* channel = dpp::find_channel(event.command.channel_id);
	if (channel == nullptr || !channel->get_user_permissions(&event.command.usr).can(dpp::p_administrator))
	{
		event.reply(dpp::message("You do not have permission to view the server settings!").set_flags(dpp::m_ephemeral));
		return;
	}

	std::smatch matches;
	const auto serverID = Server::ParseServerIDFromComponentID(event.custom_id, Server::Settings::ButtonPattern, matches);
	if (!serverID.has_value())
	{
		event.reply(dpp::message(kButtonServerParseError).set_flags(dpp::m_ephemeral));
		return;
	}

	std::shared_ptr<Server> server;
	if (server = Servers::find(*serverID); !server)
	{
		event.reply(dpp::message(kButtonMissingServer));
		return;
	}

	dpp::message message = dpp::message();
	for (const auto& buttonRow : server->getServerSettingsRows())
		message.add_component(buttonRow);
	event.reply(message.set_flags(dpp::m_ephemeral));
}

void ServerStatusComponent::onAddCustomServerButtonButton(const dpp::button_click_t& event)
{
	const auto guild = (uint64_t)event.command.guild_id;

	std::shared_ptr<ServerConfig> config;
	if (config = ServerConfigs::find(guild); !config)
	{
		event.reply(dpp::message("You must set a status channel before adding a custom server button!").set_flags(dpp::m_ephemeral));
		return;
	}

	std::shared_lock lock(config->mutex());

	dpp::channel* channel = dpp::find_channel(event.command.channel_id);
	if (channel == nullptr || !channel->get_user_permissions(&event.command.usr).can(dpp::p_administrator))
	{
		event.reply(dpp::message("You do not have permission to add server buttons!").set_flags(dpp::m_ephemeral));
		return;
	}

	std::smatch matches;
	const auto serverID = Server::ParseServerIDFromComponentID(event.custom_id, Server::AddCustomButton::ButtonPattern, matches);
	if (!serverID.has_value())
	{
		event.reply(dpp::message(kButtonServerParseError).set_flags(dpp::m_ephemeral));
		return;
	}

	std::shared_ptr<Server> server;
	if (server = Servers::find(*serverID); !server)
	{
		event.reply(dpp::message(kButtonMissingServer).set_flags(dpp::m_ephemeral));
		return;
	}

	if (server->m_buttons.size() >= Server::CustomButton::MaxButtons)
	{
		event.reply(dpp::message(std::format("Exceeded the number of maximum custom buttons ({})!", Server::CustomButton::MaxButtons)).set_flags(dpp::m_ephemeral));
		return;
	}

	event.dialog(server->getAddCustomButtonModal());
}

void ServerStatusComponent::onAddCustomServerButtonForm(const dpp::form_submit_t& event)
{
	const auto guild = (uint64_t)event.command.guild_id;

	std::shared_ptr<ServerConfig> config;
	if (config = ServerConfigs::find(guild); !config)
	{
		event.reply(dpp::message("You must set a status channel before add custom server buttons!").set_flags(dpp::m_ephemeral));
		return;
	}

	std::unique_lock lock(config->mutex());

	dpp::channel* channel = dpp::find_channel(event.command.channel_id);
	if (channel == nullptr || !channel->get_user_permissions(&event.command.usr).can(dpp::p_administrator))
	{
		event.reply(dpp::message("You do not have permission to add server buttons!").set_flags(dpp::m_ephemeral));
		return;
	}

	std::smatch matches;
	const auto serverID = Server::ParseServerIDFromComponentID(event.custom_id, Server::AddCustomButton::FormPattern, matches);
	if (!serverID.has_value())
	{
		event.reply(dpp::message("Could not parse server ID from custom button form!").set_flags(dpp::m_ephemeral));
		return;
	}

	std::shared_ptr<Server> server;
	if (server = Servers::find(*serverID); !server)
	{
		event.reply(dpp::message("Could not find the (possibly deleted) server corresponding to that form").set_flags(dpp::m_ephemeral));
		return;
	}

	if (server->m_buttons.size() >= Server::CustomButton::MaxButtons)
	{
		event.reply(dpp::message(std::format("Exceeded the number of maximum custom buttons {}!", Server::CustomButton::MaxButtons)).set_flags(dpp::m_ephemeral));
		return;
	}

	const uint64_t id = (uint64_t)event.command.id;
	const std::string& buttonName = std::get<std::string>(event.components[0].value);
	const std::string& endpoint = std::get<std::string>(event.components[1].value);

	server->m_buttons.emplace_back(id, buttonName, endpoint, server->m_id);

	{
		auto client = m_p->m_databasePool.acquire();
		server->updateCustomButtons(*client);
	}

	updateServerStatusWidget(*config);

	event.reply(dpp::message("Custom button added successfully!").set_flags(dpp::m_ephemeral));
	auto logMessage = std::make_unique<GuildEmbedMessage>("Added new custom button", config->guildId());
	logMessage->user = event.command.usr;
	logMessage->fields.emplace_back("Server", server->m_name);
	logMessage->fields.emplace_back("Label", buttonName);
	logMessage->fields.emplace_back("Endpoint", endpoint);
	m_bot.componentLog(std::move(logMessage));
}

void ServerStatusComponent::onRemoveCustomServerButtonButton(const dpp::button_click_t& event)
{
	const auto guild = (uint64_t)event.command.guild_id;

	std::shared_ptr<ServerConfig> config;
	if (config = ServerConfigs::find(guild); !config)
	{
		event.reply(dpp::message("You must set a status channel before removing custom server buttons!").set_flags(dpp::m_ephemeral));
		return;
	}

	std::shared_lock lock(config->mutex());

	dpp::channel* channel = dpp::find_channel(event.command.channel_id);
	if (channel == nullptr || !channel->get_user_permissions(&event.command.usr).can(dpp::p_administrator))
	{
		event.reply(dpp::message("You do not have permission to remove server buttons!").set_flags(dpp::m_ephemeral));
		return;
	}

	std::smatch matches;
	const auto serverID = Server::ParseServerIDFromComponentID(event.custom_id, Server::RemoveCustomButton::ButtonPattern, matches);
	if (!serverID.has_value())
	{
		event.reply(dpp::message(kButtonServerParseError).set_flags(dpp::m_ephemeral));
		return;
	}

	std::shared_ptr<Server> server;
	if (server = Servers::find(*serverID); !server)
	{
		event.reply(dpp::message(kButtonMissingServer).set_flags(dpp::m_ephemeral));
		return;
	}

	if (server->m_buttons.empty())
	{
		event.reply(dpp::message("No custom buttons to remove!").set_flags(dpp::m_ephemeral));
		return;
	}

	event.reply(dpp::message()
		.add_component(server->getRemoveCustomButtonComponent())
		.set_flags(dpp::m_ephemeral)
	);
}

void ServerStatusComponent::onRemoveCustomServerButtonSelect(const dpp::select_click_t& event)
{
	const auto guild = (uint64_t)event.command.guild_id;
	std::vector<ServerButton> activeButtons;
	std::vector<std::string> buttonsToDelete(event.values);
	std::string deletedButtons;
	std::shared_ptr<Server> server;

	std::shared_ptr<ServerConfig> config;
	if (config = ServerConfigs::find(guild); !config)
	{
		event.reply(dpp::message("You must set a status channel before removing server buttons!").set_flags(dpp::m_ephemeral));
		return;
	}

	{
		std::unique_lock lock(config->mutex());

		dpp::channel* channel = dpp::find_channel(event.command.channel_id);
		if (channel == nullptr || !channel->get_user_permissions(&event.command.usr).can(dpp::p_administrator))
		{
			event.reply(dpp::message("You do not have permission to remove server buttons!").set_flags(dpp::m_ephemeral));
			return;
		}

		std::smatch matches;
		const auto serverID = Server::ParseServerIDFromComponentID(event.custom_id, Server::RemoveCustomButton::OptionPattern, matches);
		if (!serverID.has_value())
		{
			event.reply(dpp::message("Could not parse server ID from the select option!").set_flags(dpp::m_ephemeral));
			return;
		}

		if (server = Servers::find(*serverID); !server)
		{
			event.reply(dpp::message("Could not find the (possibly deleted) server corresponding to that select menu!"));
			return;
		}

		std::vector<uint64_t> deletedIDs(event.values.size());
		std::copy_if(server->m_buttons.begin(), server->m_buttons.end(), std::back_inserter(activeButtons), [&buttonsToDelete, &deletedButtons, config, &deletedIDs, this](const ServerButton& button) {
			if (const auto it = std::find(buttonsToDelete.begin(), buttonsToDelete.end(), std::to_string(button.id())); it != buttonsToDelete.end())
			{
				buttonsToDelete.erase(it);
				deletedIDs.push_back(button.id());
				deletedButtons += std::format("  - {}\n", button.name());
				return false;
			}

			return true;
			});

		server->m_buttons = std::move(activeButtons);

		{
			auto client = m_p->m_databasePool.acquire();
			server->updateCustomButtons(*client);
		}

		updateServerStatusWidget(*config);
	}

	std::string notFoundButtons;
	for (const auto& button : buttonsToDelete)
	{
		if (const auto it = std::find_if(event.command.msg.components[0].options.begin(),
			event.command.msg.components[0].options.end(),
			[button](const dpp::select_option& option) { return option.value == button; });
			it != event.command.msg.components[0].options.end()
			)
			notFoundButtons += std::format("  - {}\n", it->label);
	}

	std::string message("```\n");
	if (!deletedButtons.empty())
		message += std::format("Removed buttons from {}:\n{}", server->m_name, deletedButtons);
	if (!notFoundButtons.empty())
		message += std::format("Could not find the following (possibly deleted) buttons:\n{}", notFoundButtons);
	message += "```";

	event.reply(dpp::message(message).set_flags(dpp::m_ephemeral));
	auto logMessage = std::make_unique<GuildEmbedMessage>(message, guild);
	logMessage->user = event.command.usr;
	m_bot.componentLog(std::move(logMessage));
}

void ServerStatusComponent::updateServerStatusWidget(const ServerConfig& config)
{	
	// HACK:	Current dpp API does not let you modify the ID of a message, you must retrieve the message and run a callback method.
	//			We don't actually need the retrieve the message to edit the widget since we already know the channel and the ID of the message.
	//			This is ripped straight from the GitHub repository, without a callback since we don't care if it fails
	const auto widget = getServerStatusWidget(config);
	m_bot->post_rest_multipart(
		API_PATH "/channels",
		std::to_string(config.channelId()),
		"messages/" + std::to_string(*config.statusWidget().messageID()),
		dpp::m_patch,
		widget.build_json(true), [this](dpp::json& j, const dpp::http_request_completion_t& http) { },
		widget.file_data
	);
}

void ServerStatusComponent::updateAgentStatusWidget(
	std::string_view guildId,
	const scorch::server::Agent& agent
)
{
	uint64_t guild = 0;
	const auto [end, error] = std::from_chars(guildId.data(), guildId.data() + guildId.size(), guild);
	if (error != std::errc{} || end != guildId.data() + guildId.size())
	{
		Logger::App().error("Invalid guild ID '{}' for agent status update", guildId);
		return;
	}

	const auto config = ServerConfigs::find(guild);
	if (! config)
		return;

	dpp::message widget;
	{
		std::shared_lock lock(config->mutex());
		if (! config->statusWidget().agentMessageID())
			return;

		widget = getAgentStatusWidget(*config, agent);
		widget.id = *config->statusWidget().agentMessageID();
	}

	const auto messageId = widget.id;
	m_bot->message_edit(
		widget,
		[guild, messageId](const dpp::confirmation_callback_t& response)
		{
			if (response.is_error())
			{
				Logger::App().warn(
					"Failed to update agent status widget {} for guild {}: {}",
					messageId.str(),
					guild,
					response.get_error().message
				);
			}
		}
	);
}

dpp::task<void> ServerStatusComponent::updateAgentStatusWidgets()
{
	for (const auto& [guild, config] : ServerConfigs::snapshot())
	{
		dpp::message widget;
		{
			std::shared_lock lock(config->mutex());
			if (! config->statusWidget().agentMessageID())
				continue;

			const auto agent = m_p->m_agentsManager.connectedAgent(std::to_string(guild));
			widget = getAgentStatusWidget(*config, agent);
			widget.id = *config->statusWidget().agentMessageID();
		}

		const auto response = co_await m_bot->co_message_edit(widget);
		if (response.is_error())
		{
			Logger::App().warn(
				"Failed to update agent status widget {} for guild {}: {}",
				widget.id.str(),
				guild,
				response.get_error().message
			);
		}
	}
}

dpp::interaction_modal_response ServerStatusComponent::getAddServerModal()
{
	dpp::interaction_modal_response modal(AddServer::Form, "Add a server");
	modal.add_component(
		dpp::component()
		.set_label("Server name")
		.set_id(AddServer::ServerName)
		.set_type(dpp::cot_text)
		.set_text_style(dpp::text_short)
		.set_required(true)
	);

	modal.add_row();
	modal.add_component(
		dpp::component()
		.set_label("Address for API endpoint")
		.set_id(AddServer::Address)
		.set_type(dpp::cot_text)
		.set_text_style(dpp::text_short)
		.set_required(true)
	);

	modal.add_row();
	modal.add_component(
		dpp::component()
		.set_label("Advertised server connection URL")
		.set_id(AddServer::URL)
		.set_type(dpp::cot_text)
		.set_text_style(dpp::text_short)
		.set_required(true)
	);

	return modal;
}

dpp::component ServerStatusComponent::getRemoveServerComponent(const ServerConfig& config)
{
	return dpp::component().add_component(
		getServerSelectMenuComponent(config)
		.set_id(RemoveServer::SelectOption)
		.set_min_values(1)
		.set_max_values(config.serverIds().size())
		.set_placeholder(RemoveServer::Placeholder)
	);
}

dpp::component ServerStatusComponent::getServerSelectMenuComponent(const ServerConfig& config)
{
	dpp::component selectMenuComponent = dpp::component().set_type(dpp::cot_selectmenu);
	for (const auto& serverID : config.serverIds())
	{
		std::shared_ptr<Server> server;
		if (server = Servers::find(serverID); !server)
			continue;
		selectMenuComponent.add_select_option(dpp::select_option(server->m_name, std::to_string(server->m_id)));
	}
	
	return selectMenuComponent;
}

dpp::message ServerStatusComponent::getAgentStatusWidget(const ServerConfig& config, const scorch::server::Agent& agent)
{
	dpp::embed embed;
	embed.set_title("Agent Status");
	embed.set_footer("Last checked", "");
	embed.set_timestamp(std::time(nullptr));

	if (agent.isConnected())
	{
		embed.set_color(0x57F287);
		embed.add_field("Connection", "Connected", true);

		if (const auto latency = agent.latency())
		{
			embed.add_field(
				"Latency",
				std::format(
					"{:.2f} ms",
					std::chrono::duration<double, std::milli>(*latency).count()
				),
				true
			);
		}
		else
		{
			embed.add_field("Latency", "Measuring...", true);
		}
	}
	else
	{
		embed.set_color(0xED4245);
		embed.add_field("Connection", "Disconnected", true);
		embed.add_field("Latency", "Unavailable", true);
	}

	return dpp::message()
		.set_channel_id(config.channelId())
		.add_embed(std::move(embed));
}

dpp::message ServerStatusComponent::getServerStatusWidget(const ServerConfig& config)
{
	auto message = dpp::message();
	message.set_channel_id(config.channelId());

	if (config.statusWidget().activeServerID())
	{
		message.add_embed(config.statusWidget().activeServer()->getEmbed());
		for (const auto& buttonRow : config.statusWidget().activeServer()->getButtonRows())
			message.add_component(buttonRow);
	}
	else
	{
		message.add_embed(
			dpp::embed()
				.set_description("No pinned server selected!")
				.set_timestamp(time(0))
		);
	}

	dpp::component settingsButton =
		dpp::component()
			.set_label("Widget Settings")
			.set_id(ServerStatusWidget::WidgetSettings)
			.set_type(dpp::cot_button);

	if (message.components.empty() || message.components.back().components.size() % Server::CustomButton::ButtonsPerRow == 0)
		message.add_component(dpp::component().add_component(std::move(settingsButton)));
	else
		message.components.back().add_component(std::move(settingsButton));

	if (! config.serverIds().empty())
	{
		message.add_component(
			dpp::component().add_component(
			getServerSelectMenuComponent(config)
				.set_placeholder("Select a server to query")
				.set_id(ServerStatusWidget::QueryServer)
				.set_min_values(0)
			)
		);
	}

	return message;
}

dpp::task<void> ServerStatusComponent::onChannelDelete(const dpp::channel_delete_t& event)
{
	const auto guild = (uint64_t)event.deleted.guild_id;
	const auto channel = (uint64_t)event.deleted.id;
	
	std::shared_ptr<ServerConfig> config;
	if (config = ServerConfigs::find(guild); !config)
		co_return;

	std::unique_lock lock(config->mutex());

	if (config->channelId() != channel)
		co_return;

	{
		auto client = m_p->m_databasePool.acquire();
		config->removeFromDatabase(*client);
	}

	ServerConfigs::erase(guild);
	m_bot.componentLog(std::make_unique<GuildEmbedMessage>("Server status channel was deleted, removing saved server configurations!", guild));
}

dpp::task<void> ServerStatusComponent::onMessageDelete(const dpp::message_delete_t& event)
{
	const auto guild = (uint64_t)event.guild_id;

	std::shared_ptr<ServerConfig> config;
	if (config = ServerConfigs::find(guild); !config)
		co_return;

	std::unique_lock lock(config->mutex());

	if (event.channel_id != config->channelId() || config->statusWidget().commandID())
		co_return;

	const bool deletedServerWidget =
			config->statusWidget().messageID()
		&&	event.id == *config->statusWidget().messageID();

	const bool deletedAgentWidget =
			config->statusWidget().agentMessageID()
		&&	event.id == *config->statusWidget().agentMessageID();

	if (! deletedServerWidget && ! deletedAgentWidget)
		co_return;

	{
		auto client = m_p->m_databasePool.acquire();
		config->removeFromDatabase(*client);
	}

	ServerConfigs::erase(guild);
	m_bot.componentLog(std::make_unique<GuildEmbedMessage>("Server status widget was deleted, removing saved server configurations!", guild));
}

