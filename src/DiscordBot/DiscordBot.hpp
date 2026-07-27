#pragma once

#include <memory>
#include <string>

namespace dpp
{
	class cluster;
}

class AgentsManager;
class ComponentLogMessage;
class DiscordBotPrivate;

class DiscordBot
{
public:
	explicit		DiscordBot(const std::string& token);
					~DiscordBot();

	void			start();

	void			componentLog(std::unique_ptr<ComponentLogMessage> message);
	AgentsManager&	getAgentsManager();

	dpp::cluster& operator*() const;
	dpp::cluster* operator->() const;

private:
	std::unique_ptr<DiscordBotPrivate>	m_p;
};