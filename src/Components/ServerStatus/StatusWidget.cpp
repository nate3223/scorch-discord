#include "StatusWidget.hpp"
#include "StatusWidget_p.hpp"

#include <dpp/dpp.h>

namespace
{
	namespace Database
	{
		constexpr char MessageID[]		= "messageID";
		constexpr char AgentMessageID[]	= "agentMessageID";
		constexpr char ActiveServerID[]	= "activeServerID";
	}
}

StatusWidget::StatusWidget()
	: m_p(std::make_unique<StatusWidgetPrivate>())
{

}

StatusWidget::StatusWidget(const bsoncxx::document::view& view)
	: StatusWidget()
{
	if (const auto& messageID = view[Database::MessageID]; messageID)
		m_p->m_messageID = (uint64_t)messageID.get_int64().value;
	if (const auto& agentMessageID = view[Database::AgentMessageID]; agentMessageID)
		m_p->m_agentMessageID = (uint64_t)agentMessageID.get_int64().value;
	if (const auto& activeServerID = view[Database::ActiveServerID]; activeServerID)
		m_p->m_activeServerID = (uint64_t)activeServerID.get_int64().value;
}

StatusWidget::StatusWidget(const StatusWidget& other)
	: m_p(std::make_unique<StatusWidgetPrivate>(*other.m_p))
{
}

StatusWidget::StatusWidget(StatusWidget&& other) noexcept = default;

StatusWidget& StatusWidget::operator=(const StatusWidget& other)
{
	if (this != &other)
		m_p = std::make_unique<StatusWidgetPrivate>(*other.m_p);
	return *this;
}

StatusWidget& StatusWidget::operator=(StatusWidget&& other) noexcept = default;

StatusWidget::~StatusWidget() = default;

bsoncxx::document::value StatusWidget::getValue() const
{
	bsoncxx::builder::basic::document document;
	if (m_p->m_messageID.has_value())
		document.append(kvp(Database::MessageID, (int64_t)*m_p->m_messageID));
	if (m_p->m_agentMessageID.has_value())
		document.append(kvp(Database::AgentMessageID, (int64_t)*m_p->m_agentMessageID));
	if (m_p->m_activeServerID.has_value())
		document.append(kvp(Database::ActiveServerID, (int64_t)*m_p->m_activeServerID));
	return document.extract();
}

dpp::message StatusWidget::getMessage() const
{
	auto message = dpp::message();

	/*dpp::component buttonRow = dpp::component();
	if (m_p->m_activeServerID.has_value())
	{
		message.add_embed(m_p->m_activeServer->getEmbed());
		buttonRow.add_component(
			dpp::component()
			.set_label("Restart")
			.set_id(ServerStatusWidget::RestartServer)
			.set_type(dpp::cot_button)
		).add_component(
			dpp::component()
			.set_label("Start")
			.set_id(ServerStatusWidget::StartServer)
			.set_type(dpp::cot_button)
		).add_component(
			dpp::component()
			.set_label("Stop")
			.set_id(ServerStatusWidget::StopServer)
			.set_type(dpp::cot_button)
		);
	}
	else
	{
		message.add_embed(
			dpp::embed()
			.set_description("No pinned server selected!")
			.set_timestamp(time(0))
		);
	}
	buttonRow.add_component(
		dpp::component()
		.set_label("Settings")
		.set_id(ServerStatusWidget::ServerSettings)
		.set_type(dpp::cot_button)
	);
	message.add_component(buttonRow);

	if (config.m_p->m_statusWidget.m_p->m_activeServerID.has_value() && config.m_p->m_servers.size() > 1)
	{
		message.add_component(
			dpp::component().add_component(
				getServerSelectMenuComponent(config)
				.set_placeholder("Select a server to query")
				.set_id(ServerStatusWidget::QueryServer)
			)
		);
	}*/

	return message;
}

std::optional<uint64_t>& StatusWidget::messageID() noexcept
{
	return m_p->m_messageID;
}

const std::optional<uint64_t>& StatusWidget::messageID() const noexcept
{
	return m_p->m_messageID;
}

void StatusWidget::setMessageID(std::optional<uint64_t> messageID)
{
	m_p->m_messageID = std::move(messageID);
}

std::optional<uint64_t>& StatusWidget::agentMessageID() noexcept
{
	return m_p->m_agentMessageID;
}

const std::optional<uint64_t>& StatusWidget::agentMessageID() const noexcept
{
	return m_p->m_agentMessageID;
}

void StatusWidget::setAgentMessageID(std::optional<uint64_t> agentMessageID)
{
	m_p->m_agentMessageID = std::move(agentMessageID);
}

std::optional<uint64_t>& StatusWidget::commandID() noexcept
{
	return m_p->m_commandID;
}

const std::optional<uint64_t>& StatusWidget::commandID() const noexcept
{
	return m_p->m_commandID;
}

void StatusWidget::setCommandID(std::optional<uint64_t> commandID)
{
	m_p->m_commandID = std::move(commandID);
}

std::shared_ptr<Server>& StatusWidget::activeServer() noexcept
{
	return m_p->m_activeServer;
}

const std::shared_ptr<Server>& StatusWidget::activeServer() const noexcept
{
	return m_p->m_activeServer;
}

void StatusWidget::setActiveServer(std::shared_ptr<Server> activeServer)
{
	m_p->m_activeServer = std::move(activeServer);
}

std::optional<uint64_t>& StatusWidget::activeServerID() noexcept
{
	return m_p->m_activeServerID;
}

const std::optional<uint64_t>& StatusWidget::activeServerID() const noexcept
{
	return m_p->m_activeServerID;
}

void StatusWidget::setActiveServerID(std::optional<uint64_t> activeServerID)
{
	m_p->m_activeServerID = std::move(activeServerID);
}
