#pragma once

#include <boost/asio.hpp>

#include <chrono>
#include <functional>
#include <mutex>
#include <string>

namespace asio = boost::asio;

enum class PairingCodeResult
{
	Pending,
	Acknowledged,
	Timeout,
	Confirming,
	Approved,
	Rejected,
};

class PairingCodeManager;

struct PairingCodeRequest
{
	PairingCodeRequest(std::string agentUUID, boost::asio::any_io_executor e)
		: agentUUID(std::move(agentUUID))
		, timer(std::move(e))
	{
	}

	asio::awaitable<PairingCodeResult>	waitUntil(std::chrono::steady_clock::duration timeout)
	{
		timer.expires_after(timeout);
		boost::system::error_code ec;

		{
			std::lock_guard lock(mutex);
			if (result == PairingCodeResult::Acknowledged)
				result = PairingCodeResult::Confirming;
		}

		co_await timer.async_wait(asio::redirect_error(asio::use_awaitable, ec));

		std::lock_guard lock(mutex);

		if (result == PairingCodeResult::Pending || result == PairingCodeResult::Confirming)
			result = PairingCodeResult::Timeout;

		co_return result;
	}

	bool acknowledge(std::string inf)
	{
		{
			std::lock_guard lock(mutex);
			if (result != PairingCodeResult::Pending)
				return false;

			info = std::move(inf);
			result = PairingCodeResult::Acknowledged;
		}

		timer.cancel();

		return true;
	}

	bool confirm(bool approved)
	{
		{
			std::lock_guard lock(mutex);
			if (result != PairingCodeResult::Confirming)
				return false;

			result = approved ? PairingCodeResult::Approved : PairingCodeResult::Rejected;
		}

		timer.cancel();

		return true;
	}

	std::string						agentUUID;
	std::string						code;
	asio::steady_timer				timer;
	boost::asio::any_io_executor	executor;
	PairingCodeResult				result = PairingCodeResult::Pending;
	std::string						info;

private:
	std::mutex			mutex;
};
