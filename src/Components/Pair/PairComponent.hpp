#pragma once

#include "Component.hpp"

class DiscordBot;

namespace scorch
{
	namespace server
	{
		class AgentsManager;
	}
}

class PairComponent
	: public Component
{
public:
	explicit		PairComponent(DiscordBot& bot);

private:
	dpp::task<void>	onPairRequest(const dpp::slashcommand_t& event);

	scorch::server::Server&	m_scorchServer;
};
