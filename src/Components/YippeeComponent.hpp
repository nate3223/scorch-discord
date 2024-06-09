#pragma once

#include "Component.hpp"

class YippeeComponent
	: public Component
{
public:
	YippeeComponent(DiscordBot& bot);

	void    onYippeeCommand(const dpp::slashcommand_t& event);
};