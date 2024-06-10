#include "ServerStatusComponent.hpp"

#include <algorithm>
#include <dpp/unicode_emoji.h>
#include <format>
#include <functional>
#include <thread>
#include <chrono>

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
		constexpr auto RestartServer	= "StatusWidgetRestartServerButton";
		constexpr auto StartServer		= "StatusWidgetStartServerButton";
		constexpr auto StopServer		= "StatusWidgetStopServerButton";
		constexpr auto ServerSettings	= "StatusWidgetServerSettingsButton";
		constexpr auto QueryServer		= "StatusWidgetQueryServerOption";
		constexpr auto PinnedServer		= "StatusWidgetSettingPinnedServerOption";
		constexpr auto NoChannel		= "No status channel is set for this guild. You shouldn't be able to press this.";
	}

	namespace Database
	{
		constexpr auto Name			= "Scorch";
		constexpr auto ServerConfig	= "ServerConfig";
	}
}

ServerStatusComponent::ServerStatusComponent(DiscordBot& bot)
	: Component(bot)
	, m_databasePool(DatabaseManager::GetInstance<MongoDBManager>()->getPool())
{
	// Server Status Channel Widget
	m_slashCommands.emplace_back(
		std::bind_front(&ServerStatusComponent::onSetStatusChannel, this),
		dpp::slashcommand("setstatuschannel", "Sets the channel where the server status is displayed.", m_bot->me.id)
			.add_option(
				dpp::command_option(dpp::co_channel, SetStatusChannel::Channel, "Channel to display server status", true)
				.add_channel_type(dpp::CHANNEL_TEXT)
			)
			.set_default_permissions(0)
	);
	m_buttonCommands.emplace_back(
		ServerStatusWidget::RestartServer,
		std::bind_front(&ServerStatusComponent::onServerRestartButton, this)
	);
	m_buttonCommands.emplace_back(
		ServerStatusWidget::StartServer,
		std::bind_front(&ServerStatusComponent::onServerStartButton, this)
	);
	m_buttonCommands.emplace_back(
		ServerStatusWidget::StopServer,
		std::bind_front(&ServerStatusComponent::onServerStopButton, this)
	);
	m_buttonCommands.emplace_back(
		ServerStatusWidget::ServerSettings,
		std::bind_front(&ServerStatusComponent::onServerSettingsButton, this)
	);
	m_selectCommands.emplace_back(
		ServerStatusWidget::PinnedServer,
		std::bind_front(&ServerStatusComponent::onPinnedServerSelect, this)
	);
	m_selectCommands.emplace_back(
		ServerStatusWidget::QueryServer,
		std::bind_front(&ServerStatusComponent::onSelectQueryServer, this)
	);
	m_buttonCommands.emplace_back(
		AddServer::Button,
		std::bind_front(&ServerStatusComponent::onAddServerButton, this)
	);
	m_buttonCommands.emplace_back(
		RemoveServer::Button,
		std::bind_front(&ServerStatusComponent::onRemoveServerButton, this)
	);

	// Adding server
	m_slashCommands.emplace_back(
		std::bind_front(&ServerStatusComponent::onAddServerCommand, this),
		dpp::slashcommand(AddServer::Command, "Adds a server to be tracked.", m_bot->me.id)
			.set_default_permissions(0)
	);
	m_formCommands.emplace_back(
		AddServer::Form,
		std::bind_front(&ServerStatusComponent::onAddServerForm, this)
	);

	// Removing server
	m_slashCommands.emplace_back(
		std::bind_front(&ServerStatusComponent::onRemoveServerCommand, this),
		dpp::slashcommand(RemoveServer::Command, "Removes tracked servers", m_bot->me.id)
			.set_default_permissions(0)
	);
	m_selectCommands.emplace_back(
		RemoveServer::SelectOption,
		std::bind_front(&ServerStatusComponent::onRemoveServerSelect, this)
	);

	m_selectCommands.emplace_back(ServerSelect::MenuOption, std::bind_front(&ServerStatusComponent::onSelectServer, this));

	{
		auto client = m_databasePool.acquire();
		for (auto& doc : client->database(Database::Name)[Database::ServerConfig].find({}))
		{
			auto config = ServerConfig(doc);
			if (config.m_statusWidget.m_activeServerID.has_value())
				config.m_statusWidget.m_activeServer = std::find_if(config.m_servers.begin(), config.m_servers.end(), [serverID = config.m_statusWidget.m_activeServerID](const auto& server) { return server->m_id == *serverID; })->get();
			m_configs[config.m_guildID] = std::make_unique<ServerConfig>(std::move(config));
		}
	}
}

