#include "AgentConnection.hpp"
#include "AgentConnection_p.hpp"

#include "PairingCodeRequest.hpp"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/string_generator.hpp>

#include <format>

namespace
{
	constexpr std::uint32_t kMaxMessageSize = 1024 * 1024;

	bool GenerateChallenge(Buffer& challenge)
	{
		challenge.resize(32);

		return RAND_bytes(
			reinterpret_cast<unsigned char*>(challenge.data()),
			challenge.size()
		) == 1;
	}

	bool VerifyChallengeSignature(const Buffer& publicKey, const Buffer& challenge, std::span<const std::byte>& signature)
	{
		if (publicKey.size() != 32)
			return false;

		if (signature.size() != 64)
			return false;

		EVP_PKEY* rawKey = EVP_PKEY_new_raw_public_key(
			EVP_PKEY_ED25519,
			nullptr,
			reinterpret_cast<const unsigned char*>(publicKey.data()),
			publicKey.size()
		);
		if (! rawKey)
			return false;

		std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(rawKey, EVP_PKEY_free);
		EVP_MD_CTX* rawContext = EVP_MD_CTX_new();
		if (! rawContext)
			return false;

		std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> context(rawContext, EVP_MD_CTX_free);
		if (EVP_DigestVerifyInit(
			context.get(),
			nullptr,
			nullptr,
			nullptr,
			key.get()
		) <= 0)
			return false;

		const int result = EVP_DigestVerify(
			context.get(),
			reinterpret_cast<const unsigned char*>(signature.data()),
			signature.size(),
			reinterpret_cast<const unsigned char*>(challenge.data()),
			challenge.size()
		);

		return result == 1;
	}
}

template <>
struct std::formatter<AgentState> : std::formatter<std::string_view> {
	auto format(AgentState state, std::format_context& ctx) const {
		std::string_view name = "Unknown";
		switch (state)
		{
		case AgentState::Connecting:		name = "Connecting";		break;
		case AgentState::Pairing:			name = "Pairing";			break;
		case AgentState::Authenticating:	name = "Authenticating";	break;
		case AgentState::Connected:			name = "Connected";			break;
		case AgentState::Disconnected:		name = "Disconnected";		break;
		}
		return std::formatter<std::string_view>::format(name, ctx);
	}
};

AgentConnection::AgentConnection(tcp::socket&& socket, asio::ssl::context& context, IAgentIdentityStore& store, PairingCodeManager& pairingCodeManager)
	: m_p(std::make_unique<AgentConnectionPrivate>(std::move(socket), context, store, pairingCodeManager))
{

}

asio::awaitable<void> AgentConnection::run()
{
	co_await m_p->run();
}

AgentConnection::~AgentConnection() = default;

AgentConnectionPrivate::AgentConnectionPrivate(tcp::socket&& socket, asio::ssl::context& context, IAgentIdentityStore& store, PairingCodeManager& pairingCodeManager)
	: m_store(store)
	, m_pairingCodeManager(pairingCodeManager)
	, m_socket(std::move(socket), context)
	, m_logger(Logger::Agents())
{

}

asio::awaitable<void> AgentConnectionPrivate::run()
{
	co_await m_socket.async_handshake(asio::ssl::stream_base::server, asio::use_awaitable);
	
	for(;;)
	{
		switch (m_state)
		{
			case AgentState::Connecting:
				co_await handleConnecting();
				break;
			case AgentState::Pairing:
				co_await handlePairing();
				break;
			case AgentState::Authenticating:
				co_await handleAuthenticating();
				break;
			case AgentState::Connected:
				co_await handleConnected();
				break;
			case AgentState::Disconnected:
				co_await handleDisconnected();
				co_return;
		}
	}

	co_return;
}

