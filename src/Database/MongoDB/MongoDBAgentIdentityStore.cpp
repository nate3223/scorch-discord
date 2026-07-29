#include "MongoDBAgentIdentityStore.hpp"

#include "Database/DatabaseManager.hpp"
#include "Database/MongoDB/Document.hpp"
#include "Database/MongoDB/MongoDBManager.hpp"
#include "Log.hpp"

#include <bsoncxx/builder/basic/document.hpp>


namespace
{
	namespace Database
	{
		constexpr char Collection[] = "AgentIdentities";
		constexpr char UUID[]		= "uuid";
		constexpr char PublicKey[]	= "publicKey";
		constexpr char GuildIds[]	= "guildIds";
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
			kvp(Database::GuildIds, 1)
		),
		mongocxx::options::index{}
			.unique(true)
			.sparse(true)
	);
}

bool MongoDBAgentIdentityStore::loadAgentGuildIds(std::string_view uuid, std::vector<std::string>& guildIds)
{
	auto client = m_pool.acquire();
	auto collection = client->database(MongoDB::DATABASE_NAME)[Database::Collection];

	static const auto kProjection = make_document(
		kvp(Database::GuildIds, 1),
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
		auto elem = view[Database::GuildIds];

		if (! elem || elem.type() != bsoncxx::type::k_array)
			return false;

		guildIds.clear();
		for (const auto guildId : elem.get_array().value)
		{
			if (guildId.type() != bsoncxx::type::k_string)
				return false;
			guildIds.emplace_back(guildId.get_string().value);
		}
	}
	catch (const std::exception& e)
	{
		return false;
	}

	return ! guildIds.empty();
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
		kvp(Database::GuildIds, guildId)
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
	std::string previousUUID;
	if (loadAgentFromGuildId(guildId, previousUUID) && previousUUID == uuid)
		return true;

	auto client = m_pool.acquire();
	auto collection = client->database(MongoDB::DATABASE_NAME)[Database::Collection];

	auto filter = make_document(
		kvp(Database::UUID, uuid)
	);

	auto addGuildId = make_document(
		kvp("$addToSet", make_document(
			kvp(Database::GuildIds, guildId)
		))
	);

	try
	{
		if (! collection.find_one(filter.view()))
			return false;

		if (! previousUUID.empty())
		{
			auto previousFilter = make_document(
				kvp(Database::UUID, previousUUID)
			);
			auto removeGuildId = make_document(
				kvp("$pull", make_document(
					kvp(Database::GuildIds, guildId)
				))
			);

			auto removed = collection.update_one(previousFilter.view(), removeGuildId.view());
			if (! removed || removed->matched_count() == 0)
				return false;
		}

		auto result = collection.update_one(filter.view(), addGuildId.view());
		if (result && result->matched_count() > 0)
			return true;
	}
	catch (const std::exception& error)
	{
		Logger::Agents().error(
			"Failed to assign guild {} to agent {}: {}",
			guildId,
			uuid,
			error.what()
		);
	}

	if (! previousUUID.empty())
	{
		try
		{
			auto previousFilter = make_document(
				kvp(Database::UUID, previousUUID)
			);
			collection.update_one(previousFilter.view(), addGuildId.view());
		}
		catch (const std::exception& error)
		{
			Logger::Agents().error(
				"Failed to restore guild {} assignment to agent {}: {}",
				guildId,
				previousUUID,
				error.what()
			);
		}
	}

	return false;
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