void ServerStatusComponent::onSetStatusChannel(const dpp::slashcommand_t& event)
{
	event.thinking(true);
	dpp::snowflake channel = std::get<dpp::snowflake>(event.get_parameter(SetStatusChannel::Channel));
	m_bot->channel_get(channel, [this, event, channel](const dpp::confirmation_callback_t& callback) {
		if (callback.is_error())
		{
			event.edit_original_response(dpp::message("Cannot see channel. Try checking the channel permissions."));
			return;
		}

		std::lock_guard guard(m_mutex);
		auto client = m_databasePool.acquire();

		dpp::snowflake guild = event.command.guild_id;

		ServerConfig* config;
		if (!m_configs.contains((uint64_t)guild))
		{
			config = new ServerConfig();
			config->m_guildID = guild;
			config->m_channelID = channel;
			client->database(Database::Name)[Database::ServerConfig].insert_one(config->getValue());
			m_configs[(uint64_t)guild]= std::unique_ptr<ServerConfig>(config);
			config->m_statusWidget = StatusWidget();
		}
		else
		{
			config = m_configs[(uint64_t)guild].get();

			// DELETE ITSELF IF THE CALLBACK COMES BACK AND THE MESSAGE ID IS GONE
			if (config->m_statusWidget.m_messageID.has_value())
				m_bot->message_delete(*config->m_statusWidget.m_messageID, config->m_channelID);

			config->m_channelID = channel;
			client->database(Database::Name)[Database::ServerConfig].update_one(
				make_document(kvp("guildID", (int64_t)config->m_guildID)),
				make_document(kvp("$set", make_document(kvp("channelID", (int64_t)config->m_channelID))))
			);
		}

		const uint64_t commandID = (uint64_t)event.command.id;
		config->m_statusWidget.m_commandID = commandID;
		m_bot->message_create(getServerStatusWidget(*config), [this, guild, commandID](const dpp::confirmation_callback_t& callback) {
			if (callback.is_error())
			{
				//abort
				return;
			}

			dpp::message msg = callback.get<dpp::message>();
			
			std::lock_guard guard(m_mutex);
			auto client = m_databasePool.acquire();

			if (!m_configs.contains(guild))
			{
				m_bot->message_delete(msg.id, msg.channel_id);
				//abort
				return;
			}

			auto& config = *m_configs[(uint64_t)guild];

			if (!config.m_statusWidget.m_commandID.has_value() || *config.m_statusWidget.m_commandID != commandID)
			{
				m_bot->message_delete(msg.id, msg.channel_id);
				//abort
				return;
			}

			config.m_statusWidget.m_messageID = (uint64_t)msg.id;
			config.m_statusWidget.m_commandID.reset();
			client->database(Database::Name)[Database::ServerConfig].update_one(
				make_document(kvp("guildID", (int64_t)config.m_guildID)),
				make_document(kvp("$set", make_document(kvp("statusWidget", config.m_statusWidget.getValue()))))
			);
		});

		std::string reply = std::format("Server status channel changed to <#{}>", channel.str());
		event.edit_original_response(dpp::message(reply));
		auto logMessage = std::make_shared<GuildEmbedMessage>(reply, guild);
		logMessage->user = event.command.usr;
		m_bot.componentLog(logMessage);
	});
}

void ServerStatusComponent::onAddServerCommand(const dpp::slashcommand_t& event)
{
	std::lock_guard guard(m_mutex);
	auto client = m_databasePool.acquire();

	const auto guildID = event.command.guild_id;
	if (!m_configs.contains(guildID))
	{
		event.reply(dpp::message(AddServer::NoChannel).set_flags(dpp::m_ephemeral));
		return;
	}

	event.dialog(getAddServerModal());
}

