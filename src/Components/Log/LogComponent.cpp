#include "LogComponent.hpp"

namespace
{
	namespace SetLogChannel
	{
		constexpr auto Channel = "channel";
	}
}

LogComponent::LogComponent(DiscordBot& bot)
	: Component(bot)
	, m_databasePool(DatabaseManager::GetInstance<MongoDBManager>()->getPool())
{
	m_slashCommands.emplace_back(
		std::bind_front(&LogComponent::onSetLogChannel, this),
		dpp::slashcommand("setlogchannel", "Sets the channel where logs are recorded.", m_bot->me.id)
			.add_option(
				dpp::command_option(dpp::co_channel, SetLogChannel::Channel, "Channel to print logs", true)
					.add_channel_type(dpp::CHANNEL_TEXT)
			)
			.set_default_permissions(0)
	);

	{
		auto client = m_databasePool.acquire();
		for (auto& logConfig : LogConfig::FindAll(*client))
		{
			const auto guildID = logConfig->m_guildID;
			m_configs.store(guildID, std::move(logConfig));
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

		const auto guild = (uint64_t)event.command.guild_id;

		LogConfig* config;
		if (config = m_configs.find(guild); !config)
		{
			config = new LogConfig();
			config->m_guildID = guild;
			config->m_channelID = channel;

			{
				auto client = m_databasePool.acquire();
				config->insertIntoDatabase(*client);
			}

			m_configs.store(guild, std::unique_ptr<LogConfig>(config));
		}
		else
		{
			std::unique_lock lock(config->m_mutex);

			config->m_channelID = channel;

			{
				auto client = m_databasePool.acquire();
				config->updateChannelID(*client);
			}
		}

		std::string msg(std::format("Log channel changed to <#{}>", channel.str()));
		event.edit_original_response(dpp::message(msg));
		auto logMessage = std::make_shared<GuildEmbedMessage>(msg, guild);
		logMessage->user = event.command.usr;
		m_bot.componentLog(logMessage);
	});

}

void LogComponent::onChannelDelete(const dpp::channel_delete_t& event)
{
	const auto guild = (uint64_t)event.deleted.guild_id;
	const auto channel = (uint64_t)event.deleted.id;

	LogConfig* config;
	if (config = m_configs.find(guild); !config)
		return;

	std::unique_lock lock(config->m_mutex);

	if (config->m_channelID != channel)
		return;

	{
		auto client = m_databasePool.acquire();
		config->removeFromDatabase(*client);
	}

	m_configs.erase(guild);
}

void LogComponent::onComponentLog(const ComponentLogMessage* message)
{
	if (const auto guildEmbedMessage = dynamic_cast<const GuildEmbedMessage*>(message); guildEmbedMessage != nullptr)
	{
		LogConfig* config;
		if (config = m_configs.find((uint64_t)guildEmbedMessage->guildID); !config)
			return;

		std::shared_lock lock(config->m_mutex);

		dpp::embed embed = dpp::embed();

		embed.set_description(guildEmbedMessage->message);
		embed.set_timestamp(time(0));

		if (guildEmbedMessage->user.has_value())
		{
			embed.set_author(guildEmbedMessage->user->format_username(), "", guildEmbedMessage->user->get_avatar_url());
			embed.set_footer(std::to_string((int64_t)guildEmbedMessage->user->id), "");
		}

		for (const auto& field : guildEmbedMessage->fields)
			embed.fields.push_back(std::move(field));

		m_bot->message_create(dpp::message(config->m_channelID, std::move(embed)));
	}
	else if (const auto guildMessage = dynamic_cast<const GuildMessage*>(message); guildMessage != nullptr)
	{
		LogConfig* config;
		if (config = m_configs.find((uint64_t)guildMessage->guildID); !config)
			return;

		std::shared_lock lock(config->m_mutex);

		dpp::message newMessage(guildMessage->message);
		newMessage.set_channel_id(config->m_channelID);
		m_bot->message_create(newMessage);
	}
	else if (const auto broadcastMessage = dynamic_cast<const BroadcastMessage*>(message); broadcastMessage != nullptr)
	{
		for (auto it = m_configs.begin(); it != m_configs.end(); it++)
		{
			const auto guild = it->first;
			dpp::message newMessage(broadcastMessage->message);
			newMessage.set_channel_id((dpp::snowflake)it->second->m_channelID);
			m_bot->message_create(newMessage);
		}
	}
}


