#pragma once

#include "Log.hpp"

#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/ssl.hpp>

#include <capnp/message.h>
#include <capnp/serialize.h>
#include <ScorchProtocol.capnp.h>

#include <span>
#include <string>
#include <vector>

namespace asio = boost::asio;

using tcp = asio::ip::tcp;
using TLSSocket = asio::ssl::stream<tcp::socket>;
using Buffer = std::vector<std::byte>;
using AgentMessage = scorch::protocol::AgentMessage::Reader;
using ServerMessage = scorch::protocol::ServerMessage::Builder;

enum class AgentState
{
	Connecting,
	Pairing,
	Authenticating,
	Connected,
	Disconnected,
};

class AgentConnectionPrivate
{
public:
							AgentConnectionPrivate(tcp::socket&& socket, asio::ssl::context& context);

	asio::awaitable<void>	run();

	asio::awaitable<void>	handleConnecting();
	asio::awaitable<void>	handlePairing();
	asio::awaitable<void>	handleAuthenticating();
	asio::awaitable<void>	handleConnected();
	asio::awaitable<void>	handleDisconnected();

	// Pairing
	bool					verifyNewUUID(std::string_view uuid);
	bool					verifyPublicKey(std::span<const std::byte>& publicKey);
	std::string				generatePairingCode();
	bool					savePublicKey(std::string_view uuid, Buffer& publicKey);

	// Authenticating
	bool					getPublicKey(std::string_view uuid, Buffer& publicKey);

	template <typename Callback>
		requires std::invocable<Callback, ServerMessage&>
	asio::awaitable<void>			sendServerMessage(Callback&& callback)
	{
		capnp::MallocMessageBuilder message;
		ServerMessage serverMessage = message.initRoot<scorch::protocol::ServerMessage>();

		callback(serverMessage);

		co_await writeMessage(message);
	}

	template <typename Callback>
		requires std::invocable<Callback, AgentMessage&>
	asio::awaitable<void>			readAgentMessage(Callback&& callback)
	{
		auto response = co_await read();

		if (response.size() % sizeof(capnp::word) != 0)
			throw std::runtime_error("Invalid Cap'n Proto message size");

		auto reader = capnp::FlatArrayMessageReader(
			kj::ArrayPtr<const capnp::word>(
				reinterpret_cast<const capnp::word*>(response.data()),
				response.size() / sizeof(capnp::word)
			)
		);

		AgentMessage agentMessage = reader.getRoot<scorch::protocol::AgentMessage>();

		callback(agentMessage);

		co_return;
	}

	asio::awaitable<Buffer>			read();
	asio::awaitable<void>			writeMessage(capnp::MallocMessageBuilder& message);

	void							setState(AgentState state);
	void							resetState();

	spdlog::logger& m_logger;
	TLSSocket		m_socket;
	AgentState		m_state = AgentState::Connecting;

	std::string		m_uuid;
	Buffer			m_publicKey;
};
