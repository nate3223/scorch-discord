#include "MongoDBManager.hpp"

#include "Log.hpp"

#include <cstdlib>
#include <sstream>

mongocxx::instance MongoDBManager::g_instance{std::make_unique<MongoDBLogger>()};

MongoDBManager::MongoDBManager()
{
	const char* address = std::getenv("MONGO_DB_ADDRESS");
	if (address == nullptr)
	{
		Logger::App().critical("MONGO_DB_ADDRESS environment variable is not set");
		std::exit(EXIT_FAILURE);
	}
	const char* username = std::getenv("MONGO_DB_USERNAME");
	const char* password = std::getenv("MONGO_DB_PASSWORD");
	std::string login = "";
	if (username != nullptr && password == nullptr)
	{
		Logger::App().critical("MONGO_DB_PASSWORD is not set, but MONGO_DB_USERNAME is set");
		std::exit(EXIT_FAILURE);
	}
	else if (username == nullptr && password != nullptr)
	{
		Logger::App().critical("MONGO_DB_USERNAME is not set, but MONGO_DB_PASSWORD is set");
		std::exit(EXIT_FAILURE);
	}
	else if (username != nullptr && password != nullptr)
	{
		login = std::format("{}:{}@", username, password);
	}

	mongocxx::options::client clientOptions;

	const bool tlsEnabled = std::getenv("MONGO_DB_TLS_ENABLED") != nullptr;
	if (tlsEnabled)
	{
		mongocxx::options::tls tlsOptions;

		if (const char* caFile = std::getenv("MONGO_DB_CA_FILE"); caFile != nullptr)
			tlsOptions.ca_file(caFile);

		if (const char* pemFile = std::getenv("MONGO_DB_PEM_FILE"); pemFile != nullptr)
			tlsOptions.pem_file(pemFile);

		if (const char* pemPassword = std::getenv("MONGO_DB_PEM_PASSWORD"); pemPassword != nullptr)
			tlsOptions.pem_password(pemPassword);

		if (std::getenv("MONGO_DB_ALLOW_INVALID_CERTIFICATES") != nullptr)
			tlsOptions.allow_invalid_certificates(true);
		else
			tlsOptions.allow_invalid_certificates(false);

		clientOptions.tls_opts(tlsOptions);
	}

	int numParameters = 0;
	std::stringstream parametersStream;
	std::initializer_list<std::pair<bool, const char*>> parameters = {
		{ tlsEnabled, "tls=true" }
	};
	for (const auto parameter : parameters)
	{
		if (parameter.first)
		{
			if (numParameters++ == 0)
				parametersStream << "/?";
			else
				parametersStream << "&";
			parametersStream << parameter.second;
		}
	}

	std::string uri = std::format("mongodb://{}{}{}", login, address, parametersStream.str());
	try
	{
		m_pool = std::make_unique<mongocxx::pool>(mongocxx::uri(uri), clientOptions);
		if (!m_pool->try_acquire())
		{
			Logger::App().critical("Could not connect to MongoDB");
			std::exit(EXIT_FAILURE);
		}
		Logger::App().info("Connected to MongoDB (TLS {})", tlsEnabled ? "enabled" : "disabled");
	}
	catch (const std::exception& e)
	{
		Logger::App().critical("Could not connect to MongoDB: {}", e.what());
		std::exit(EXIT_FAILURE);
	}
}


mongocxx::pool& MongoDBManager::getPool()
{
	return *m_pool;
}
