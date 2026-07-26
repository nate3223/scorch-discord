#pragma once

#include "PairingCodeRequest.hpp"

#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/asio/any_io_executor.hpp>

#include <memory>

namespace boost
{
	namespace asio
	{
		class io_context;
	}
}


class PairingCodeManagerPrivate
{
public:
	PairingCodeManagerPrivate(boost::asio::io_context& context);

	boost::unordered_flat_map<std::string, PairingCodeRequest*>	m_pendingRequests;
	boost::asio::any_io_executor	m_executor;
};