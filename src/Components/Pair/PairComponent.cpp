#include "PairComponent.hpp"

#include "AgentsManager.hpp"
#include "DiscordBot.hpp"
#include "PairingCodeRequest.hpp"
#include "TaskAdapter.hpp"

#include <dpp/unicode_emoji.h>

#include <format>
#include <memory>

namespace
{
	using namespace scorch::server;
	namespace PairRequest
	{
		constexpr auto PairCode = "pair-code";
	}
}

PairComponent::PairComponent(DiscordBot& bot)
	: Component(bot)
	, m_agentsManager(bot.getAgentsManager())
{
	m_slashCommands.emplace_back(
		SlashCommand::TaskHandler{[this](const dpp::slashcommand_t& event) -> dpp::task<void> { co_await onPairRequest(event); }},
		dpp::slashcommand("pair", "Pairs an agent to this server, given a valid pairing code.", m_bot->me.id)
			.add_option(
				dpp::command_option(dpp::co_string, PairRequest::PairCode, "Pairing code provided by the agent", true)
			)
			.set_default_permissions(0)
	);
}

dpp::task<void> PairComponent::onPairRequest(const dpp::slashcommand_t& event)
{
	const auto pairCode = std::get<std::string>(event.get_parameter(PairRequest::PairCode));
	const auto& user = event.command.usr;
	const dpp::snowflake& guildId = event.command.guild_id;
	
	std::shared_ptr<PairingCodeRequest> pairingCodeRequest = m_agentsManager.confirmPairing(
		pairCode,
		std::format("User {} ({}) in guild {}", user.username, user.id.str(), guildId.str())
	);
	if (pairingCodeRequest)
	{
		dpp::message msg(std::format("Waiting for Agent {} to approve pairing...", pairingCodeRequest->agentUUID));
		msg.set_flags(dpp::m_ephemeral);
		auto waitingMsg = event.co_reply(msg);

		auto pairingResult = co_await dpp::TaskAdapter<boost::asio::awaitable<PairingCodeResult>>::Await(
			pairingCodeRequest->waitUntil(std::chrono::minutes(5)),
			pairingCodeRequest->timer.get_executor()
		);

		co_await waitingMsg;

		if (pairingResult == PairingCodeResult::Timeout)
		{
			co_await event.co_edit_original_response(dpp::message(std::format("{} Pairing with Agent `{}` **Timed out**!", dpp::unicode_emoji::hourglass, pairingCodeRequest->agentUUID)));
			co_return;
		}
		else if (pairingResult != PairingCodeResult::Approved)
		{
			co_await event.co_edit_original_response(dpp::message(std::format("{} Pairing with Agent `{}` **Rejected**!", dpp::unicode_emoji::x, pairingCodeRequest->agentUUID)));
			co_return;
		}

		auto reply = std::format("{} Pairing with Agent `{}` **Approved**!", dpp::unicode_emoji::white_check_mark, pairingCodeRequest->agentUUID);

		auto response = event.co_edit_original_response(dpp::message(reply));
		auto logMessage = std::make_unique<GuildEmbedMessage>(reply, guildId);
		logMessage->user = user;
		m_bot.componentLog(std::move(logMessage));

		co_await response;
	}
}