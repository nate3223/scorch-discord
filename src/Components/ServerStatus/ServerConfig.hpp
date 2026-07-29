#pragma once

#include "Components/Common/StaticCache.hpp"
#include "Database/MongoDB/Document.hpp"
#include "StatusWidget.hpp"

#include <cstdint>
#include <memory>
#include <mongocxx/client-fwd.hpp>
#include <shared_mutex>
#include <vector>

class ServerConfigPrivate;
class ServerConfig final
	: public Document<ServerConfig>
{
public:
	static std::vector<std::unique_ptr<ServerConfig>>	FindAll(const mongocxx::client& client);

								ServerConfig();
	explicit					ServerConfig(const bsoncxx::document::view& view);
								~ServerConfig();

	void						insertIntoDatabase(const mongocxx::client& client);
	void						removeFromDatabase(const mongocxx::client& client);
	void						updateChannelID(const mongocxx::client& client);
	void						updateServerIDs(const mongocxx::client& client);
	void						updateStatusWidget(const mongocxx::client& client);

	uint64_t					guildId() const noexcept;
	void						setGuildId(uint64_t guildId) noexcept;
	uint64_t					channelId() const noexcept;
	void						setChannelId(uint64_t channelId) noexcept;
	StatusWidget&				statusWidget() noexcept;
	const StatusWidget&			statusWidget() const noexcept;
	void						setStatusWidget(StatusWidget statusWidget);
	std::vector<uint64_t>&		serverIds() noexcept;
	const std::vector<uint64_t>& serverIds() const noexcept;
	void						setServerIds(std::vector<uint64_t> serverIds);
	std::shared_mutex&			mutex() const noexcept;

// Document i/f:
public:
	bsoncxx::document::value	getValue() const override;

private:
	std::unique_ptr<ServerConfigPrivate>	m_p;
};

using ServerConfigs = StaticCache<ServerConfig>;
