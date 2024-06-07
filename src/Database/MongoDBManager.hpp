#pragma once

#include "DatabaseManager.hpp"

#include <memory>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/pool.hpp>
#include <mongocxx/uri.hpp>

class MongoDBClient
{

};

class MongoDBManager
	: public DatabaseManager::Database
{
	friend class DatabaseManager;

public:
	MongoDBManager(MongoDBManager&) = delete;
	void operator=(const MongoDBManager&) = delete;

	mongocxx::pool&	getPool();

protected:
	MongoDBManager();

	static mongocxx::instance				g_instance;

	std::unique_ptr<mongocxx::pool>			m_pool;
};