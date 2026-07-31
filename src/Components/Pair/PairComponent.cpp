#include "PairComponent.hpp"
#include "PairComponent_p.hpp"

#include "Agents/AgentsManager.hpp"
#include "DiscordBot/DiscordBot.hpp"
#include "Log.hpp"

#include <scorch/server/PairingCodeRequest.hpp>
#include <scorch/server/Server.hpp>

#include <dpp/unicode_emoji.h>

#include <openssl/rand.h>

#include <algorithm>
#include <chrono>
#include <format>
#include <memory>
#include <stdexcept>

namespace
{
	namespace PairRequest
	{
		constexpr auto PairCode = "pair-code";
	}

	constexpr std::string_view kShareCodeCharacters = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
	constexpr auto kShareCodeLifetime = std::chrono::minutes(5);

	std::string GenerateShareCode()
	{
		std::string code(8, '\0');

		if (RAND_bytes(reinterpret_cast<unsigned char*>(code.data()), static_cast<int>(code.size())) != 1)
			throw std::runtime_error("Failed to generate share code");

		for (char& c : code)
			c = kShareCodeCharacters[static_cast<unsigned char>(c) % kShareCodeCharacters.size()];

		return code;
	}
}

PairComponent::PairComponent(DiscordBot& bot)
	: Component(bot)
	, m_p(std::make_unique<PairComponentPrivate>())
{
	m_slashCommands.emplace_back(
		SlashCommand::TaskHandler{[this](const dpp::slashcommand_t& event) -> dpp::task<void> { co_await onPairRequest(event); }},
		dpp::slashcommand("pair", "Pairs an agent to this server, given a valid pairing code.", m_bot->me.id)
			.add_option(
				dpp::command_option(dpp::co_string, PairRequest::PairCode, "Pairing or share code", true)
			)
			.set_default_permissions(0)
	);
	m_slashCommands.emplace_back(
		std::bind_front(&PairComponent::onShareAgentRequest, this),
		dpp::slashcommand(
			"shareagent",
			"Creates a temporary code for linking this guild's agent to another guild.",
			m_bot->me.id
		).set_default_permissions(0)
	);
}

PairComponent::~PairComponent() = default;

dpp::task<void> PairComponent::onPairRequest(const dpp::slashcommand_t& event)
{
	const auto pairCode = std::get<std::string>(event.get_parameter(PairRequest::PairCode));
	const auto& user = event.command.usr;
	const dpp::snowflake& guildId = event.command.guild_id;
	
	using PairingCodeRequest = scorch::server::PairingCodeRequest;
	using PairingCodeState = scorch::server::PairingCodeState;

	std::shared_ptr<PairingCodeRequest> pairingCodeRequest = m_p->m_agentsManager.confirmPairing(
		pairCode,
		std::format("User {} ({}) in guild {}", user.username, user.id.str(), guildId.str())
	);
	if (! pairingCodeRequest)
	{
		auto sharedAgentUUID = m_p->consumeShareCode(pairCode);
		if (! sharedAgentUUID)
		{
			Logger::App().warn(
				"User {} in guild {} submitted an invalid or expired pairing code",
				user.id,
				guildId
			);
			event.reply(dpp::message("Invalid or expired pairing code.").set_flags(dpp::m_ephemeral));
			co_return;
		}

		auto agent = m_p->m_agentsManager.connectedAgentByUUID(*sharedAgentUUID);
		if (! agent)
		{
			event.reply(dpp::message("The agent is not connected.").set_flags(dpp::m_ephemeral));
			co_return;
		}

		auto waiting = event.co_reply(
			dpp::message(std::format(
				"Waiting for Agent `{}` to approve sharing...",
				*sharedAgentUUID
			)).set_flags(dpp::m_ephemeral)
		);

		bool approved = false;
		std::string shareError;
		try
		{
			approved = co_await agent.requestShare(std::format(
				"User {} ({}) requested access from Discord server {}",
				user.username,
				user.id.str(),
				guildId.str()
			));
		}
		catch (const std::exception& error)
		{
			shareError = error.what();
			Logger::App().error(
				"Agent {} share request failed for Discord server {}: {}",
				*sharedAgentUUID,
				guildId,
				shareError
			);
		}

		co_await waiting;
		if (! shareError.empty())
		{
			co_await event.co_edit_original_response(
				dpp::message("The agent could not complete the share request.")
			);
			co_return;
		}

		if (! approved)
		{
			Logger::App().info(
				"Agent {} rejected sharing with guild {} requested by user {}",
				*sharedAgentUUID,
				guildId,
				user.id
			);
			co_await event.co_edit_original_response(
				dpp::message(std::format(
					"{} Agent `{}` rejected sharing.",
					dpp::unicode_emoji::x,
					*sharedAgentUUID
				))
			);
			co_return;
		}

		if (! m_p->m_agentsManager.saveAgentGuildId(*sharedAgentUUID, guildId.str()))
		{
			Logger::App().error(
				"Failed to save approved association between agent {} and guild {}",
				*sharedAgentUUID,
				guildId
			);
			co_await event.co_edit_original_response(
				dpp::message("The approved agent association could not be saved.")
			);
			co_return;
		}

		const auto reply = std::format(
			"{} Agent `{}` linked successfully!",
			dpp::unicode_emoji::white_check_mark,
			*sharedAgentUUID
		);
		co_await event.co_edit_original_response(dpp::message(reply));
		Logger::App().info(
			"Agent {} shared with guild {} by user {}",
			*sharedAgentUUID,
			guildId,
			user.id
		);

		auto logMessage = std::make_unique<GuildEmbedMessage>(reply, guildId);
		logMessage->user = user;
		m_bot.componentLog(std::move(logMessage));
		co_return;
	}

	const auto agentUUID = pairingCodeRequest->uuid();

	dpp::message msg(std::format("Waiting for Agent {} to approve pairing...", agentUUID));
	msg.set_flags(dpp::m_ephemeral);
	auto waitingMsg = event.co_reply(msg);

	auto pairingResult = co_await pairingCodeRequest->waitUntil(std::chrono::minutes(5));

	co_await waitingMsg;

	if (pairingResult == PairingCodeState::Timeout)
	{
		Logger::App().warn("Pairing with agent {} timed out for guild {}", agentUUID, guildId);
		co_await event.co_edit_original_response(dpp::message(std::format("{} Pairing with Agent `{}` **Timed out**!", dpp::unicode_emoji::hourglass, agentUUID)));
		co_return;
	}
	else if (pairingResult != PairingCodeState::Approved)
	{
		Logger::App().info(
			"Agent {} rejected pairing with guild {} requested by user {}",
			agentUUID,
			guildId,
			user.id
		);
		co_await event.co_edit_original_response(dpp::message(std::format("{} Pairing with Agent `{}` **Rejected**!", dpp::unicode_emoji::x, agentUUID)));
		co_return;
	}

	auto reply = std::format("{} Pairing with Agent `{}` **Approved**!", dpp::unicode_emoji::white_check_mark, agentUUID);

	if (! m_p->m_agentsManager.saveAgentGuildId(agentUUID, guildId.str()))
	{
		Logger::App().error(
			"Failed to save approved pairing between agent {} and guild {}",
			agentUUID,
			guildId
		);
		co_await event.co_edit_original_response(dpp::message(std::format("{} Pairing with Agent `{}` **Rejected**!", dpp::unicode_emoji::x, agentUUID)));
		co_return;
	}

	auto response = event.co_edit_original_response(dpp::message(reply));
	Logger::App().info(
		"Agent {} paired with guild {} by user {}",
		agentUUID,
		guildId,
		user.id
	);
	auto logMessage = std::make_unique<GuildEmbedMessage>(reply, guildId);
	logMessage->user = user;
	m_bot.componentLog(std::move(logMessage));

	co_await response;
}

