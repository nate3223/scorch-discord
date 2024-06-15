#pragma once

#include "Document.hpp"

#include <dpp/dpp.h>
#include <memory>
#include <mongocxx/client.hpp>
#include <string>
#include <vector>

class Server
	: public Document<Server>
{
public:
	static std::vector<std::unique_ptr<Server>>	FindAll(const mongocxx::client& client);
	static void									BulkRemoveFromDatabase(const std::vector<uint64_t>& ids, const mongocxx::client& client);

	Server() = default;
	Server(const uint64_t id, const std::string& name, const std::string& address);
	Server(const bsoncxx::document::view& view);

	bsoncxx::document::value	getValue() const override;

	void	insertIntoDatabase(const mongocxx::client& client);

	dpp::embed					getEmbed() const;

	uint64_t	m_id{ 0 };
	std::string	m_name;
	std::string	m_address;
};