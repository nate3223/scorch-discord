#pragma once

#include <memory>

class AgentPrivate;

class Agent
{
public:
	Agent();
	~Agent();

private:
	std::unique_ptr<AgentPrivate>	m_p;
};
