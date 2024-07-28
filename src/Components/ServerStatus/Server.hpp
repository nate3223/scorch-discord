#pragma once

#include "Document.hpp"
#include "ServerButton.hpp"
#include "StaticCache.hpp"

#include <dpp/dpp.h>
#include <format>
#include <memory>
#include <mongocxx/client.hpp>
#include <optional>
#include <regex>
#include <string>
#include <vector>

class Server
	: public Document<Server>
{
public:
	static const std::optional<uint64_t>		ParseServerIDFromComponentID(const std::string& componentID, const std::regex& pattern, std::smatch& matches);
	static std::vector<std::unique_ptr<Server>>	FindAll(const mongocxx::client& client);
	static void									BulkRemoveFromDatabase(const std::vector<uint64_t>& ids, const mongocxx::client& client);

	Server() = default;
	Server(const uint64_t id, const std::string& name, const std::string& address, const uint64_t guildID, const std::string& url);
	Server(const bsoncxx::document::view& view);

	// Document
	bsoncxx::document::value		getValue() const override;
	// /Document

	void							insertIntoDatabase(const mongocxx::client& client);
	void							updateCustomButtons(const mongocxx::client& client);

	dpp::embed						getEmbed() const;
	std::vector<dpp::component>		getButtonRows() const;
	dpp::component					getSettingsButton() const;
	std::vector<dpp::component>		getServerSettingsRows() const;
	dpp::interaction_modal_response	getAddCustomButtonModal() const;
	dpp::component					getRemoveCustomButtonComponent() const;

	bool							onCustomButtonPressed(const std::smatch& matches);
	std::optional<ServerButton>		getServerButton(const std::smatch& matches);

	uint64_t					m_id{ 0 };
	std::string					m_name;
	std::string					m_address;
	uint64_t					m_guildID{ 0 };
	std::string					m_url;
	std::vector<ServerButton>	m_buttons;

private:
	std::string					formatComponentID(const char* prefix) const;
	dpp::component				getServerButtonsSelectMenuComponent() const;


public:
	struct CustomButton
	{
		static constexpr auto ButtonPrefix		= "ServerStatusCustomButtonID";
		static const std::regex ButtonPattern;
		static constexpr auto MaxButtons		= 19;
		static constexpr auto ButtonsPerRow		= 5;
	};

	struct Settings
	{
		static constexpr auto ButtonPrefix		= "ServerSettingsButtonID";
		static const std::regex ButtonPattern;
	};

	struct AddCustomButton
	{
		static constexpr auto ButtonPrefix		= "ServerSettingsAddCustomButton";
		static const std::regex ButtonPattern;
		static constexpr auto FormPrefix		= "ServerSettingsAddCustomButtonModal";
		static const std::regex FormPattern;
		static constexpr auto Name				= "ServerSettingsAddCustomButtonModalName";
		static constexpr auto Endpoint			= "ServerSettingsAddCustonButtonModalEndpoint";
	};
	
	struct RemoveCustomButton
	{
		static constexpr auto ButtonPrefix		= "ServerSettingsRemoveCustomButton";
		static const std::regex ButtonPattern;
		static constexpr auto OptionPrefix		= "ServerSettingsRemoveCustomButtonSelectOption";
		static const std::regex OptionPattern;
	};
};

DEFINE_STATIC_CACHE(Servers, Server)