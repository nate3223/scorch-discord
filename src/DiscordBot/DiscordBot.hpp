#pragma once

#include <dpp/dpp.h>
#include <string.h>

class DiscordBot
{
public:
            DiscordBot(const char* token);

    void    start();

private:
    void    onSlashCommand(const dpp::slashcommand_t& event);

    dpp::cluster    m_bot;
};