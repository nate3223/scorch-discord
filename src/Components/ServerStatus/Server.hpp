#pragma once

#include "Document.hpp"
#include "ServerButton.hpp"
#include "StaticCache.hpp"

#include <dpp/dpp.h>
#include <format>
#include <memory>
#include <mongocxx/client.hpp>
#include <regex>
#include <string>
#include <vector>

class Server
	: public Document<Server>
{
public:
	struct Buttons
	{
		static constexpr auto	CustomButtonPrefix = "ServerStatusCustomButtonID";
		static const std::regex	CustomButtonPattern;
		static constexpr auto	ServerSettingsPrefix = "ServerSettingsButtonID";
		static const std::regex	ServerSettingsPattern;
		static constexpr auto	AddCustomButtonPrefix = "ServerSettingsAddCustomButton";
		static const std::regex	AddCustomButtonPattern;
		static constexpr auto	RemoveCustomButtonPrefix = "ServerSettingsRemoveCustomButton";
		static const std::regex	RemoveCustomButtonPattern;
	};
	
	static const std::optional<uint64_t>		ParseServerIDFromComponentID(const std::string& componentID, const std::regex& pattern, std::smatch& matches);

	static constexpr auto						ButtonsPerRow = 5;

	static std::vector<std::unique_ptr<Server>>	FindAll(const mongocxx::client& client);
	static void									BulkRemoveFromDatabase(const std::vector<uint64_t>& ids, const mongocxx::client& client);

	Server() = default;
	Server(const uint64_t id, const std::string& name, const std::string& address, const uint64_t guildID);
	Server(const bsoncxx::document::view& view);

	// Document
	bsoncxx::document::value	getValue() const override;
	// /Document

	std::string					formatComponentID(const char* prefix);

	void						insertIntoDatabase(const mongocxx::client& client);

	dpp::embed					getEmbed() const;
	std::vector<dpp::component>	getButtonRows() const;
	dpp::component				getSettingsButton() const;
	std::vector<dpp::component>	getServerSettingsRows() const;

	bool						onCustomButtonPressed(const std::smatch& matches);

	uint64_t					m_id{ 0 };
	std::string					m_name;
	std::string					m_address;
	uint64_t					m_guildID{ 0 };
	std::vector<ServerButton>	m_buttons;

private:
	std::string					formatServerSettingsButtonID() const;
};

DEFINE_STATIC_CACHE(Servers, Server)