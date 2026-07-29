#pragma once

#include "Database/MongoDB/Document.hpp"

#include <cstdint>
#include <memory>
#include <optional>

namespace dpp
{
	struct message;
}

class Server;

class StatusWidgetPrivate;
class StatusWidget
	: public Document<StatusWidget>
{
public:
									StatusWidget();
	explicit						StatusWidget(const bsoncxx::document::view& view);
									StatusWidget(const StatusWidget& other);
									StatusWidget(StatusWidget&& other) noexcept;
	StatusWidget&					operator=(const StatusWidget& other);
	StatusWidget&					operator=(StatusWidget&& other) noexcept;
									~StatusWidget();

	bsoncxx::document::value		getValue() const override;
	dpp::message					getMessage() const;

	std::optional<uint64_t>&		messageID() noexcept;
	const std::optional<uint64_t>&	messageID() const noexcept;
	void							setMessageID(std::optional<uint64_t> messageID);

	std::optional<uint64_t>&		agentMessageID() noexcept;
	const std::optional<uint64_t>&	agentMessageID() const noexcept;
	void							setAgentMessageID(std::optional<uint64_t> agentMessageID);

	std::optional<uint64_t>&		commandID() noexcept;
	const std::optional<uint64_t>&	commandID() const noexcept;
	void							setCommandID(std::optional<uint64_t> commandID);

	std::shared_ptr<Server>&		activeServer() noexcept;
	const std::shared_ptr<Server>&	activeServer() const noexcept;
	void							setActiveServer(std::shared_ptr<Server> activeServer);

	std::optional<uint64_t>&		activeServerID() noexcept;
	const std::optional<uint64_t>&	activeServerID() const noexcept;
	void							setActiveServerID(std::optional<uint64_t> activeServerID);

private:
	std::unique_ptr<StatusWidgetPrivate>	m_p;
};
