#pragma once

#include "Cache.hpp"
#include "Component.hpp"
#include "Document.hpp"
#include "MongoDBManager.hpp"
#include "LogConfig.hpp"

#include <boost/unordered/unordered_flat_map.hpp>
#include <memory>
#include <mutex>

class LogConfig;

class LogComponent
	: public Component
{
public:
	LogComponent(DiscordBot& bot);

	void	onSetLogChannel(const dpp::slashcommand_t& event);

	// Component
	void	onChannelDelete(const dpp::channel_delete_t& event) override;
	void	onComponentLog(const ComponentLogMessage* message) override;
	// /Component

private:
	mongocxx::pool&		m_databasePool;
	Cache<LogConfig>	m_configs;
};
