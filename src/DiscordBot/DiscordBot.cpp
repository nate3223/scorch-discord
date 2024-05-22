#include "DiscordBot.hpp"

#include <dpp/unicode_emoji.h>

DiscordBot::DiscordBot(const char* token)
    : m_bot(token, dpp::i_all_intents)
{
    m_slashCommands = {
            { "ping", "Ping pong!", std::bind_front(&DiscordBot::onPingCommand, this) },
            { "yippee", "Yippee!", std::bind_front(&DiscordBot::onYippeeCommand, this) },
            { "setstatuschannel", "Sets the channel where the server status is displayed.", std::bind_front(&DiscordBot::onSetStatusChannel, this) }
    };

    m_bot.on_log(dpp::utility::cout_logger());
    m_bot.on_slashcommand([this](const dpp::slashcommand_t& event){ onSlashCommand(event); });

    m_bot.on_ready([this](const dpp::ready_t& event) {
        if (dpp::run_once<struct register_bot_commands>()) {
            for (const auto& command : m_slashCommands)
                m_bot.global_command_create(dpp::slashcommand(command.command, command.description, m_bot.me.id));
        }
    });
}

void DiscordBot::start()
{
    m_bot.start(dpp::st_wait);
}

void DiscordBot::onSlashCommand(const dpp::slashcommand_t& event)
{
    const auto& commandName = event.command.get_command_name();
    const auto it = std::find_if(m_slashCommands.begin(), m_slashCommands.end(),
        [commandName](const SlashCommand& command)
        {
            return commandName == command.command;
        }
    );
    if (it != m_slashCommands.end())
        it->execute(event);
}

void DiscordBot::onPingCommand(const dpp::slashcommand_t &event)
{
    event.reply("Pong!");
}

void DiscordBot::onYippeeCommand(const dpp::slashcommand_t &event)
{
    event.reply("Yippee!!");
}

void DiscordBot::onSetStatusChannel(const dpp::slashcommand_t& event)
{
    const auto guild = event.command.guild_id;
    const auto channel = event.command.channel_id;
    m_serverStatusChannel.insert_or_assign(guild, channel);
    dpp::message msg(channel, "Penis message");
    msg.add_component(
        dpp::component().add_component(
            dpp::component()
                .set_label("Meow!")
                .set_type(dpp::cot_button)
                .set_emoji(dpp::unicode_emoji::cat)
                .set_style(dpp::cos_primary)
                .set_id("statuschannel")
        )
    );
    m_bot.message_create(msg);
}
