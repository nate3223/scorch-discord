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

	namespace AddServerModal
	{
		constexpr auto Form			= "AddServerModal";
		constexpr auto ServerName	= "AddServerModalServerName";
		constexpr auto Address		= "AddServerModalAddress";
		constexpr auto Emoji		= "AddServerModalEmoji";
		constexpr auto Emojis		= { dpp::unicode_emoji::globe_with_meridians, dpp::unicode_emoji::rainbow_flag, dpp::unicode_emoji::tada, dpp::unicode_emoji::clown, dpp::unicode_emoji::hearts, dpp::unicode_emoji::sauropod, dpp::unicode_emoji::goose, dpp::unicode_emoji::four_leaf_clover, dpp::unicode_emoji::lotus, dpp::unicode_emoji::star2, dpp::unicode_emoji::snowflake, dpp::unicode_emoji::fire, dpp::unicode_emoji::hamburger, dpp::unicode_emoji::champagne_glass, dpp::unicode_emoji::island, dpp::unicode_emoji::rocket, dpp::unicode_emoji::night_with_stars, dpp::unicode_emoji::gem, dpp::unicode_emoji::pushpin, dpp::unicode_emoji::mirror_ball, dpp::unicode_emoji::lock, dpp::unicode_emoji::heart_on_fire, dpp::unicode_emoji::sos, dpp::unicode_emoji::warning };
		constexpr auto NoChannel	= "You must set a status channel before adding a server!";
	}

	namespace RemoveServerSelect
	{
		constexpr auto SelectOption	= "RemoveServerSelectOption";
		constexpr auto Placeholder	= "Select servers to remove";
		constexpr auto NoChannel	= "You must set a status channel before removing a server!";
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
	{
		SlashCommand setStatusChannel {
			std::bind_front(&ServerStatusComponent::onSetStatusChannel, this),
			dpp::slashcommand("setstatuschannel", "Sets the channel where the server status is displayed.", m_bot->me.id)
		};
		setStatusChannel.slashCommand.add_option(
			dpp::command_option(dpp::co_channel, SetStatusChannel::Channel, "Channel to display server status", true)
				.add_channel_type(dpp::CHANNEL_TEXT)
				
		);
		setStatusChannel.slashCommand.default_member_permissions = 0;
		m_slashCommands.push_back(std::move(setStatusChannel));
	}

	{
		SlashCommand addServer {
			std::bind_front(&ServerStatusComponent::onAddServerCommand, this),
			dpp::slashcommand("addserver", "Adds a server to be tracked.", m_bot->me.id)
		};
		addServer.slashCommand.default_member_permissions = 0;
		m_slashCommands.push_back(std::move(addServer));

		m_formCommands.emplace_back(
			AddServerModal::Form,
			std::bind_front(&ServerStatusComponent::onAddServerForm, this)
		);
	}

	{
		SlashCommand removeServer {
			std::bind_front(&ServerStatusComponent::onRemoveServerCommand, this),
			dpp::slashcommand("removeserver", "Removes tracked servers", m_bot->me.id)

		};
		removeServer.slashCommand.default_member_permissions = 0;
		m_slashCommands.push_back(std::move(removeServer));

		m_selectCommands.emplace_back(
			RemoveServerSelect::SelectOption,
			std::bind_front(&ServerStatusComponent::onRemoveServerSelect, this)
		);
	}

	m_selectCommands.emplace_back(ServerSelect::MenuOption, std::bind_front(&ServerStatusComponent::onSelectServer, this));

	{
		auto client = m_databasePool.acquire();
		for (auto& doc : client->database(Database::Name)[Database::ServerConfig].find({}))
		{
			auto config = ServerConfig(doc);
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

		if (!m_configs.contains((uint64_t)guild))
		{
			ServerConfig config;
			config.m_guildID = guild;
			config.m_channelID = channel;
			client->database(Database::Name)[Database::ServerConfig].insert_one(config.getValue());
			m_configs[(uint64_t)guild] = std::make_unique<ServerConfig>(std::move(config));
		}
		else
		{
			ServerConfig& config = *m_configs[(uint64_t)guild];
			config.m_channelID = channel;
			client->database(Database::Name)[Database::ServerConfig].update_one(
				make_document(kvp("guildID", (int64_t)config.m_guildID)),
				make_document(kvp("$set", make_document(kvp("channelID", (int64_t)config.m_channelID))))
			);
		}

		std::string message = std::format("Server status channel changed to <#{}>", channel.str());
		event.edit_original_response(dpp::message(message));
		m_bot.componentLog(std::make_shared<UserEmbedMessage>(message, guild, event.command.usr));
	});
	
	//dpp::message msg(channel, "Penis message");
	//msg.add_component(
	//	dpp::component().add_component(
	//		dpp::component()
	//			.set_type(dpp::cot_selectmenu)
	//			.set_placeholder("Choose")
	//			//.add_select_option(dpp::select_option("Kill Kitty", "Kitty dies", "This kills kitty").set_emoji(dpp::unicode_emoji::cat).set_emoji(dpp::unicode_emoji::skull))
	//			.add_select_option(dpp::select_option("Save kitty", "Kitty lives", "This saves kitty").set_emoji(dpp::unicode_emoji::cat).set_emoji(dpp::unicode_emoji::angel))
	//			.set_id(ServerSelect::MenuOption)
	//	)
	//);
	//msg.add_component(
	//	dpp::component().add_component(
	//		dpp::component()
	//			.set_type(dpp::cot_button)
	//			.set_label("Button 1")
	//			.set_id("testbutton1")
	//	)
	//	.add_component(
	//		dpp::component()
	//			.set_type(dpp::cot_button)
	//			.set_label("Button 2")
	//			.set_id("testbutton2")
	//	)
	//	.add_component(
	//		dpp::component()
	//			.set_type(dpp::cot_button)
	//			.set_label("Button 3")
	//			.set_id("testbutton3")
	//	)
	//);
}

