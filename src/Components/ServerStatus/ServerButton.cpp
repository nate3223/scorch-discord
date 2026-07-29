#include "ServerButton.hpp"
#include "ServerButton_p.hpp"

#include "Server.hpp"

#include <format>

namespace
{
	namespace Database
	{
		constexpr char ID[]			= "id";
		constexpr char Name[]		= "name";
		constexpr char Endpoint[]	= "endpoint";
		constexpr char Method[]		= "method";
	}
}

ServerButton::ServerButton()
	: m_p(std::make_unique<ServerButtonPrivate>())
{

}

ServerButton::ServerButton(uint64_t id, std::string name, std::string endpoint, std::string method, uint64_t serverID)
	: ServerButton()
{
	m_p->m_id = id;
	m_p->m_name = std::move(name);
	m_p->m_endpoint = std::move(endpoint);
	m_p->m_method = std::move(method);
	m_p->m_componentID = formatComponentId(serverID, id);
}

ServerButton::ServerButton(const bsoncxx::document::view& view, const uint64_t serverID)
	: ServerButton()
{
	if (const auto& id = view[Database::ID]; id)
		m_p->m_id = (int64_t)id.get_int64().value;
	if (const auto& name = view[Database::Name]; name)
		m_p->m_name = std::string(name.get_string().value);
	if (const auto& endpoint = view[Database::Endpoint]; endpoint)
		m_p->m_endpoint = std::string(endpoint.get_string().value);
	if (const auto& method = view[Database::Method]; method)
		m_p->m_method = std::string(method.get_string().value);
	m_p->m_componentID = formatComponentId(serverID, m_p->m_id);
}

ServerButton::ServerButton(const ServerButton& other)
	: m_p(std::make_unique<ServerButtonPrivate>(*other.m_p))
{
}

ServerButton::ServerButton(ServerButton&& other) noexcept = default;

ServerButton& ServerButton::operator=(const ServerButton& other)
{
	if (this != &other)
		m_p = std::make_unique<ServerButtonPrivate>(*other.m_p);
	return *this;
}

ServerButton& ServerButton::operator=(ServerButton&& other) noexcept = default;

ServerButton::~ServerButton() = default;

bsoncxx::document::value ServerButton::getValue() const
{
	return make_document(
		kvp(Database::ID, (int64_t)m_p->m_id),
		kvp(Database::Name, m_p->m_name.c_str()),
		kvp(Database::Endpoint, m_p->m_endpoint.c_str()),
		kvp(Database::Method, m_p->m_method.c_str())
	);
}

bool ServerButton::press()
{
	return true;
}

uint64_t ServerButton::id() const noexcept
{
	return m_p->m_id;
}

const std::string& ServerButton::name() const noexcept
{
	return m_p->m_name;
}

const std::string& ServerButton::endpoint() const noexcept
{
	return m_p->m_endpoint;
}

const std::string& ServerButton::method() const noexcept
{
	return m_p->m_method;
}

const std::string& ServerButton::componentId() const noexcept
{
	return m_p->m_componentID;
}

std::string ServerButton::formatComponentId(uint64_t serverId, uint64_t id)
{
	return std::format("{}|{}|{}", Server::CustomButton::ButtonPrefix, serverId, id);
}
