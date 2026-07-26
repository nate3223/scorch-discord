#include "PairingCodeManager.hpp"
#include "PairingCodeManager_p.hpp"

#include "PairingCodeRequest.hpp"

#include <boost/asio.hpp>

#include <openssl/rand.h>

namespace
{
	constexpr std::string_view kPairingCodeCharacters = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";

	std::string GeneratePairingCode()
	{
		std::array<unsigned char, 6> randomBytes;

		if (RAND_bytes(randomBytes.data(), randomBytes.size()) != 1)
			throw std::runtime_error("Failed to generate pairing code");

		std::string code;
		code.reserve(randomBytes.size());

		for (auto byte : randomBytes)
			code += kPairingCodeCharacters[byte % kPairingCodeCharacters.size()];

		return code;
	}
}

PairingCodeManager::PairingCodeManager(boost::asio::io_context& ioContext)
	: m_p(std::make_unique<PairingCodeManagerPrivate>(ioContext))
{
}

PairingCodeManager::~PairingCodeManager() = default;

PairingCodeManager::PairingCodeRequestPtr PairingCodeManager::requestPairingCode()
{
	PairingCodeRequestPtr request(new PairingCodeRequest(m_p->m_executor), [this](PairingCodeRequest* req) {
		m_p->m_pendingRequests.erase(req->code);
		delete req;
	});

	for (;;)
	{
		std::string pairingCode = GeneratePairingCode();
		auto [it, inserted] = m_p->m_pendingRequests.try_emplace(pairingCode, request.get());
		if (inserted)
		{
			request->code = std::move(pairingCode);
			return request;
		}
	}
}

void PairingCodeManager::confirmPairing(std::string pairingCode, std::string info)
{
	auto request = m_p->m_pendingRequests.find(pairingCode);
	if (request == m_p->m_pendingRequests.end())
		return;

	request->second->approve(std::move(info));
}

PairingCodeManagerPrivate::PairingCodeManagerPrivate(boost::asio::io_context& context)
	: m_executor(context.get_executor())
{

}
