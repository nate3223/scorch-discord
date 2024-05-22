#pragma once

#include <dpp/dpp.h>
#include <vector>
#include <unordered_map>

struct SlashCommand
{
    const char* command;
    const char* description;
    std::function<void(const dpp::slashcommand_t&)> execute;
};

class DiscordBot
{
public:
            DiscordBot(const char* token);

    void    start();

private:
    void    onSlashCommand(const dpp::slashcommand_t& event);
    void    onPingCommand(const dpp::slashcommand_t& event);
    void    onYippeeCommand(const dpp::slashcommand_t& event);
    void    onSetStatusChannel(const dpp::slashcommand_t& event);

    dpp::cluster                m_bot;
    std::vector<SlashCommand>   m_slashCommands;
    std::unordered_map<dpp::snowflake, dpp::snowflake>  m_serverStatusChannel;
};