void ServerStatusComponent::onAddServerCommand(const dpp::slashcommand_t& event)
{
	std::lock_guard guard(m_mutex);
	auto client = m_databasePool.acquire();

	const auto guildID = event.command.guild_id;
	if (!m_configs.contains(guildID))
	{
		event.reply(dpp::message(AddServerModal::NoChannel).set_flags(dpp::m_ephemeral));
		return;
	}

	dpp::interaction_modal_response modal(AddServerModal::Form, "Add a server");
	modal.add_component(
		dpp::component()
			.set_label("Server name")
			.set_id(AddServerModal::ServerName)
			.set_type(dpp::cot_text)
			.set_text_style(dpp::text_short)
			.set_required(true)
	);

	modal.add_row();
	modal.add_component(
		dpp::component()
			.set_label("Address for API endpoint")
			.set_id(AddServerModal::Address)
			.set_type(dpp::cot_text)
			.set_text_style(dpp::text_short)
			.set_required(true)
	);

	event.dialog(modal);
}

void ServerStatusComponent::onAddServerForm(const dpp::form_submit_t& event)
{
	std::lock_guard guard(m_mutex);
	auto client = m_databasePool.acquire();

	const auto guild = event.command.guild_id;
	if (!m_configs.contains(guild))
	{
		event.reply(dpp::message(AddServerModal::NoChannel).set_flags(dpp::m_ephemeral).set_flags(dpp::m_urgent));
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
		
	config.m_servers.emplace_back((uint64_t)event.command.id, serverName, address);
	client->database(Database::Name)[Database::ServerConfig].update_one(
		make_document(kvp("guildID", (int64_t)config.m_guildID)),
		make_document(kvp("$set", make_document(kvp("servers", Server::ConstructArray(config.m_servers)))))
	);

	event.reply(dpp::message("Server added successfully!").set_flags(dpp::m_ephemeral));
	auto embedLog = std::make_shared<UserEmbedMessage>("Added new server", event.command.guild_id, event.command.usr);
	embedLog->fields.emplace_back("Name", serverName);
	embedLog->fields.emplace_back("Address", address);
	m_bot.componentLog(embedLog);
}

void ServerStatusComponent::onRemoveServerCommand(const dpp::slashcommand_t& event)
{
	std::lock_guard guard(m_mutex);
	auto client = m_databasePool.acquire();

	const auto guild = event.command.guild_id;
	if (!m_configs.contains(guild))
	{
		event.reply(dpp::message(RemoveServerSelect::NoChannel).set_flags(dpp::m_ephemeral));
		return;
	}

	ServerConfig& config = *m_configs[(uint64_t)guild];
	if (config.m_servers.empty())
	{
		event.reply(dpp::message("No servers to remove!").set_flags(dpp::m_ephemeral));
		return;
	}

	auto msg = dpp::message().set_flags(dpp::m_ephemeral)
		.add_component(
			dpp::component().add_component(
				getServerSelectMenuComponent(config)
					.set_id(RemoveServerSelect::SelectOption)
					.set_min_values(1)
					.set_max_values(config.m_servers.size())
					.set_placeholder(RemoveServerSelect::Placeholder)
			)
		);

	event.reply(msg);
}

void ServerStatusComponent::onRemoveServerSelect(const dpp::select_click_t& event)
{
	std::vector<Server> activeServers;
	std::vector<std::string> serversToDelete(event.values);
	std::string deletedServers;
	const auto guild = event.command.guild_id;
	
	{
		std::lock_guard guard(m_mutex);
		auto client = m_databasePool.acquire();

		if (!m_configs.contains(guild))
		{
			m_bot->direct_message_create(event.command.usr.id, dpp::message(RemoveServerSelect::NoChannel));
			return;
		}

		ServerConfig& config = *m_configs[(uint64_t)guild];

		std::copy_if(config.m_servers.begin(), config.m_servers.end(), std::back_inserter(activeServers), [&serversToDelete, &deletedServers](const Server& server){
			if (const auto it = std::find(serversToDelete.begin(), serversToDelete.end(), std::to_string(server.m_id)); it != serversToDelete.end())
			{
				serversToDelete.erase(it);
				deletedServers += std::format("  - {}\n", server.m_name);
				return false;
			}
		
			return true;
		});

		config.m_servers = std::move(activeServers);
		client->database(Database::Name)[Database::ServerConfig].update_one(
			make_document(kvp("guildID", (int64_t)config.m_guildID)),
			make_document(kvp("$set", make_document(kvp("servers", Server::ConstructArray(config.m_servers)))))
		);
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
	m_bot.componentLog(std::make_shared<UserEmbedMessage>(message, guild, event.command.usr));
	//m_bot->direct_message_create(event.command.usr.id, dpp::message(message));
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

dpp::component ServerStatusComponent::getServerSelectMenuComponent(const ServerConfig& config)
{
	auto selectMenuComponent = dpp::component().set_type(dpp::cot_selectmenu);
	for (const auto& server : config.m_servers)
		selectMenuComponent.add_select_option(dpp::select_option(server.m_name, std::to_string(server.m_id)));
	
	return selectMenuComponent;
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
}

