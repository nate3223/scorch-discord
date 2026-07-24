#pragma once

#include <memory>
#include <mongocxx/logger.hpp>
#include <spdlog/spdlog.h>

class Logger
{
public:
	static spdlog::logger&	App();
	static spdlog::logger&	Agents();
	static spdlog::logger&	DPP();

private:
	static inline Logger&	Instance();

	Logger();
	~Logger();

	std::shared_ptr<spdlog::logger>	m_logger;
	std::shared_ptr<spdlog::logger>	m_agents;;
	std::shared_ptr<spdlog::logger>	m_dppLogger;
};

class MongoDBLogger
	: public mongocxx::logger
{
public:
	MongoDBLogger() = default;
	void operator()(mongocxx::log_level level, bsoncxx::v1::stdx::string_view domain, bsoncxx::v1::stdx::string_view message) noexcept;
};
