#pragma once

#include <cstdint>
#include <string>

class ServerButtonPrivate
{
public:
	uint64_t	m_id = 0;
	std::string	m_name;
	std::string	m_endpoint;
	std::string	m_method;
	std::string	m_componentID; // Do not save to database
};
