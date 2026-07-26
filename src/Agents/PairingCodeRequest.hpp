#pragma once

#include <boost/asio.hpp>

#include <chrono>
#include <functional>
#include <string>

namespace asio = boost::asio;

enum class PairingCodeResult
{
	Success,
	Timeout,
};

struct PairingCodeRequest
{
	PairingCodeRequest(boost::asio::any_io_executor executor)
		: timer(std::move(executor))
	{
	}


	asio::awaitable<PairingCodeResult>	waitUntil(std::chrono::steady_clock::duration timeout)
	{
		timer.expires_after(timeout);
		boost::system::error_code ec;
		co_await timer.async_wait(asio::redirect_error(asio::use_awaitable, ec));

		co_return result;
	}

	void approve(std::string inf)
	{
		info = std::move(inf);
		result = PairingCodeResult::Success;
		timer.cancel();
	}

	std::string			code;
	asio::steady_timer	timer;
	PairingCodeResult	result = PairingCodeResult::Timeout;
	std::string			info;
};
