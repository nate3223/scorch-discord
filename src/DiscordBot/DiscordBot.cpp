#include "DiscordBot.hpp"

DiscordBot::DiscordBot(const char* token)
    : m_bot(token)
{
    m_bot.on_log(dpp::utility::cout_logger());

    m_bot.on_slashcommand([this](const dpp::slashcommand_t& event){ onSlashCommand(event); });

    m_bot.on_ready([this](const dpp::ready_t& event) {
        if (dpp::run_once<struct register_bot_commands>()) {
            m_bot.global_command_create(dpp::slashcommand("ping", "Ping pong!", m_bot.me.id));
            m_bot.global_command_create(dpp::slashcommand("yippee", "Yippee!", m_bot.me.id));
        }
    });
}

void DiscordBot::start()
{
    m_bot.start(dpp::st_wait);
}

void DiscordBot::onSlashCommand(const dpp::slashcommand_t& event)
{
    if (event.command.get_command_name() == "ping")
    {
        event.reply("Pong!");
    }
    else if (event.command.get_command_name() == "yippee")
    {
        event.reply("Yippee!");
    }
}