#include "Server.hpp"

#include <format>

namespace
{		
	namespace Database
	{
		constexpr char Collection[] = "Servers";
		constexpr char ID[]			= "id";
		constexpr char Name[]		= "name";
		constexpr char Address[]	= "address";
		constexpr char URL[]		= "url";
		constexpr char GuildID[]	= "guildID";
		constexpr char Buttons[]	= "buttons";
		constexpr char Endpoint[]	= "endpoint";
	}

	std::regex formatRegexPattern(const std::string& prefix)
	{
		return std::regex(std::format("^{}\\|([0-9]+)(?:\\|([0-9]+))?$", prefix));
	}
}

Cache<Server> Servers::g_cache;

const std::regex Server::CustomButton::ButtonPattern		= formatRegexPattern(Server::CustomButton::ButtonPrefix);
const std::regex Server::Settings::ButtonPattern			= formatRegexPattern(Server::Settings::ButtonPrefix);
const std::regex Server::AddCustomButton::ButtonPattern		= formatRegexPattern(Server::AddCustomButton::ButtonPrefix);
const std::regex Server::RemoveCustomButton::ButtonPattern	= formatRegexPattern(Server::RemoveCustomButton::ButtonPrefix);
const std::regex Server::AddCustomButton::FormPattern		= formatRegexPattern(Server::AddCustomButton::FormPrefix);
const std::regex Server::RemoveCustomButton::OptionPattern	= formatRegexPattern(Server::RemoveCustomButton::OptionPrefix);

const std::optional<uint64_t> Server::ParseServerIDFromComponentID(const std::string& componentID, const std::regex& pattern, std::smatch& matches)
{
	// TODO: Use boost-regex
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

Server::Server(const uint64_t id, const std::string& name, const std::string& address, const uint64_t guildID, const std::string& url)
	: m_id(id)
	, m_name(name)
	, m_address(address)
	, m_guildID(guildID)
	, m_url(url)
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
	if (const auto& url = view[Database::URL]; url)
		m_url = std::string(url.get_string().value);
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
		kvp(Database::GuildID, (int64_t)m_guildID),
		kvp(Database::URL, m_url.c_str())
	);
}

void Server::insertIntoDatabase(const mongocxx::client& client)
{
	client.database(MongoDB::DATABASE_NAME)[Database::Collection].insert_one(getValue());
}

void Server::updateCustomButtons(const mongocxx::client& client)
{
	array arr = ServerButton::ConstructArray(m_buttons);
	client.database(MongoDB::DATABASE_NAME)[Database::Collection].update_one(
		make_document(kvp(Database::ID, (int64_t)m_id)),
		make_document(kvp("$set", make_document(kvp(Database::Buttons, arr))))
	);
}

dpp::embed Server::getEmbed() const
{
	return dpp::embed()
		.set_title(m_name)
		.add_field("IP Address", m_url)
		.add_field("More info coming soon", "")
		.set_timestamp(time(0));
}

std::vector<dpp::component> Server::getButtonRows() const
{
	std::vector<dpp::component> rows;
	for (size_t i = 0; i < m_buttons.size(); i += CustomButton::ButtonsPerRow)
	{
		dpp::component row;
		for (size_t j = 0; i + j < m_buttons.size() && j < CustomButton::ButtonsPerRow; j++)
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
	return rows;
}

dpp::component Server::getSettingsButton() const
{
	return dpp::component()
		.set_label("Server settings")
		.set_id(formatComponentID(Settings::ButtonPrefix))
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
					.set_id(std::format("{}|{}", AddCustomButton::ButtonPrefix, m_id))
					.set_type(dpp::cot_button)
			).add_component(
				dpp::component()
					.set_label("Remove button")
					.set_id(std::format("{}|{}", RemoveCustomButton::ButtonPrefix, m_id))
					.set_type(dpp::cot_button)
			)
	);
	return rows;
}

dpp::interaction_modal_response Server::getAddCustomButtonModal() const
{
	dpp::interaction_modal_response modal(formatComponentID(AddCustomButton::FormPrefix), "Add a button");
	modal.add_component(
		dpp::component()
		.set_label("Label")
		.set_id(AddCustomButton::Name)
		.set_type(dpp::cot_text)
		.set_text_style(dpp::text_short)
		.set_required(true)
		.set_max_length(80)
	);

	modal.add_row();
	modal.add_component(
		dpp::component()
		.set_label("Endpoint")
		.set_id(AddCustomButton::Endpoint)
		.set_type(dpp::cot_text)
		.set_text_style(dpp::text_short)
		.set_required(true)
	);

	return modal;
}

dpp::component Server::getRemoveCustomButtonComponent() const
{
	return dpp::component()
		.add_component(getServerButtonsSelectMenuComponent()
			.set_id(formatComponentID(RemoveCustomButton::OptionPrefix))
			.set_min_values(1)
			.set_max_values(m_buttons.size())
			.set_placeholder("Select buttons to remove")
		);
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

	if (auto it = std::find_if(m_buttons.begin(), m_buttons.end(), [buttonID](const ServerButton& button) { return button.m_id == buttonID; }); it != m_buttons.end())
		return it->press();
	return false;
}

std::optional<ServerButton> Server::getServerButton(const std::smatch& matches)
{
	uint64_t buttonID;
	try
	{
		buttonID = std::stoull(matches[2]);
	}
	catch (const std::exception& e)
	{
		return std::nullopt;
	}

	if (auto it = std::find_if(m_buttons.begin(), m_buttons.end(), [buttonID](const ServerButton& button) { return button.m_id == buttonID; }); it != m_buttons.end())
		return *it;

	return std::nullopt;
}

std::string Server::formatComponentID(const char* prefix) const
{
	return std::format("{}|{}", prefix, m_id);
}

dpp::component Server::getServerButtonsSelectMenuComponent() const
{
	dpp::component component = dpp::component().set_type(dpp::cot_selectmenu);
	for (const auto& button : m_buttons)
		component.add_select_option(dpp::select_option(button.m_name, std::to_string(button.m_id)));
	return component;
}
