#pragma once

#include "Document.hpp"
#include "Server.hpp"

#include <dpp/dpp.h>
#include <optional>

class StatusWidget
	: public Document<StatusWidget>
{
public:
	StatusWidget() = default;
	StatusWidget(const bsoncxx::document::view& view);

	bsoncxx::document::value	getValue() const override;

	dpp::message				getMessage() const;

	std::optional<uint64_t>	m_messageID;
	std::optional<uint64_t>	m_commandID;	// Do not save to database
	Server*					m_activeServer;	// Do not save to database
	std::optional<uint64_t>	m_activeServerID;
};