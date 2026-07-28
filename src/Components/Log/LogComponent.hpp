#pragma once

#include "Components/Component.hpp"

#include <memory>

class LogComponentPrivate;
class LogComponent final
	: public Component
{
public:
	explicit	LogComponent(DiscordBot& bot);
				~LogComponent();

private:
	void		onSetLogChannel(const dpp::slashcommand_t& event);

// Component i/f:
public:
	dpp::task<void>					onChannelDelete(const dpp::channel_delete_t& event) override;
	boost::asio::awaitable<void>	onComponentLog(const ComponentLogMessage* message) override;

private:
	std::unique_ptr<LogComponentPrivate>	m_p;
};
