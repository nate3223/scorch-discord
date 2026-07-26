#include "MongoDBAgentIdentityStore.hpp"

#include "DatabaseManager.hpp"
#include "Document.hpp"
#include "MongoDBManager.hpp"

#include <bsoncxx/builder/basic/document.hpp>


namespace
{
	namespace Database
	{
		constexpr char Collection[] = "AgentIdentities";
		constexpr char UUID[]		= "uuid";
		constexpr char PublicKey[]	= "publicKey";
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
}

bool MongoDBAgentIdentityStore::loadPublicKey(std::string_view uuid, std::vector<std::byte>& publicKey)
{
	auto client = m_pool.acquire();
	auto collection = client->database(MongoDB::DATABASE_NAME)[Database::Collection];

	auto result = collection.find_one(
		make_document(
			kvp(Database::UUID, uuid)
		)
	);

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
