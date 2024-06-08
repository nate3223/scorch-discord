#include "ServerStatusComponent.hpp"

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
	}

	namespace AddServerModal
	{
		constexpr auto Form			= "AddServerModal";
		constexpr auto ServerName	= "AddServerModalServerName";
		constexpr auto IPAddress	= "AddServerModalIPAddress";
		constexpr auto Emoji		= "AddServerModalEmoji";
		constexpr auto Emojis		= { dpp::unicode_emoji::globe_with_meridians, dpp::unicode_emoji::rainbow_flag, dpp::unicode_emoji::tada, dpp::unicode_emoji::clown, dpp::unicode_emoji::hearts, dpp::unicode_emoji::sauropod, dpp::unicode_emoji::goose, dpp::unicode_emoji::four_leaf_clover, dpp::unicode_emoji::lotus, dpp::unicode_emoji::star2, dpp::unicode_emoji::snowflake, dpp::unicode_emoji::fire, dpp::unicode_emoji::hamburger, dpp::unicode_emoji::champagne_glass, dpp::unicode_emoji::island, dpp::unicode_emoji::rocket, dpp::unicode_emoji::night_with_stars, dpp::unicode_emoji::gem, dpp::unicode_emoji::pushpin, dpp::unicode_emoji::mirror_ball, dpp::unicode_emoji::lock, dpp::unicode_emoji::heart_on_fire, dpp::unicode_emoji::sos, dpp::unicode_emoji::warning };
	}

	namespace Database
	{
		constexpr auto Name			= "Scorch";
		constexpr auto ServerConfig	= "ServerConfig";
	}
}

ServerStatusComponent::ServerStatusComponent(dpp::cluster& bot)
	: Component(bot)
	, m_databasePool(DatabaseManager::GetInstance<MongoDBManager>()->getPool())
{
	{
		SlashCommand setStatusChannel {
			std::bind_front(&ServerStatusComponent::onSetStatusChannel, this),
			dpp::slashcommand("setstatuschannel", "Sets the channel where the server status is displayed.", m_bot.me.id)
		};
		setStatusChannel.slashCommand.add_option(
			dpp::command_option(dpp::co_channel, SetStatusChannel::Channel, "Channel to display server status", true)
				.add_channel_type(dpp::CHANNEL_TEXT)
				
		);
		setStatusChannel.slashCommand.default_member_permissions = 0;
		m_slashCommands.push_back(std::move(setStatusChannel));
	}

	{
		m_slashCommands.emplace_back(
			std::bind_front(&ServerStatusComponent::onAddServerCommand, this),
			dpp::slashcommand("addserver", "Adds a server to be tracked.", m_bot.me.id)
		);
		m_formCommands.emplace_back(
			AddServerModal::Form,
			std::bind_front(&ServerStatusComponent::onAddServerForm, this)
		);
	}

	m_selectCommands.emplace_back(ServerSelect::MenuOption, std::bind_front(&ServerStatusComponent::onSelectServer, this));

	{
		auto client = m_databasePool.acquire();
		for (auto& doc : client->database(Database::Name)[Database::ServerConfig].find({}))
		{
			auto config = ServerConfig::Construct(doc);
			m_configs[config.guildID] = std::make_unique<ServerConfig>(std::move(config));
		}
	}
}

void ServerStatusComponent::onSetStatusChannel(const dpp::slashcommand_t& event)
{
	dpp::snowflake guild = event.command.guild_id;
	dpp::snowflake channel = std::get<dpp::snowflake>(event.get_parameter(SetStatusChannel::Channel));

	{
		std::lock_guard guard(m_mutex);
		auto client = m_databasePool.acquire();

		if (! m_configs.contains((uint64_t)guild))
		{
			ServerConfig config{
				.guildID = guild,
				.channelID = channel
			};
			client->database(Database::Name)[Database::ServerConfig].insert_one(config.getValue());
			m_configs[(uint64_t)guild] = std::make_unique<ServerConfig>(std::move(config));
		}
		else
		{
			ServerConfig& config = *m_configs[(uint64_t)guild];
			config.channelID = channel;
			client->database(Database::Name)[Database::ServerConfig].update_one(
				make_document(kvp("guildID", (int64_t)config.guildID)),
				make_document(kvp("$set", make_document(kvp("channelID", (int64_t)config.channelID))))
			);
		}
	}

	event.reply(dpp::message(std::format("Set the server status channel to <#{}>", channel.str())).set_flags(dpp::m_ephemeral));
	
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
	const auto guildID = event.command.guild_id;
	if (!m_configs.contains(guildID))
	{
		event.reply(dpp::message("You must set a status channel before adding a server!").set_flags(dpp::m_ephemeral));
		return;
	}

	dpp::interaction_modal_response modal(AddServerModal::Form, "Add a server");
	modal.add_component(
		dpp::component()
			.set_label("Server name")
			.set_id(AddServerModal::ServerName)
			.set_type(dpp::cot_text)
			.set_text_style(dpp::text_short)
	);

	modal.add_row();
	modal.add_component(
		dpp::component()
			.set_label("IP address for API endpoint")
			.set_id(AddServerModal::IPAddress)
			.set_type(dpp::cot_text)
			.set_text_style(dpp::text_short)
	);

	event.dialog(modal);
}

void ServerStatusComponent::onAddServerForm(const dpp::form_submit_t& event)
{
	const auto guild = event.command.guild_id;
	if (!m_configs.contains(guild))
		return;
	
	{
		const auto serverName = std::get<std::string>(event.components[0].components[0].value);
		const auto ipAddress = std::get<std::string>(event.components[1].components[0].value);

		std::lock_guard guard(m_mutex);
		auto client = m_databasePool.acquire();
	}
	event.reply(dpp::message("Server added successfully!").set_flags(dpp::m_ephemeral));
}

void ServerStatusComponent::onSelectServer(const dpp::select_click_t& event)
{
	dpp::message msg(event.command.channel_id, "Kitty slips and falls");
	m_bot.message_create(msg, [this](const dpp::confirmation_callback_t& callback) {
		if (callback.is_error())
			return;
		std::this_thread::sleep_for(std::chrono::seconds(5));
		dpp::message msg = callback.get<dpp::message>();
		msg.set_content("Just kidding! Kitty lives happy and unharmed!");
		m_bot.message_edit(msg);
		});
	event.reply("");
}

void ServerStatusComponent::onChannelDelete(const dpp::channel_delete_t& event)
{
	const auto guild = event.deleted.guild_id;
	const auto channel = event.deleted.id;
	if (!m_configs.contains(guild) || (dpp::snowflake)m_configs[(uint64_t)guild]->channelID != channel)
		return;

	{
		std::lock_guard guard(m_mutex);
		auto client = m_databasePool.acquire();

		m_configs.erase((uint64_t)guild);
		client->database(Database::Name)[Database::ServerConfig].delete_one(make_document(kvp("guildID", (int64_t)guild)));
	}
}

