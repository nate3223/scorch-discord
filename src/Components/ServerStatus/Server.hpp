#pragma once

#include "Document.hpp"

#include <dpp/dpp.h>
#include <format>
#include <memory>
#include <mongocxx/client.hpp>
#include <regex>
#include <string>
#include <vector>

class ServerButton;

class Server
	: public Document<Server>
{
public:
	static constexpr auto						CustomButtonPrefix = "ServerStatusCustomButtonID";
	static const std::regex						CustomButtonPattern;

	static constexpr auto						ButtonsPerRow = 5;

	static std::vector<std::unique_ptr<Server>>	FindAll(const mongocxx::client& client);
	static void									BulkRemoveFromDatabase(const std::vector<uint64_t>& ids, const mongocxx::client& client);

	Server() = default;
	Server(const uint64_t id, const std::string& name, const std::string& address);
	Server(const bsoncxx::document::view& view);

	// Document
	bsoncxx::document::value	getValue() const override;
	// /Document

	void						insertIntoDatabase(const mongocxx::client& client);

	dpp::embed					getEmbed() const;
	std::vector<dpp::component>	getButtonRows() const;

	bool						onCustomButtonPressed(const std::string& buttonName);

	uint64_t					m_id{ 0 };
	std::string					m_name;
	std::string					m_address;
	std::vector<ServerButton>	m_buttons;
};

class ServerButton
	: public Document<ServerButton>
{
public:
	ServerButton() = default;
	ServerButton(const std::string& name, const std::string& endpoint);
	ServerButton(const bsoncxx::document::view& view);

	// Document
	bsoncxx::document::value	getValue() const override;
	// /Document

	std::string	m_name;
	std::string	m_endpoint;
};