void ServerStatusComponent::onAddServerButton(const dpp::button_click_t& event)
{
	std::lock_guard guard(m_mutex);
	auto client = m_databasePool.acquire();

	const auto guild = event.command.guild_id;
	if (!m_configs.contains(guild))
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
	std::lock_guard guard(m_mutex);
	auto client = m_databasePool.acquire();

	const auto guild = event.command.guild_id;
	if (!m_configs.contains(guild))
	{
		event.reply(dpp::message(AddServer::NoChannel).set_flags(dpp::m_ephemeral));
		return;
	}

	dpp::channel* channel = dpp::find_channel(event.command.channel_id);
	if (channel == nullptr || !channel->get_user_permissions(&event.command.usr).can(dpp::p_administrator))
	{
		event.reply(dpp::message("You do not have permission to add servers!").set_flags(dpp::m_ephemeral));
		return;
	}
	
	ServerConfig& config = *m_configs[(uint64_t)guild];
	if (config.m_servers.size() >= ServerSelect::MaxServers)
	{
		event.reply(dpp::message(std::format("Exceeded the maximum number of servers ({})!", ServerSelect::MaxServers)).set_flags(dpp::m_ephemeral));
		return;
	}

	const auto serverName = std::get<std::string>(event.components[0].components[0].value);
	const auto address = std::get<std::string>(event.components[1].components[0].value);
		
	config.m_servers.emplace_back(std::make_unique<Server>((uint64_t)event.command.id, serverName, address));
	client->database(Database::Name)[Database::ServerConfig].update_one(
		make_document(kvp("guildID", (int64_t)config.m_guildID)),
		make_document(kvp("$set", make_document(kvp("servers", Server::ConstructArray(config.m_servers)))))
	);
	updateServerStatusWidget(config);

	event.reply(dpp::message("Server added successfully!").set_flags(dpp::m_ephemeral));
	auto logMessage = std::make_shared<GuildEmbedMessage>("Added new server", event.command.guild_id);
	logMessage->user = event.command.usr;
	logMessage->fields.emplace_back("Name", serverName);
	logMessage->fields.emplace_back("Address", address);
	m_bot.componentLog(logMessage);
}

void ServerStatusComponent::onRemoveServerCommand(const dpp::slashcommand_t& event)
{
	std::lock_guard guard(m_mutex);
	auto client = m_databasePool.acquire();

	const auto guild = event.command.guild_id;
	if (!m_configs.contains(guild))
	{
		event.reply(dpp::message(RemoveServer::NoChannel).set_flags(dpp::m_ephemeral));
		return;
	}

	ServerConfig& config = *m_configs[(uint64_t)guild];
	if (config.m_servers.empty())
	{
		event.reply(dpp::message("No servers to remove!").set_flags(dpp::m_ephemeral));
		return;
	}

	event.reply(dpp::message()
		.set_flags(dpp::m_ephemeral)
		.add_component(getRemoveServerComponent(config))
	);
}

void ServerStatusComponent::onRemoveServerButton(const dpp::button_click_t& event)
{
	std::lock_guard guard(m_mutex);
	auto client = m_databasePool.acquire();

	const auto guild = event.command.guild_id;
	if (!m_configs.contains(guild))
	{
		event.reply(dpp::message(RemoveServer::NoChannel).set_flags(dpp::m_ephemeral));
		return;
	}

	dpp::channel* channel = dpp::find_channel(event.command.channel_id);
	if (channel == nullptr || !channel->get_user_permissions(&event.command.usr).can(dpp::p_administrator))
	{
		event.reply(dpp::message("You do not have permission to remove servers!").set_flags(dpp::m_ephemeral));
		return;
	}

	ServerConfig& config = *m_configs[(uint64_t)guild];
	if (config.m_servers.empty())
	{
		event.reply(dpp::message("No servers to remove!").set_flags(dpp::m_ephemeral));
		return;
	}

	event.reply(dpp::message()
		.set_flags(dpp::m_ephemeral)
		.add_component(getRemoveServerComponent(config))
	);
}

