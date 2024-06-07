#pragma once

#include <memory>
#include <mongocxx/logger.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <vector>

namespace Logger
{

	static spdlog::logger& getInstance()
	{
		static std::shared_ptr<spdlog::logger> logger;
		static std::once_flag initFlag;
		std::call_once(initFlag, []() {
			spdlog::init_thread_pool(8192, 2);
			std::vector<spdlog::sink_ptr> sinks;
			auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt >();
			auto rotating = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("DiscordBot.log", 1024 * 1024 * 5, 10);
			sinks.push_back(stdout_sink);
			sinks.push_back(rotating);
			logger = std::make_unique<spdlog::async_logger>("logs", sinks.begin(), sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);
			spdlog::register_logger(logger);
			logger->set_pattern("%^%Y-%m-%d %H:%M:%S.%e [%L] [th#%t]%$ : %v");
			logger->set_level(spdlog::level::level_enum::debug);
			});

		return *logger;
	}

	class MongoDBLogger
		: public mongocxx::logger
	{
	public:
		MongoDBLogger() = default;
		void operator()(mongocxx::log_level level, mongocxx::stdx::string_view domain, mongocxx::stdx::string_view message) noexcept;
	};

}