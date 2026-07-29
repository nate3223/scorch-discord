#include "MongoDBAgentIdentityStore.hpp"

#include "Database/DatabaseManager.hpp"
#include "Database/MongoDB/Document.hpp"
#include "Database/MongoDB/MongoDBManager.hpp"

#include <bsoncxx/builder/basic/document.hpp>


namespace
{
	namespace Database
	{
		constexpr char Collection[] = "AgentIdentities";
		constexpr char UUID[]		= "uuid";
		constexpr char PublicKey[]	= "publicKey";
		constexpr char GuildId[]	= "guildId";
	}
}

MongoDBAgentIdentityStore::MongoDBAgentIdentityStore()
	: m_pool(DatabaseManager::GetInstance<MongoDBManager>()->getPool())
{
	auto client = m_pool.acquire();
	auto collection = client->database(MongoDB::DATABASE_NAME)[Database::Collection];

	collection.create_index(
		make_document(
			kvp(Database::UUID, 1)
		),
		mongocxx::options::index{}.unique(true)
	);

	collection.create_index(
		make_document(
			kvp(Database::GuildId, 1)
		),
		mongocxx::options::index{}
	);
}

bool MongoDBAgentIdentityStore::loadAgentGuildId(std::string_view uuid, std::string& guildId)
{
	auto client = m_pool.acquire();
	auto collection = client->database(MongoDB::DATABASE_NAME)[Database::Collection];

	static const auto kProjection = make_document(
		kvp(Database::GuildId, 1),
		kvp("_id", 0)
	);

	mongocxx::options::find options{};
	options.projection(kProjection.view());

	auto filter = make_document(
		kvp(Database::UUID, uuid)
	);

	try
	{
		auto result = collection.find_one(filter.view(), options);
		if (! result)
			return false;

		auto view = result->view();
		auto elem = view[Database::GuildId];

		if (! elem || elem.type() != bsoncxx::type::k_string)
			return false;

		guildId = std::string(elem.get_string().value);
	}
	catch (const std::exception& e)
	{
		return false;
	}

	return true;
}

bool MongoDBAgentIdentityStore::loadAgentFromGuildId(std::string_view guildId, std::string& uuid)
{
	auto client = m_pool.acquire();
	auto collection = client->database(MongoDB::DATABASE_NAME)[Database::Collection];

	static const auto kProjection = make_document(
		kvp(Database::UUID, 1),
		kvp("_id", 0)
	);

	mongocxx::options::find options{};
	options.projection(kProjection.view());

	auto filter = make_document(
		kvp(Database::GuildId, guildId)
	);

	try
	{
		// Assuming one to one relationship for now
		auto result = collection.find_one(filter.view(), options);
		if (! result)
			return false;

		auto view = result->view();
		auto elem = view[Database::UUID];
		if (! elem || elem.type() != bsoncxx::type::k_string)
			return false;

		uuid = std::string(elem.get_string().value);
	}
	catch (const std::exception& e)
	{
		return false;
	}

	return true;
}

bool MongoDBAgentIdentityStore::saveAgentGuildId(std::string_view uuid, std::string_view guildId)
{
	auto client = m_pool.acquire();
	auto collection = client->database(MongoDB::DATABASE_NAME)[Database::Collection];

	// Agent exists, but doesn't have a guild ID assigned
	auto filter = make_document(
		kvp(Database::UUID, uuid),
		kvp(Database::GuildId, make_document(
			kvp("$exists", false)
		))
	);

	auto updateGuildId = make_document(
		kvp("$set", make_document(
			kvp(Database::GuildId, guildId)
		))
	);

	try
	{
		auto result = collection.update_one(filter.view(), updateGuildId.view());
		return result && result->modified_count() > 0;
	}
	catch (const std::exception&)
	{
		return false;
	}
}

bool MongoDBAgentIdentityStore::loadPublicKey(std::string_view uuid, std::vector<std::byte>& publicKey)
{
	auto client = m_pool.acquire();
	auto collection = client->database(MongoDB::DATABASE_NAME)[Database::Collection];

	auto filter = make_document(
		kvp(Database::UUID, uuid)
	);

	try
	{
		auto result = collection.find_one(filter.view());

		if (! result)
			return false;

		auto view = result->view();
		auto publicKeyView = view[Database::PublicKey];
		if (! publicKeyView || publicKeyView.type() != bsoncxx::type::k_binary)
			return false;

		auto binary = publicKeyView.get_binary();

		publicKey.assign(
			reinterpret_cast<const std::byte*>(binary.bytes),
			reinterpret_cast<const std::byte*>(binary.bytes + binary.size)
		);
	}
	catch (const std::exception& e)
	{
		return false;
	}

	return true;
}

bool MongoDBAgentIdentityStore::savePublicKey(std::string_view uuid, std::span<const std::byte>& publicKey)
{
	auto client = m_pool.acquire();
	auto collection = client->database(MongoDB::DATABASE_NAME)[Database::Collection];

	auto document = make_document(
		kvp(Database::UUID, uuid),
		kvp(Database::PublicKey, bsoncxx::types::b_binary{
			bsoncxx::binary_sub_type::k_binary,
			static_cast<std::uint32_t>(publicKey.size()),
			reinterpret_cast<const std::uint8_t*>(publicKey.data())
		})
	);

	try
	{
		auto result = collection.insert_one(document.view());
		return result.has_value();
	}
	catch (std::exception& e)
	{
		// Duplicate UUID
		return false;
	}
}
