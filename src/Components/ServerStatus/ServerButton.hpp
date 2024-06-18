#pragma once

#include "Document.hpp"
#include "StaticCache.hpp"

#include <string>

class ServerButton
	: public Document<ServerButton>
{
public:
	ServerButton() = default;
	ServerButton(const uint64_t id, const std::string& name, const std::string& endpoint, const uint64_t serverID);
	ServerButton(const bsoncxx::document::view& view, const uint64_t guildID);

	// Document
	bsoncxx::document::value	getValue() const override;
	// /Document

	bool						press();

	uint64_t	m_id{ 0 };
	std::string	m_name;
	std::string	m_endpoint;
	std::string	m_componentID; // Do not save to database

private:
	std::string					formatComponentID(const uint64_t guildID);
};