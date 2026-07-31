#include "LogComponent.hpp"
#include "LogComponent_p.hpp"

#include "Database/DatabaseManager.hpp"
#include "Database/MongoDB/MongoDBManager.hpp"
#include "Log.hpp"

#include <boost/asio.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <functional>
#include <tuple>
#include <type_traits>

namespace
{
	namespace SetLogChannel
	{
		constexpr auto Channel = "channel";
	}
}

template<typename T>
struct function_traits;

template<typename R, typename C, typename... Args>
struct function_traits<R(C::*)(Args...)> {
	using args_tuple = std::tuple<Args...>;
	static constexpr std::size_t arity = sizeof...(Args);

	template<std::size_t I>
	using arg = std::tuple_element_t<I, args_tuple>;
};

template<typename R, typename C, typename... Args>
struct function_traits<R(C::*)(Args...) const> {
	using args_tuple = std::tuple<Args...>;
	static constexpr std::size_t arity = sizeof...(Args);

	template<std::size_t I>
	using arg = std::tuple_element_t<I, args_tuple>;
};

// 2. Extract Callback type (last argument of target DPP function)
template<typename Fn>
using callback_type = std::decay_t<
	typename function_traits<Fn>::template arg<function_traits<Fn>::arity - 1>
>;

// 3. Extract Result type from callback signature
template<typename Callback>
struct callback_traits;

// Primary match for std::function<void(Arg0, ...)> or dpp::command_completion_event_t
template<typename R, typename Arg0, typename... Rest>
struct callback_traits<std::function<R(Arg0, Rest...)>> {
	using result_type = std::decay_t<Arg0>;
};

template<typename R, typename Arg0, typename... Rest>
struct callback_traits<R(*)(Arg0, Rest...)> {
	using result_type = std::decay_t<Arg0>;
};

template<typename T>
struct callback_traits {
	using functor_sig = decltype(&T::operator());
	using result_type = typename callback_traits<functor_sig>::result_type;
};

template<typename C, typename R, typename Arg0, typename... Rest>
struct callback_traits<R(C::*)(Arg0, Rest...) const> {
	using result_type = std::decay_t<Arg0>;
};

// 4. The Complete AwaitDpp Wrapper
template<typename Fn, typename Obj, typename... Args>
auto AwaitDpp(Fn fn, Obj* obj, Args&&... args)
{
	using Callback = callback_type<Fn>;
	using Result = typename callback_traits<Callback>::result_type;

	return boost::asio::async_initiate<
		const boost::asio::use_awaitable_t<>&,
		void(Result)
	>(
		[fn, obj, ... args = std::forward<Args>(args)](auto handler) mutable
		{
			// Wrap the move-only Asio handler in a shared_ptr to satisfy std::function's copy requirement
			auto shared_handler = std::make_shared<decltype(handler)>(std::move(handler));

			(obj->*fn)(
				std::move(args)...,
				[shared_handler](const auto& res) mutable
				{
					if (shared_handler) {
						std::move(*shared_handler)(res);
					}
				}
				);
		},
		boost::asio::use_awaitable
	);
}

LogComponent::LogComponent(DiscordBot& bot)
	: Component(bot)
	, m_p(std::make_unique<LogComponentPrivate>())
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
		auto client = m_p->m_databasePool.acquire();
		for (auto& logConfig : LogConfig::FindAll(*client))
		{
			const auto guildID = logConfig->m_guildID;
			m_p->m_configs.store(guildID, std::move(logConfig));
		}
	}
}

LogComponent::~LogComponent() = default;

void LogComponent::onSetLogChannel(const dpp::slashcommand_t& event)
{
	event.thinking(true);
	dpp::snowflake channel = std::get<dpp::snowflake>(event.get_parameter(SetLogChannel::Channel));
	m_bot->channel_get(channel, [this, event, channel](const dpp::confirmation_callback_t& callback) {
		if (callback.is_error())
		{
			Logger::App().warn(
				"Cannot set log channel {} for guild {}: {}",
				channel,
				event.command.guild_id,
				callback.get_error().message
			);
			event.edit_original_response(dpp::message("Cannot see channel. Try checking the channel permissions."));
			return;
		}

		const auto guild = (uint64_t)event.command.guild_id;

		std::shared_ptr<LogConfig> config;
		if (config = m_p->m_configs.find(guild); !config)
		{
			config = std::make_shared<LogConfig>();
			config->m_guildID = guild;
			config->m_channelID = channel;

			{
				auto client = m_p->m_databasePool.acquire();
				config->insertIntoDatabase(*client);
			}

			m_p->m_configs.store(guild, config);
		}
		else
		{
			std::unique_lock lock(config->m_mutex);

			config->m_channelID = channel;

			{
				auto client = m_p->m_databasePool.acquire();
				config->updateChannelID(*client);
			}
		}

		std::string msg(std::format("Log channel changed to <#{}>", channel.str()));
		event.edit_original_response(dpp::message(msg));
		Logger::App().info(
			"Log channel for guild {} changed to {} by user {}",
			guild,
			channel,
			event.command.usr.id
		);
		auto logMessage = std::make_unique<GuildEmbedMessage>(msg, guild);
		logMessage->user = event.command.usr;
		m_bot.componentLog(std::move(logMessage));
	});

}