void PairComponent::onShareAgentRequest(const dpp::slashcommand_t& event)
{
	const auto agent = m_p->m_agentsManager.connectedAgent(event.command.guild_id.str());
	if (! agent)
	{
		event.reply(
			dpp::message("This guild does not have a connected agent.")
				.set_flags(dpp::m_ephemeral)
		);
		return;
	}
	
	std::string code;

	try
	{
		code = m_p->createShareCode(agent.uuid());
	}
	catch (const std::exception& error)
	{
		Logger::App().error(
			"Failed to create agent share code for guild {}: {}",
			event.command.guild_id,
			error.what()
		);
		event.reply(
			dpp::message("Could not create an agent share code.")
				.set_flags(dpp::m_ephemeral)
		);
		return;
	}

	event.reply(
		dpp::message(std::format(
			"Use `/pair {}` in the other guild within five minutes. This code can only be used once.",
			code
		)).set_flags(dpp::m_ephemeral)
	);

	auto logMessage = std::make_unique<GuildEmbedMessage>(
		"Created a temporary agent share code",
		event.command.guild_id
	);
	logMessage->user = event.command.usr;
	logMessage->fields.emplace_back("Agent", agent.uuid());
	m_bot.componentLog(std::move(logMessage));
}

PairComponentPrivate::PairComponentPrivate()
	: m_agentsManager(AgentsManager::Instance())
{

}

std::string PairComponentPrivate::createShareCode(std::string uuid)
{
	std::scoped_lock lock(m_shareCodesMutex);

	const auto now = std::chrono::steady_clock::now();
	for (auto entry = m_shareCodes.begin(); entry != m_shareCodes.end();)
	{
		if (entry->second.expiresAt <= now)
			entry = m_shareCodes.erase(entry);
		else
			++entry;
	}

	ShareCode details{
		.uuid = uuid,
		.expiresAt = now + kShareCodeLifetime
	};

	std::string code;
	constexpr auto kMaxAttempts = 100;
	for (int attempt = 0; attempt < kMaxAttempts; ++attempt)
	{
		code = GenerateShareCode();
		auto [it, inserted] = m_shareCodes.try_emplace(code, details);
		if (inserted)
			return code;
	}

	throw std::runtime_error("Failed to allocate a unique agent share code");
}

std::optional<std::string> PairComponentPrivate::consumeShareCode(std::string_view code)
{
	std::lock_guard lock(m_shareCodesMutex);

	const auto entry = m_shareCodes.find(std::string(code));
	if (entry == m_shareCodes.end())
		return std::nullopt;

	std::optional<std::string> uuid;
	if (entry->second.expiresAt > std::chrono::steady_clock::now())
		uuid = std::move(entry->second.uuid);

	m_shareCodes.erase(entry);

	return uuid;
}
