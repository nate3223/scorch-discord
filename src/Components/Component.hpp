#pragma once

#include <algorithm>
#include <dpp/dpp.h>
#include <functional>
#include <string>
#include <variant>
#include <vector>

enum class MatchType
{
	EXACT,
	PREFIX
};

struct SlashCommand
{
	using RegularHandler	= std::function<void(const dpp::slashcommand_t&)>;
	using TaskHandler		= std::function<dpp::task<void>(const dpp::slashcommand_t&)>;
	using Handler			= std::variant<RegularHandler, TaskHandler>;

	Handler				handler;
	dpp::slashcommand	slashCommand;
};

struct ButtonCommand
{
	using RegularHandler	= std::function<void(const dpp::button_click_t&)>;
	using TaskHandler		= std::function<dpp::task<void>(const dpp::button_click_t&)>;
	using Handler			= std::variant<RegularHandler, TaskHandler>;

	std::string	id;
	Handler		handler;
	MatchType	type = MatchType::EXACT;
};

struct SelectCommand
{
	using RegularHandler	= std::function<void(const dpp::select_click_t&)>;
	using TaskHandler		= std::function<dpp::task<void>(const dpp::select_click_t&)>;
	using Handler			= std::variant<RegularHandler, TaskHandler>;

	std::string	id;
	Handler		handler;
	MatchType	type = MatchType::EXACT;
};

struct FormCommand
{
	using RegularHandler	= std::function<void(const dpp::form_submit_t&)>;
	using TaskHandler		= std::function<dpp::task<void>(const dpp::form_submit_t&)>;
	using Handler			= std::variant<RegularHandler, TaskHandler>;

	std::string	id;
	Handler		handler;
	MatchType	type = MatchType::EXACT;
};

class ComponentLogMessage
{
public:
	ComponentLogMessage(const std::string& message) : message(message) {}
	virtual ~ComponentLogMessage() = default;

	std::string	message;
};

class BroadcastMessage : public ComponentLogMessage
{
public:
	using ComponentLogMessage::ComponentLogMessage;
};

class GuildMessage : public ComponentLogMessage
{
public:
	GuildMessage(const std::string& message, const dpp::snowflake guildID) : ComponentLogMessage(message), guildID(guildID) {}
	dpp::snowflake	guildID;
};

class GuildEmbedMessage : public ComponentLogMessage
{
public:
	GuildEmbedMessage(const std::string& message, const dpp::snowflake guildID)
		: ComponentLogMessage(message)
		, guildID(guildID)
	{
	}
	dpp::snowflake	guildID;
	std::optional<dpp::user>		user;
	std::vector<dpp::embed_field>	fields;
};

class DiscordBot;

class Component
{
public:
	explicit Component(DiscordBot& bot) : m_bot(bot) {}
	virtual ~Component() = default;

	std::vector<SlashCommand>	getSlashCommands()	{ return m_slashCommands; }
	std::vector<ButtonCommand>  getButtonCommands()	{ return m_buttonCommands; }
	std::vector<SelectCommand>	getSelectCommands()	{ return m_selectCommands; }
	std::vector<FormCommand>	getFormCommands()	{ return m_formCommands; }

	virtual void				onChannelDelete(const dpp::channel_delete_t& event)	{}
	virtual void				onMessageDelete(const dpp::message_delete_t& event) {}
	virtual void				onComponentLog(const ComponentLogMessage* message) {}

protected:
	DiscordBot&					m_bot;
	std::vector<SlashCommand>	m_slashCommands;
	std::vector<ButtonCommand>	m_buttonCommands;
	std::vector<SelectCommand>	m_selectCommands;
	std::vector<FormCommand>	m_formCommands;
};

#include "DiscordBot.hpp"