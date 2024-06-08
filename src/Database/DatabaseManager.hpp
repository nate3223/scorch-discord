#pragma once

#include <algorithm>
#include <memory>
#include <vector>

class DatabaseManager
{
public:

	class Database
	{
		public:
			virtual ~Database() {};
		protected:
			Database() {};
	};

	template <typename T>
	static std::shared_ptr<T> GetInstance()
	{
		if (auto it = std::find_if(g_databases.begin(), g_databases.end(),
								   [](const std::shared_ptr<Database>& database)
								   { return std::dynamic_pointer_cast<T>(database) != nullptr; });
			it != g_databases.end()
		)
			return std::dynamic_pointer_cast<T>(*it);

		auto database = std::shared_ptr<T>(new T());
		g_databases.push_back(database);
		return database;
	}

	DatabaseManager() = delete;

protected:
	static std::vector<std::shared_ptr<Database>>	g_databases;
};