void ServerStatusComponent::onRemoveServerSelect(const dpp::select_click_t& event)
{
	std::vector<std::unique_ptr<Server>> activeServers;
	std::vector<std::string> serversToDelete(event.values);
	std::string deletedServers;
	const auto guild = event.command.guild_id;
	
	{
		std::lock_guard guard(m_mutex);
		auto client = m_databasePool.acquire();

		if (!m_configs.contains(guild))
		{
			event.reply(dpp::message(RemoveServer::NoChannel).set_flags(dpp::m_ephemeral));
			return;
		}

		dpp::channel* channel = dpp::find_channel(event.command.channel_id);
		if (channel == nullptr || !channel->get_user_permissions(&event.command.usr).can(dpp::p_administrator))
		{
			event.reply(dpp::message("You do not have permission to remove servers!").set_flags(dpp::m_ephemeral));
			return;
		}

		ServerConfig& config = *m_configs[(uint64_t)guild];
		bool isPinnedRemoved = false;

		std::copy_if(std::make_move_iterator(config.m_servers.begin()), std::make_move_iterator(config.m_servers.end()), std::back_inserter(activeServers), [&serversToDelete, &deletedServers, &config, &isPinnedRemoved](const auto& server){
			if (const auto it = std::find(serversToDelete.begin(), serversToDelete.end(), std::to_string(server->m_id)); it != serversToDelete.end())
			{
				if (config.m_statusWidget.m_activeServerID.has_value() && server->m_id == config.m_statusWidget.m_activeServerID)
					isPinnedRemoved = true;
					
				serversToDelete.erase(it);
				deletedServers += std::format("  - {}\n", server->m_name);
				return false;
			}
		
			return true;
		});

		config.m_servers = std::move(activeServers);
		client->database(Database::Name)[Database::ServerConfig].update_one(
			make_document(kvp("guildID", (int64_t)config.m_guildID)),
			make_document(kvp("$set", make_document(kvp("servers", Server::ConstructArray(config.m_servers)))))
		);
		if (isPinnedRemoved)
		{
			config.m_statusWidget.m_activeServerID.reset();
			config.m_statusWidget.m_activeServer = nullptr;
		}
		updateServerStatusWidget(config);
	}

	std::string notFoundServers;
	for (const auto& server : serversToDelete)
	{
		if (const auto it = std::find_if(event.command.msg.components[0].components[0].options.begin(), 
										 event.command.msg.components[0].components[0].options.end(),
										 [server](const dpp::select_option& option) { return option.value == server; });
			it != event.command.msg.components[0].components[0].options.end()
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
	auto logMessage = std::make_shared<GuildEmbedMessage>(message, guild);
	logMessage->user = event.command.usr;
	m_bot.componentLog(logMessage);
}

void ServerStatusComponent::onSelectServer(const dpp::select_click_t& event)
{
	dpp::message msg(event.command.channel_id, "Kitty slips and falls");
	m_bot->message_create(msg, [this](const dpp::confirmation_callback_t& callback) {
		if (callback.is_error())
			return;
		std::this_thread::sleep_for(std::chrono::seconds(5));
		dpp::message msg = callback.get<dpp::message>();
		msg.set_content("Just kidding! Kitty lives happy and unharmed!");
		m_bot->message_edit(msg);
		});
	event.reply("");
}

void ServerStatusComponent::onServerRestartButton(const dpp::button_click_t& event)
{
}

void ServerStatusComponent::onServerStartButton(const dpp::button_click_t& event)
{
}

void ServerStatusComponent::onServerStopButton(const dpp::button_click_t& event)
{
}

void ServerStatusComponent::onServerSettingsButton(const dpp::button_click_t& event)
{
	std::lock_guard guard(m_mutex);
	auto client = m_databasePool.acquire();

	const auto guild = event.command.guild_id;
	if (!m_configs.contains(guild))
	{
		event.reply(dpp::message(RemoveServer::NoChannel).set_flags(dpp::m_ephemeral));
		return;
	}

	dpp::channel* channel = dpp::find_channel(event.command.channel_id);
	if (channel == nullptr || !channel->get_user_permissions(&event.command.usr).can(dpp::p_administrator))
	{
		event.reply(dpp::message("You do not have permission to change the server status widget!").set_flags(dpp::m_ephemeral));
		return;
	}

	const auto& config = *m_configs[guild];
	auto msg = dpp::message().set_flags(dpp::m_ephemeral);
	if (!config.m_servers.empty())
	{
		msg.add_component(
			dpp::component().add_component(
				getServerSelectMenuComponent(config)
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
	std::lock_guard guard(m_mutex);
	auto client = m_databasePool.acquire();

	const auto guild = event.command.guild_id;
	if (!m_configs.contains(guild))
	{
		event.reply(dpp::message("You must set a status channel before selecting a pinned server!").set_flags(dpp::m_ephemeral));
		return;
	}

	dpp::channel* channel = dpp::find_channel(event.command.channel_id);
	if (channel == nullptr || !channel->get_user_permissions(&event.command.usr).can(dpp::p_administrator))
	{
		event.reply(dpp::message("You do not have permission to change the pinned server!").set_flags(dpp::m_ephemeral));
		return;
	}

	auto& config = *m_configs[guild];

	if (config.m_statusWidget.m_commandID.has_value())
	{
		event.reply(dpp::message("Server status widget is still building, please try again!").set_flags(dpp::m_ephemeral));
		return;
	}

	const uint64_t serverID = std::stoull(event.values[0]);
	if (config.m_statusWidget.m_activeServerID.has_value() && serverID == *config.m_statusWidget.m_activeServerID)
	{
		event.reply(dpp::message("Server is already pinned!").set_flags(dpp::m_ephemeral));
		return;
	}

	auto it = std::find_if(config.m_servers.begin(), config.m_servers.end(), [serverID](const auto& server){ return server->m_id == serverID; });
	if (it == config.m_servers.end())
	{
		event.reply(dpp::message("Could not find selected server, it was probably deleted!").set_flags(dpp::m_ephemeral));
		return;
	}

	config.m_statusWidget.m_activeServerID = (*it)->m_id;
	config.m_statusWidget.m_activeServer = it->get();
	updateServerStatusWidget(config);
	client->database(Database::Name)[Database::ServerConfig].update_one(
		make_document(kvp("guildID", (int64_t)config.m_guildID)),
		make_document(kvp("$set", make_document(kvp("statusWidget", config.m_statusWidget.getValue()))))
	);

	event.reply("");
}

void ServerStatusComponent::onSelectQueryServer(const dpp::select_click_t& event)
{
	std::lock_guard guard(m_mutex);
	auto client = m_databasePool.acquire();

	const auto guild = event.command.guild_id;
	if (!m_configs.contains(guild))
	{
		event.reply(dpp::message("You must set a status channel before querying a server!").set_flags(dpp::m_ephemeral));
		return;
	}

	dpp::channel* channel = dpp::find_channel(event.command.channel_id);
	if (channel == nullptr || !channel->get_user_permissions(&event.command.usr).can(dpp::p_administrator))
	{
		event.reply(dpp::message("You do not have permission to change the pinned server!").set_flags(dpp::m_ephemeral));
		return;
	}

	auto& config = *m_configs[guild];


	const uint64_t serverID = std::stoull(event.values[0]);
	auto it = std::find_if(config.m_servers.begin(), config.m_servers.end(), [serverID](const auto& server) { return server->m_id == serverID; });
	if (it == config.m_servers.end())
	{
		event.reply(dpp::message("Could not find selected server, it was probably deleted!").set_flags(dpp::m_ephemeral));
		return;
	}

	event.reply(dpp::message()
		.add_embed(
			getServerStatusEmbed(**it)
		)
		.set_flags(dpp::m_ephemeral)
	);
}

void ServerStatusComponent::updateServerStatusWidget(const ServerConfig& config)
{	
	// HACK:	Current dpp API does not let you modify the ID of a message, you must retrieve the message and run a callback method.
	//			We don't actually need the retrieve the message to edit the widget since we already know the channel and the ID of the message.
	//			This is ripped straight from the GitHub repository, without a callback since we don't care if it fails
	const auto widget = getServerStatusWidget(config);
	m_bot->post_rest_multipart(
		API_PATH "/channels",
		std::to_string(config.m_channelID),
		"messages/" + std::to_string(*config.m_statusWidget.m_messageID),
		dpp::m_patch,
		widget.build_json(true), [this](dpp::json& j, const dpp::http_request_completion_t& http) { },
		widget.file_data
	);
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

	return std::move(modal);
}

dpp::component ServerStatusComponent::getRemoveServerComponent(const ServerConfig& config)
{
	return dpp::component().add_component(
		getServerSelectMenuComponent(config)
		.set_id(RemoveServer::SelectOption)
		.set_min_values(1)
		.set_max_values(config.m_servers.size())
		.set_placeholder(RemoveServer::Placeholder)
	);
}

dpp::component ServerStatusComponent::getServerSelectMenuComponent(const ServerConfig& config)
{
	auto selectMenuComponent = dpp::component().set_type(dpp::cot_selectmenu);
	for (const auto& server : config.m_servers)
		selectMenuComponent.add_select_option(dpp::select_option(server->m_name, std::to_string(server->m_id)));
	
	return std::move(selectMenuComponent);
}

dpp::embed ServerStatusComponent::getServerStatusEmbed(const Server& server)
{
	return dpp::embed()
		.set_title(server.m_name)
		.add_field("IP Address", "TestServer.com")
		.add_field("Player Count", "10")
		.set_timestamp(time(0));
}

dpp::message ServerStatusComponent::getServerStatusWidget(const ServerConfig& config)
{
	auto message = dpp::message();
	message.set_channel_id(config.m_channelID);

	dpp::component buttonRow = dpp::component();
	if (config.m_statusWidget.m_activeServerID.has_value())
	{
		message.add_embed(getServerStatusEmbed(*config.m_statusWidget.m_activeServer));
		buttonRow.add_component(
			dpp::component()
				.set_label("Restart")
				.set_id(ServerStatusWidget::RestartServer)
				.set_type(dpp::cot_button)
		).add_component(
			dpp::component()
				.set_label("Start")
				.set_id(ServerStatusWidget::StartServer)
				.set_type(dpp::cot_button)
		).add_component(
			dpp::component()
				.set_label("Stop")
				.set_id(ServerStatusWidget::StopServer)
				.set_type(dpp::cot_button)
		);
	}
	else
	{
		message.add_embed(
			dpp::embed()
				.set_description("No pinned server selected!")
				.set_timestamp(time(0))
		);
	}
	buttonRow.add_component(
		dpp::component()
			.set_label("Settings")
			.set_id(ServerStatusWidget::ServerSettings)
			.set_type(dpp::cot_button)
	);
	message.add_component(buttonRow);

	if (config.m_statusWidget.m_activeServerID.has_value() && config.m_servers.size() > 1)
	{
		message.add_component(
			dpp::component().add_component(
			getServerSelectMenuComponent(config)
				.set_placeholder("Select a server to query")
				.set_id(ServerStatusWidget::QueryServer)
			)
		);
	}

	return message;
}

void ServerStatusComponent::onChannelDelete(const dpp::channel_delete_t& event)
{
	std::lock_guard guard(m_mutex);
	auto client = m_databasePool.acquire();

	const auto guild = event.deleted.guild_id;
	const auto channel = event.deleted.id;
	if (!m_configs.contains(guild) || (dpp::snowflake)m_configs[(uint64_t)guild]->m_channelID != channel)
		return;

	m_configs.erase((uint64_t)guild);
	client->database(Database::Name)[Database::ServerConfig].delete_one(make_document(kvp("guildID", (int64_t)guild)));
	m_bot.componentLog(std::make_shared<GuildEmbedMessage>("Server status channel was deleted, removing saved server configurations!", guild));
}

void ServerStatusComponent::onMessageDelete(const dpp::message_delete_t& event)
{
	std::lock_guard guard(m_mutex);
	auto client = m_databasePool.acquire();

	const auto guild = event.guild_id;
	if (! m_configs.contains(guild))
		return;

	const auto& config = *m_configs[guild];
	if (event.channel_id != config.m_channelID || event.id != config.m_statusWidget.m_messageID || config.m_statusWidget.m_commandID.has_value())
		return;

 	m_configs.erase((uint64_t)guild);
	client->database(Database::Name)[Database::ServerConfig].delete_one(make_document(kvp("guildID", (int64_t)guild)));
	m_bot.componentLog(std::make_shared<GuildEmbedMessage>("Server status widget was deleted, removing saved server configurations!", guild));
}

