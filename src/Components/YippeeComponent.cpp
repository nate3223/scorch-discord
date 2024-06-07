#include "YippeeComponent.hpp"

YippeeComponent::YippeeComponent(dpp::cluster& bot)
	: Component(bot)
{
	m_slashCommands.emplace_back(std::bind_front(&YippeeComponent::onYippeeCommand, this), dpp::slashcommand("yippee", "Yippee!", m_bot.me.id));
}

void YippeeComponent::onYippeeCommand(const dpp::slashcommand_t& event)
{
	event.reply("Yippee!!");
}

