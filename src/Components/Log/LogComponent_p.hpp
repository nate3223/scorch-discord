#pragma once

#include "Components/Common/Cache.hpp"
#include "LogConfig.hpp"

#include <mongocxx/pool.hpp>

class LogComponentPrivate
{
public:
	LogComponentPrivate();

	mongocxx::pool&		m_databasePool;
	Cache<LogConfig>	m_configs;
};