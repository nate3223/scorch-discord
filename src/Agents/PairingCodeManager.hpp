#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>

struct PairingCodeRequest;

class PairingCodeManagerPrivate;

namespace boost
{
	namespace asio
	{
		class io_context;
	}
}

class PairingCodeManager
{
public:
	explicit							PairingCodeManager(boost::asio::io_context& ioContext);
										~PairingCodeManager();

	std::shared_ptr<PairingCodeRequest>	requestPairingCode(std::string agentUUID);

	// Thread-safe
	std::shared_ptr<PairingCodeRequest>	confirmPairing(const std::string& pairingCode, std::string info);

private:
	std::unique_ptr<PairingCodeManagerPrivate>	m_p;
};
