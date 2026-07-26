#pragma once

#include <functional>
#include <memory>
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
	explicit				PairingCodeManager(boost::asio::io_context& ioContext);
							~PairingCodeManager();

	using PairingCodeRequestPtr = std::unique_ptr<PairingCodeRequest, std::function<void(PairingCodeRequest*)>>;
	PairingCodeRequestPtr	requestPairingCode();

	void					confirmPairing(std::string pairingCode, std::string info);

private:
	std::unique_ptr<PairingCodeManagerPrivate>	m_p;
};
