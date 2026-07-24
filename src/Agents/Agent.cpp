#include "Agent.hpp"
#include "Agent_p.hpp"

Agent::Agent()
	: m_p(std::make_unique<AgentPrivate>())
{

}

Agent::~Agent() = default;
