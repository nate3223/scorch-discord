#include "Log.hpp"

#include <format>

namespace Logger
{

	void MongoDBLogger::operator()(mongocxx::log_level level, mongocxx::stdx::string_view domain, mongocxx::stdx::string_view message) noexcept
	{
		// printf("%s", std::format("{} | {} | {}\n", mongocxx::to_string(level).data(), domain.data(), message.data()).c_str());
	}

}