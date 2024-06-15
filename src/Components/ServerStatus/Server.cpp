#include "Server.hpp"

namespace
{
	namespace Database
	{
		constexpr char Collection[] = "Servers";
		constexpr char ID[]			= "id";
		constexpr char Name[]		= "name";
		constexpr char Address[]	= "Address";
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

Server::Server(const uint64_t id, const std::string& name, const std::string& address)
	: m_id(id)
	, m_name(name)
	, m_address(address)
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
}

bsoncxx::document::value Server::getValue() const
{
	return make_document(
		kvp(Database::ID, (int64_t)m_id),
		kvp(Database::Name, m_name.c_str()),
		kvp(Database::Address, m_address.c_str())
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
