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
		std::string code(6, '\0');

		if (RAND_bytes(reinterpret_cast<unsigned char*>(code.data()), code.size()) != 1)
			throw std::runtime_error("Failed to generate pairing code");

		for (char& c : code)
			c = kPairingCodeCharacters[static_cast<unsigned char>(c) % kPairingCodeCharacters.size()];

		return code;
	}
}

PairingCodeManager::PairingCodeManager(boost::asio::io_context& ioContext)
	: m_p(std::make_unique<PairingCodeManagerPrivate>(ioContext))
{
}

PairingCodeManager::~PairingCodeManager() = default;

std::shared_ptr<PairingCodeRequest> PairingCodeManager::requestPairingCode(std::string agentUUID)
{
	std::shared_ptr<PairingCodeRequest> request(new PairingCodeRequest(std::move(agentUUID), m_p->m_executor), [this](PairingCodeRequest* req)
	{
		{
			std::unique_lock lock(m_p->m_mutex);
			auto it = m_p->m_pendingRequests.find(req->code);
			if (it != m_p->m_pendingRequests.end())
			{
				auto existing = it->second.lock();
				if (! existing || existing.get())
					m_p->m_pendingRequests.erase(it);
			}
		}

		delete req;
	});

	std::unique_lock lock(m_p->m_mutex);
	constexpr auto kMaxAttempts = 100;
	for (int attempt = 0; attempt < kMaxAttempts; ++attempt)
	{
		std::string pairingCode = GeneratePairingCode();

		auto [it, inserted] = m_p->m_pendingRequests.try_emplace(pairingCode, request);
		if (inserted)
		{
			request->code = std::move(pairingCode);
			return request;
		}
	}

	return nullptr;
}

std::shared_ptr<PairingCodeRequest> PairingCodeManager::confirmPairing(const std::string& pairingCode, std::string info)
{
	std::shared_ptr<PairingCodeRequest> req;

	{
		std::shared_lock lock(m_p->m_mutex);

		auto request = m_p->m_pendingRequests.find(pairingCode);
		if (request == m_p->m_pendingRequests.end())
			return nullptr;

		req = request->second.lock();
	}

	if (req && req->acknowledge(std::move(info)))
		return req;

	return nullptr;
}

PairingCodeManagerPrivate::PairingCodeManagerPrivate(boost::asio::io_context& context)
	: m_executor(context.get_executor())
{

}
