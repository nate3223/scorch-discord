#include "MongoDBManager.hpp"

#include "Log.hpp"
#include <sstream>

mongocxx::instance MongoDBManager::g_instance{std::make_unique<Logger::MongoDBLogger>()};

MongoDBManager::MongoDBManager()
{
	const char* address = std::getenv("MONGO_DB_ADDRESS");
	if (address == nullptr)
	{
		printf("Could not find MONGO_DB_ADDRESS environment variable\n");
		exit(1);
	}
	const char* username = std::getenv("MONGO_DB_USERNAME");
	const char* password = std::getenv("MONGO_DB_PASSWORD");
	std::string login = "";
	if (username != nullptr && password == nullptr)
	{
		printf("Could not find MONGO_DB_PASSWORD environment variable, even though MONGO_DB_USERNAME is found\n");
		exit(1);
	}
	else if (username == nullptr && password != nullptr)
	{
		printf("Could not find MONGO_DB_USERNAME environment variable, even though MONGO_DB_PASSWORD is found\n");
		exit(1);
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
			printf("Could not connect to MongoDB database\n");
			exit(1);
		}
	}
	catch (const std::exception& e)
	{
		printf("Could not connect to MongoDB database\n");
		printf(e.what());
		exit(1);
	}
}


mongocxx::pool& MongoDBManager::getPool()
{
	return *m_pool;
}