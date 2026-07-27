#pragma once

#include "Component.hpp"

class DiscordBot;
class AgentsManager;

class PairComponent
	: public Component
{
public:
	explicit		PairComponent(DiscordBot& bot);

private:
	dpp::task<void>	onPairRequest(const dpp::slashcommand_t& event);

	AgentsManager&	m_agentsManager;
};
