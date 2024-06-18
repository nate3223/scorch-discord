#include "Server.hpp"

#include <format>

namespace
{
	namespace Button
	{
		constexpr auto MaxCustomButtons			= 19;
	}
	namespace Database
	{
		constexpr char Collection[] = "Servers";
		constexpr char ID[]			= "id";
		constexpr char Name[]		= "name";
		constexpr char Address[]	= "address";
		constexpr char GuildID[]	= "guildID";
		constexpr char Buttons[]	= "buttons";
		constexpr char Endpoint[]	= "endpoint";
	}

	std::regex formatRegexPattern(const std::string& prefix)
	{
		return std::regex(std::format("^{}\\|([0-9]+)(?:\\|([0-9]+))?$", prefix, "", ""));
	}
}

Cache<Server> Servers::g_cache;

const std::regex Server::Buttons::CustomButtonPattern		= formatRegexPattern(Server::Buttons::CustomButtonPrefix);
const std::regex Server::Buttons::ServerSettingsPattern		= formatRegexPattern(Server::Buttons::ServerSettingsPrefix);
const std::regex Server::Buttons::AddCustomButtonPattern	= formatRegexPattern(Server::Buttons::AddCustomButtonPrefix);
const std::regex Server::Buttons::RemoveCustomButtonPattern	= formatRegexPattern(Server::Buttons::RemoveCustomButtonPrefix);

const std::optional<uint64_t> Server::ParseServerIDFromComponentID(const std::string& componentID, const std::regex& pattern, std::smatch& matches)
{
	if (!std::regex_search(componentID, matches, pattern) || matches.size() != 3)
		return {};

	try
	{
		return std::stoull(matches[1]);
	}
	catch (const std::exception& e)
	{
		return {};
	}
}

std::vector<std::unique_ptr<Server>> Server::FindAll(const mongocxx::client& client)
{
	std::vector<std::unique_ptr<Server>> servers;
	auto serverCursor = client.database(MongoDB::DATABASE_NAME)[Database::Collection].find({});
	for (const auto& doc : serverCursor)
		servers.emplace_back(new Server(doc));
	return servers;
}

void Server::BulkRemoveFromDatabase(const std::vector<uint64_t>& ids, const mongocxx::client& client)
{
	array arr;
	for (const uint64_t id : ids)
		arr.append((int64_t)id);
	client.database(MongoDB::DATABASE_NAME)[Database::Collection].delete_many(make_document(kvp(Database::ID, make_document(kvp("$in", arr)))));
}

Server::Server(const uint64_t id, const std::string& name, const std::string& address, const uint64_t guildID)
	: m_id(id)
	, m_name(name)
	, m_address(address)
	, m_guildID(guildID)
{
}

Server::Server(const bsoncxx::document::view& view)
{
	if (const auto& id = view[Database::ID]; id)
		m_id = (uint64_t)id.get_int64().value;
	if (const auto& name = view[Database::Name]; name)
		m_name = std::string(name.get_string().value);
	if (const auto& address = view[Database::Address]; address)
		m_address = std::string(address.get_string().value);
	if (const auto& guildID = view[Database::GuildID]; guildID)
		m_guildID = (uint64_t)guildID.get_int64().value;
	if (const auto& buttons = view[Database::Buttons]; buttons)
	{
		for (const auto& doc : buttons.get_array().value)
			m_buttons.emplace_back(doc.get_document(), m_id);
	}
}

bsoncxx::document::value Server::getValue() const
{
	return make_document(
		kvp(Database::ID, (int64_t)m_id),
		kvp(Database::Name, m_name.c_str()),
		kvp(Database::Address, m_address.c_str()),
		kvp(Database::GuildID, (int64_t)m_guildID)
	);
}

void Server::insertIntoDatabase(const mongocxx::client& client)
{
	client.database(MongoDB::DATABASE_NAME)[Database::Collection].insert_one(getValue());
}

dpp::embed Server::getEmbed() const
{
	return dpp::embed()
		.set_title(m_name)
		.add_field("IP Address", "TestServer.com")
		.add_field("Player Count", "10")
		.set_timestamp(time(0));
}

std::vector<dpp::component> Server::getButtonRows() const
{
	std::vector<dpp::component> rows;
	for (size_t i = 0; i < m_buttons.size(); i += ButtonsPerRow)
	{
		dpp::component row;
		for (size_t j = 0; i + j < m_buttons.size() && j < ButtonsPerRow; j++)
		{
			row.add_component(
				dpp::component()
					.set_label(m_buttons[i + j].m_name)
					.set_id(m_buttons[i + j].m_componentID)
					.set_type(dpp::cot_button)
			);
		}
		rows.push_back(std::move(row));
	}
	return std::vector<dpp::component>();
}

dpp::component Server::getSettingsButton() const
{
	return dpp::component()
		.set_label("Server settings")
		.set_id(formatServerSettingsButtonID())
		.set_type(dpp::cot_button);
}

std::vector<dpp::component> Server::getServerSettingsRows() const
{
	std::vector<dpp::component> rows;
	rows.push_back(
		dpp::component()
			.add_component(
				dpp::component()
					.set_label("Add button")
					.set_id(std::format("{}|{}", Buttons::AddCustomButtonPrefix, m_id))
					.set_type(dpp::cot_button)
			).add_component(
				dpp::component()
					.set_label("Remove button")
					.set_id(std::format("{}|{}", Buttons::RemoveCustomButtonPrefix, m_id))
					.set_type(dpp::cot_button)
			)
	);
	return rows;
}

bool Server::onCustomButtonPressed(const std::smatch& matches)
{
	uint64_t buttonID;
	try
	{
		buttonID = std::stoull(matches[2]);
	}
	catch (const std::exception& e)
	{
		return false;
	}

	if (auto it = std::find(m_buttons.begin(), m_buttons.end(), [buttonID](const ServerButton& button) { return button.m_id == buttonID; }); it != m_buttons.end())
		return it->press();
	return false;
}

std::string Server::formatServerSettingsButtonID() const
{
	return std::format("{}|{}", Buttons::ServerSettingsPrefix, std::to_string(m_id));
}