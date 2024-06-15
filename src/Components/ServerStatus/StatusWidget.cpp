#include "StatusWidget.hpp"

namespace
{
	namespace Database
	{
		constexpr char MessageID[]		= "messageID";
		constexpr char ActiveServerID[] = "activeServerID";
	}
}

StatusWidget::StatusWidget(const bsoncxx::document::view& view)
{
	if (const auto& messageID = view[Database::MessageID]; messageID)
		m_messageID = (uint64_t)messageID.get_int64().value;
	if (const auto& activeServerID = view[Database::ActiveServerID]; activeServerID)
		m_activeServerID = (uint64_t)activeServerID.get_int64().value;
}

bsoncxx::document::value StatusWidget::getValue() const
{
	bsoncxx::builder::basic::document document;
	if (m_messageID.has_value())
		document.append(kvp(Database::MessageID, (int64_t)*m_messageID));
	if (m_activeServerID.has_value())
		document.append(kvp(Database::ActiveServerID, (int64_t)*m_activeServerID));
	return document.extract();
}

dpp::message StatusWidget::getMessage() const
{
	auto message = dpp::message();

	/*dpp::component buttonRow = dpp::component();
	if (m_activeServerID.has_value())
	{
		message.add_embed(m_activeServer->getEmbed());
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

	if (config.m_statusWidget.m_activeServerID.has_value() && config.m_servers.size() > 1)
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