dpp::task<void> LogComponent::onChannelDelete(const dpp::channel_delete_t& event)
{
	const auto guild = (uint64_t)event.deleted.guild_id;
	const auto channel = (uint64_t)event.deleted.id;

	std::shared_ptr<LogConfig> config;
	if (config = m_p->m_configs.find(guild); !config)
		co_return;

	std::unique_lock lock(config->m_mutex);

	if (config->m_channelID != channel)
		co_return;

	{
		auto client = m_p->m_databasePool.acquire();
		config->removeFromDatabase(*client);
	}

	m_p->m_configs.erase(guild);
	Logger::App().info("Removed deleted log channel {} for guild {}", channel, guild);

	co_return;
}

boost::asio::awaitable<void> LogComponent::onComponentLog(const ComponentLogMessage* message)
{
	if (const auto guildEmbedMessage = dynamic_cast<const GuildEmbedMessage*>(message); guildEmbedMessage != nullptr)
	{
		std::shared_ptr<LogConfig> config;
		if (config = m_p->m_configs.find((uint64_t)guildEmbedMessage->guildID); !config)
			co_return;

		std::shared_lock lock(config->m_mutex);

		dpp::embed embed = dpp::embed();

		embed.set_description(guildEmbedMessage->message);
		embed.set_timestamp(time(0));

		if (guildEmbedMessage->user.has_value())
		{
			embed.set_author(guildEmbedMessage->user->format_username(), "", guildEmbedMessage->user->get_avatar_url());
			embed.set_footer(std::to_string((int64_t)guildEmbedMessage->user->id), "");
		}

		for (auto& field : guildEmbedMessage->fields)
			embed.fields.push_back(std::move(field));

		const auto response = co_await AwaitDpp(&dpp::cluster::message_create, &(*m_bot), dpp::message(config->m_channelID, std::move(embed)));
		if (response.is_error())
		{
			Logger::App().warn(
				"Failed to send component log to channel {} for guild {}: {}",
				config->m_channelID,
				guildEmbedMessage->guildID,
				response.get_error().message
			);
		}
	}
	else if (const auto guildMessage = dynamic_cast<const GuildMessage*>(message); guildMessage != nullptr)
	{
		std::shared_ptr<LogConfig> config;
		if (config = m_p->m_configs.find((uint64_t)guildMessage->guildID); !config)
			co_return;

		std::shared_lock lock(config->m_mutex);

		dpp::message newMessage(guildMessage->message);
		newMessage.set_channel_id(config->m_channelID);
		const auto response = co_await AwaitDpp(&dpp::cluster::message_create, &(*m_bot), newMessage);
		if (response.is_error())
		{
			Logger::App().warn(
				"Failed to send component log to channel {} for guild {}: {}",
				config->m_channelID,
				guildMessage->guildID,
				response.get_error().message
			);
		}
	}
	else if (const auto broadcastMessage = dynamic_cast<const BroadcastMessage*>(message); broadcastMessage != nullptr)
	{
		for (const auto& entry : m_p->m_configs.snapshot())
		{
			const auto& config = entry.second;
			std::shared_lock lock(config->m_mutex);
			dpp::message newMessage(broadcastMessage->message);
			newMessage.set_channel_id((dpp::snowflake)config->m_channelID);
			const auto guild = entry.first;
			const auto channel = config->m_channelID;
			m_bot->message_create(
				newMessage,
				[guild, channel](const dpp::confirmation_callback_t& response)
				{
					if (response.is_error())
					{
						Logger::App().warn(
							"Failed to send broadcast log to channel {} for guild {}: {}",
							channel,
							guild,
							response.get_error().message
						);
					}
				}
			);
		}
	}
}

LogComponentPrivate::LogComponentPrivate()
	: m_databasePool(DatabaseManager::GetInstance<MongoDBManager>()->getPool())
{

}
