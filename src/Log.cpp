#include "Log.hpp"

#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <vector>

#include <string_view>

namespace
{
#ifndef NDEBUG
	constexpr auto kLogLevel = spdlog::level::debug;
#else
	constexpr auto kLogLevel = spdlog::level::info;
#endif
}

Logger& Logger::Instance()
{
	static Logger sLogger;
	return sLogger;
}

spdlog::logger& Logger::App()
{
	return *Instance().m_logger;
}

spdlog::logger& Logger::Agents()
{
	return *Instance().m_agents;
}

spdlog::logger& Logger::DPP()
{
	return *Instance().m_dppLogger;
}

Logger::Logger()
{
	if (! spdlog::thread_pool())
		spdlog::init_thread_pool(8192, 2);

	std::vector<spdlog::sink_ptr> sinks;

	auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	auto rotating = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("ScorchDiscord.log", 1024 * 1024 * 5, 10);
	sinks.push_back(stdout_sink);
	sinks.push_back(rotating);

	constexpr auto kPattern = "%^%Y-%m-%d %H:%M:%S.%e [%L] [th#%t] [%n] %$%v";

	auto MakeLogger = [&](const char* const name) {
		auto logger = std::make_shared<spdlog::async_logger>(name, sinks.begin(), sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);
		logger->set_pattern(kPattern);
		logger->set_level(kLogLevel);
		logger->flush_on(spdlog::level::err);
		spdlog::register_logger(logger);
		return logger;
	};

	m_logger	= MakeLogger("App");
	m_agents	= MakeLogger("Agents");
	m_dppLogger	= MakeLogger("DPP");

	spdlog::flush_every(std::chrono::seconds(3));
}

Logger::~Logger()
{
	spdlog::shutdown();
}

void MongoDBLogger::operator()(mongocxx::log_level level, bsoncxx::v1::stdx::string_view domain, bsoncxx::v1::stdx::string_view message) noexcept
{
	spdlog::level::level_enum logLevel;
	switch (level)
	{
		case mongocxx::log_level::k_error:		logLevel = spdlog::level::err;		break;
		case mongocxx::log_level::k_critical:	logLevel = spdlog::level::critical;	break;
		case mongocxx::log_level::k_warning:	logLevel = spdlog::level::warn;		break;
		case mongocxx::log_level::k_message:
		case mongocxx::log_level::k_info:		logLevel = spdlog::level::info;		break;
		case mongocxx::log_level::k_debug:		logLevel = spdlog::level::debug;	break;
		case mongocxx::log_level::k_trace:		logLevel = spdlog::level::trace;	break;
		default:								logLevel = spdlog::level::info;		break;
	}

	try
	{
		Logger::App().log(
			logLevel,
			"[MongoDB:{}] {}",
			std::string_view(domain.data(), domain.size()),
			std::string_view(message.data(), message.size())
		);
	}
	catch (...)
	{
	}
}

