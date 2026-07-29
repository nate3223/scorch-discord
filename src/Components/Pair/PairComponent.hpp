#pragma once

#include "Components/Component.hpp"

class DiscordBot;

class PairComponentPrivate;
class PairComponent
	: public Component
{
public:
	explicit		PairComponent(DiscordBot& bot);
					~PairComponent();

private:
	dpp::task<void>	onPairRequest(const dpp::slashcommand_t& event);
	void			onShareAgentRequest(const dpp::slashcommand_t& event);

	std::unique_ptr<PairComponentPrivate>	m_p;
};
