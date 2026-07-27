#include "Log.hpp"

#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <vector>

#include <format>

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
		logger->set_level(spdlog::level::level_enum::debug);
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
	// printf("%s", std::format("{} | {} | {}\n", mongocxx::to_string(level).data(), domain.data(), message.data()).c_str());
}

