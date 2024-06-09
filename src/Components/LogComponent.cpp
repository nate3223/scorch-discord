#include "LogComponent.hpp"

namespace
{
	namespace SetLogChannel
	{
		constexpr auto Channel = "channel";
	}

	namespace Database
	{
		constexpr auto Name = "Scorch";
		constexpr auto LogConfig = "LogConfig";
	}
}

LogComponent::LogComponent(DiscordBot& bot)
	: Component(bot)
	, m_databasePool(DatabaseManager::GetInstance<MongoDBManager>()->getPool())
{
	{
		SlashCommand setStatusChannel{
			std::bind_front(&LogComponent::onSetLogChannel, this),
			dpp::slashcommand("setlogchannel", "Sets the channel where logs are recorded.", m_bot->me.id)
		};
		setStatusChannel.slashCommand.add_option(
			dpp::command_option(dpp::co_channel, SetLogChannel::Channel, "Channel to print logs", true)
			.add_channel_type(dpp::CHANNEL_TEXT)

		);
		setStatusChannel.slashCommand.default_member_permissions = 0;
		m_slashCommands.push_back(std::move(setStatusChannel));
	}
	{
		auto client = m_databasePool.acquire();
		for (auto& doc : client->database(Database::Name)[Database::LogConfig].find({}))
		{
			auto config = LogConfig(doc);
			m_configs[config.m_guildID] = std::make_unique<LogConfig>(std::move(config));
		}
	}
}

void LogComponent::onSetLogChannel(const dpp::slashcommand_t& event)
{
	event.thinking(true);
	dpp::snowflake channel = std::get<dpp::snowflake>(event.get_parameter(SetLogChannel::Channel));
	m_bot->channel_get(channel, [this, event, channel](const dpp::confirmation_callback_t& callback) {
		if (callback.is_error())
		{
			event.edit_original_response(dpp::message("Cannot see channel. Try checking the channel permissions."));
			return;
		}

		dpp::snowflake guild = event.command.guild_id;

		{
			std::lock_guard guard(m_mutex);
			auto client = m_databasePool.acquire();
			
			if (!m_configs.contains((uint64_t)guild))
			{
				LogConfig config;
				config.m_guildID = guild;
				config.m_channelID = channel;
				client->database(Database::Name)[Database::LogConfig].insert_one(config.getValue());
				m_configs[(uint64_t)guild] = std::make_unique<LogConfig>(std::move(config));
			}
			else
			{
				LogConfig& config = *m_configs[(uint64_t)guild];
				config.m_channelID = channel;
				client->database(Database::Name)[Database::LogConfig].update_one(
					make_document(kvp("guildID", (int64_t)config.m_guildID)),
					make_document(kvp("$set", make_document(kvp("channelID", (int64_t)config.m_channelID))))
				);
			}
		}

		std::string msg(std::format("Log channel changed to <#{}>", channel.str()));
		event.edit_original_response(dpp::message(msg));
		m_bot.componentLog(std::make_shared<UserEmbedMessage>(msg, guild, event.command.usr));
	});

}

void LogComponent::onChannelDelete(const dpp::channel_delete_t& event)
{
	std::lock_guard guard(m_mutex);
	auto client = m_databasePool.acquire();

	const auto guild = event.deleted.guild_id;
	const auto channel = event.deleted.id;
	if (!m_configs.contains(guild) || (dpp::snowflake)m_configs[(uint64_t)guild]->m_channelID != channel)
		return;

	m_configs.erase((uint64_t)guild);
	client->database(Database::Name)[Database::LogConfig].delete_one(make_document(kvp("guildID", (int64_t)guild)));
}

void LogComponent::onComponentLog(const ComponentLogMessage* message)
{
	std::lock_guard guard(m_mutex);
	auto client = m_databasePool.acquire();

	if (const auto userEmbedMessage = dynamic_cast<const UserEmbedMessage*>(message); userEmbedMessage != nullptr)
	{
		if (!m_configs.contains(userEmbedMessage->guildID))
			return;

		const auto& config = *m_configs[userEmbedMessage->guildID];
		dpp::embed embed = dpp::embed();
		embed.set_author(userEmbedMessage->user.format_username(), "", userEmbedMessage->user.get_avatar_url());
		embed.set_description(userEmbedMessage->message);
		embed.set_footer(std::to_string((int64_t)userEmbedMessage->user.id), "");
		embed.set_timestamp(time(0));

		for (const auto& field : userEmbedMessage->fields)
			embed.fields.push_back(std::move(field));

		m_bot->message_create(dpp::message(config.m_channelID, std::move(embed)));
	}
	else if (const auto guildMessage = dynamic_cast<const GuildMessage*>(message); guildMessage != nullptr)
	{
		if (!m_configs.contains(guildMessage->guildID))
			return;

		const auto& config = *m_configs[guildMessage->guildID];
		dpp::message newMessage(guildMessage->message);
		newMessage.set_channel_id(config.m_channelID);
		m_bot->message_create(newMessage);
	}
	else if (const auto broadcastMessage = dynamic_cast<const BroadcastMessage*>(message); broadcastMessage != nullptr)
	{
		for (const auto& config : m_configs)
		{
			const auto guild = config.first;
			dpp::message newMessage(broadcastMessage->message);
			newMessage.set_channel_id((dpp::snowflake)config.second->m_channelID);
			m_bot->message_create(newMessage);
		}
	}
}