asio::awaitable<void> AgentConnectionPrivate::handleConnecting()
{
	assert(m_state == AgentState::Connecting);
	enum class State {
		Valid,
		Invalid,
		AuthInitUnknownUUID,
		PairRequestInvalid,
	};
	State state = State::Valid;

	// Wait for an authentication or pairing request
	co_await readAgentMessage([this, &state](AgentMessage& agentMessage) {
		if (agentMessage.isAuthenticationInitiation())
		{
			auto authInit = agentMessage.getAuthenticationInitiation();
			std::string_view uuidView(authInit.getUuid().cStr(), authInit.getUuid().size());

			if (! m_store.loadPublicKey(uuidView, m_publicKey))
			{
				m_logger.info("Unknown UUID for Authentication Initiation: {}", uuidView);
				state = State::AuthInitUnknownUUID;
				return;
			}

			m_uuid = uuidView;
			m_logger.info("Loaded public key for UUID {}", m_uuid);
			setState(AgentState::Authenticating);
		}
		else if (agentMessage.isPair())
		{
			auto pairRequest = agentMessage.getPair();

			// Check that uuid doesn't exists, is valid, and public key is valid
			std::string_view uuidView(pairRequest.getUuid().cStr(), pairRequest.getUuid().size());
			bool valid = verifyNewUUID(uuidView);
			if (valid)
			{
				auto publicKey = pairRequest.getPublicKey();
				std::span<const std::byte> publicKeyView(
					reinterpret_cast<const std::byte*>(publicKey.begin()),
					publicKey.size()
				);
				valid &= verifyPublicKey(publicKeyView);
				if (! valid)
					m_logger.info("Invalid public key for UUID: {}", uuidView);
			}
			else
			{
				m_logger.info("Rejected new UUID: {}", uuidView);
			}

			if (! valid)
			{
				state = State::PairRequestInvalid;
				return;
			}

			m_uuid = uuidView;
			m_logger.info("New Agent registration: {}", m_uuid);
			m_publicKey = Buffer(
				reinterpret_cast<const std::byte*>(pairRequest.getPublicKey().begin()),
				reinterpret_cast<const std::byte*>(pairRequest.getPublicKey().end())
			);
			setState(AgentState::Pairing);
		}
		else
		{
			m_logger.error("Unexpected agent message. Expected an Authentication Initiation or Pair Request");
			state = State::Invalid;
			resetState();
			return;
		}
	});

	switch (state)
	{
		case State::AuthInitUnknownUUID:
		case State::PairRequestInvalid:
			break;

		default:
			co_return;
	}

	co_await sendServerMessage([this, state](ServerMessage& serverMessage) {
		switch (state)
		{
			case State::AuthInitUnknownUUID:
			{
				auto authInitResult = serverMessage.initAuthenticationInitiation();
				authInitResult.setInvalidUuid();
				break;
			}
			case State::PairRequestInvalid:
			{
				auto pairCodeResult = serverMessage.initPairCode();
				pairCodeResult.setInvalid();
				break;
			}
		}
	});

	co_return;
}

asio::awaitable<void> AgentConnectionPrivate::handlePairing()
{
	assert(m_state == AgentState::Pairing);
	bool success = true;

	{
		auto pairingCodeRequestPtr = m_pairingCodeManager.requestPairingCode(m_uuid);

		co_await sendServerMessage([this, &pairingCodeRequestPtr, &success](ServerMessage& serverMessage) {
			auto pairCodeResult = serverMessage.initPairCode();
			if (pairingCodeRequestPtr)
			{
				m_logger.info("Sending pairing code {} to UUID {}", pairingCodeRequestPtr->code, m_uuid);
				auto valid = pairCodeResult.initValid();
				valid.setCode(kj::StringPtr(pairingCodeRequestPtr->code.data(), pairingCodeRequestPtr->code.size()));
			}
			else
			{
				m_logger.info("Max attempts exceeded while trying to generate pairing code for UUID {}. Notifying agent to try again later", m_uuid);
				pairCodeResult.setRetry();
				success = false;
			}
		});

		if (! success)
		{
			resetState();
			co_return;
		}

		// Wait for pairing code to be entered
		const auto pairingCodeResult = co_await pairingCodeRequestPtr->waitUntil(std::chrono::seconds(20));
		success = pairingCodeResult == PairingCodeResult::Acknowledged;

		co_await sendServerMessage([&pairingCodeRequestPtr, pairingCodeResult](ServerMessage& serverMessage) {
			auto pairingResult = serverMessage.initPairingResult();
			switch (pairingCodeResult)
			{
			case PairingCodeResult::Acknowledged:
			{
				auto pairingSuccess = pairingResult.initSuccess();
				pairingSuccess.setPairingInfo(pairingCodeRequestPtr->info);
				break;
			}
			case PairingCodeResult::Timeout:
			{
				pairingResult.setTimedOut();
				break;
			}
			}
		});

		if (! success)
		{
			resetState();
			co_return;
		}

		co_await readAgentMessage([this, &success](AgentMessage& message) {
			if (message.isPairingConfirmation())
			{
				auto pairingConfirmation = message.getPairingConfirmation();
				if (! pairingConfirmation.isApproved())
				{
					m_logger.info("Pairing for UUID {} rejected by Agent", m_uuid);
					success = false;
				}
			}
			else
			{
				m_logger.error("Unexpected agent message. Expected a PairingConfirmation");
				success = false;
			}
		});

		pairingCodeRequestPtr->confirm(success);
		if (! success)
		{
			resetState();
			co_return;
		}
	}

	std::span<const std::byte> publicKeySpan(m_publicKey);
	if (m_store.savePublicKey(m_uuid, publicKeySpan))
		m_logger.info("Agent {} successfully registered", m_uuid);
	else
		m_logger.error("Failed to save public key for UUID {}", m_uuid);

	resetState();

	co_return;
}

