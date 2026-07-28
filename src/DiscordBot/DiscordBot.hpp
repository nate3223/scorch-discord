#pragma once

#include <memory>
#include <string>

namespace dpp
{
	class cluster;
}

class ComponentLogMessage;
class DiscordBotPrivate;

class DiscordBot
{
public:
	explicit				DiscordBot(const std::string& token);
							~DiscordBot();

	void					start();

	void					componentLog(std::unique_ptr<ComponentLogMessage> message);

	dpp::cluster& operator*() const;
	dpp::cluster* operator->() const;

private:
	std::unique_ptr<DiscordBotPrivate>	m_p;
};