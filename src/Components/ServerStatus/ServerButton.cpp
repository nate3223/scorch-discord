#include "ServerButton.hpp"

#include "Server.hpp"

#include <format>

namespace
{
	namespace Database
	{
		constexpr char ID[]			= "id";
		constexpr char Name[]		= "name";
		constexpr char Endpoint[]	= "endpoint";
	}
}

ServerButton::ServerButton(const uint64_t id, const std::string& name, const std::string& endpoint, const uint64_t serverID)
	: m_id(id)
	, m_name(name)
	, m_endpoint(endpoint)
{
	m_componentID = formatComponentID(serverID);
}

ServerButton::ServerButton(const bsoncxx::document::view& view, const uint64_t serverID)
{
	if (const auto& id = view[Database::ID]; id)
		m_id = (int64_t)id.get_int64().value;
	if (const auto& name = view[Database::Name]; name)
		m_name = std::string(name.get_string().value);
	if (const auto& endpoint = view[Database::Endpoint]; endpoint)
		m_endpoint = std::string(endpoint.get_string().value);
	m_componentID = formatComponentID(serverID);
}

bsoncxx::document::value ServerButton::getValue() const
{
	return make_document(
		kvp(Database::ID, (int64_t)m_id),
		kvp(Database::Name, m_name.c_str()),
		kvp(Database::Endpoint, m_endpoint.c_str())
	);
}

std::string ServerButton::formatComponentID(const uint64_t serverID)
{
	return std::format("{}|{}|{}", Server::CustomButton::ButtonPrefix, std::to_string(serverID), std::to_string(m_id));
}

bool ServerButton::press()
{
	printf("Button %ull pressed to execute at %s\n", m_id, m_endpoint.c_str());
	return true;
}
