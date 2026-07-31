#include "DiscordBot/DiscordBot.hpp"
#include "Log.hpp"

#include <cstdlib>
#include <exception>

int main(int argc, char* argv[])
{
	const char* api_key = std::getenv("DISCORD_TOKEN");
	if (api_key == nullptr)
	{
		Logger::App().critical("DISCORD_TOKEN environment variable is not set");
		return EXIT_FAILURE;
	}

	try
	{
		DiscordBot bot(api_key);
		bot.start();
	}
	catch (const std::exception& error)
	{
		Logger::App().critical("ScorchDiscord failed: {}", error.what());
		return EXIT_FAILURE;
	}

	return 0;
}
