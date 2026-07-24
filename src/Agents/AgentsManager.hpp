#pragma once

#include <memory>
#include <string>

class AgentsManagerPrivate;

class AgentsManager
{
public:
	AgentsManager();
	~AgentsManager();

	void	listen(std::string port);

private:
	std::unique_ptr<AgentsManagerPrivate>	m_p;
};
