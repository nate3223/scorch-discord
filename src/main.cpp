#include "DiscordBot.hpp"

#include <cstdlib>
#include <stdio.h>

int main(int argc, char* argv[])
{
    const char* api_key = std::getenv("DISCORD_TOKEN");
    if (api_key == nullptr)
    {
        printf("Could not find DISCORD_TOKEN environment variable\n");
        exit(1);
    }

    DiscordBot bot(api_key);
    bot.start();
    return 0;
}