asio::awaitable<void> AgentConnectionPrivate::handleAuthenticating()
{
	assert(m_state == AgentState::Authenticating);
	assert(! m_publicKey.empty());
	bool success = true;

	Buffer challenge;
	const bool challengeGenerated = GenerateChallenge(challenge);

	co_await sendServerMessage([&challenge, challengeGenerated](ServerMessage& serverMessage) {
		auto authInitResult = serverMessage.initAuthenticationInitiation();
		if (! challengeGenerated)
		{
			authInitResult.setRetry();
			return;
		}
		auto authChallenge = authInitResult.initChallenge();
		authChallenge.setChallenge(
			kj::ArrayPtr<const kj::byte>(
				reinterpret_cast<const kj::byte*>(challenge.data()),
				challenge.size()
			)
		);
	});

	if (! challengeGenerated)
	{
		m_logger.error("Failed to generate challenge for UUID {}", m_uuid);
		co_return;
	}

	m_logger.info("Generated challenge for UUID {}", m_uuid);

	co_await readAgentMessage([this, &success, &challenge](AgentMessage& agentMessage) {
		if (! agentMessage.isAuthenticationRequest())
		{
			m_logger.error("Unexpected agent message. Expected an Authentication Request");
			success = false;
			resetState();
			return;
		}

		auto authRequest = agentMessage.getAuthenticationRequest();
		std::span<const std::byte> signatureView(
			reinterpret_cast<const std::byte*>(authRequest.getSignature().begin()),
			authRequest.getSignature().size()
		);

		if (! VerifyChallengeSignature(m_publicKey, challenge, signatureView))
		{
			m_logger.info("Agent {} failed challenge", m_uuid);
			success = false;
			resetState();
			return;
		}
	});

	co_await sendServerMessage([&success](ServerMessage& serverMessage) {
		auto authResult = serverMessage.initAuthenticationResult();
		if (success)
			authResult.setSuccess();
		else
			authResult.setChallengeFailed();
	});
	if (! success)
		co_return;

	m_logger.info("Agent {} authenticated", m_uuid);
	setState(AgentState::Connected);
	m_publicKey.resize(0);

	co_return;
}

asio::awaitable<void> AgentConnectionPrivate::handleConnected()
{
	assert(m_state == AgentState::Connected);
	co_await readAgentMessage([](AgentMessage& agentMessage) {

	});
	co_return;
}

asio::awaitable<void> AgentConnectionPrivate::handleDisconnected()
{
	assert(m_state == AgentState::Disconnected);
	co_return;
}

bool AgentConnectionPrivate::verifyNewUUID(std::string_view uuid)
{
	boost::uuids::string_generator generator;
	boost::uuids::uuid parsed;

	try { parsed = generator(std::string(uuid)); }
	catch (const std::exception& e) { return false; }

	// Check if UUID is already registered

	return true;
}

bool AgentConnectionPrivate::verifyPublicKey(std::span<const std::byte>& publicKey)
{
	// ED25519 validation
	if (publicKey.size() != 32)
		return false;

	EVP_PKEY* rawKey = EVP_PKEY_new_raw_public_key(
		EVP_PKEY_ED25519,
		nullptr,
		reinterpret_cast<const unsigned char*>(publicKey.data()),
		publicKey.size()
	);

	if (! rawKey)
		return false;

	EVP_PKEY_free(rawKey);

	return true;
}

asio::awaitable<Buffer> AgentConnectionPrivate::read()
{
	uint32_t size;

	co_await asio::async_read(
		m_socket,
		asio::buffer(&size, sizeof(size)),
		asio::use_awaitable
	);

	size = ntohl(size);
	if (size > kMaxMessageSize)
		throw std::runtime_error("Message too large");

	Buffer buffer(size);

	co_await asio::async_read(
		m_socket,
		asio::buffer(buffer),
		asio::use_awaitable
	);

	co_return buffer;
}

asio::awaitable<void> AgentConnectionPrivate::writeMessage(capnp::MallocMessageBuilder& message)
{
	auto serialized = capnp::messageToFlatArray(message);
	auto bytes = serialized.asBytes();

	std::uint32_t size = htonl(static_cast<std::uint32_t>(bytes.size()));

	co_await asio::async_write(
		m_socket,
		asio::buffer(&size, sizeof(size)),
		asio::use_awaitable
	);

	co_await asio::async_write(
		m_socket,
		asio::buffer(bytes.begin(), bytes.size()),
		asio::use_awaitable
	);
}

void AgentConnectionPrivate::setState(AgentState state)
{
	m_state = state;
	if (m_state != state)
		m_logger.debug(std::format("Updated state: {} -> {}", state, m_state));
}

void AgentConnectionPrivate::resetState()
{
	setState(AgentState::Connecting);
	m_uuid.clear();
	m_publicKey.resize(0);
}
