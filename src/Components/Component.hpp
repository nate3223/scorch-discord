#pragma once


#include <algorithm>
#include <dpp/dpp.h>
#include <functional>
#include <string>
#include <vector>

typedef std::function<void(const dpp::slashcommand_t&)> SlashFunction;
struct SlashCommand
{
	SlashFunction slashFunction;
	dpp::slashcommand slashCommand;
};

typedef std::function<void(const dpp::button_click_t&)> ButtonFunction;
struct ButtonCommand
{
	std::string id;
	ButtonFunction buttonFunction;
};

typedef std::function<void(const dpp::select_click_t&)> SelectFunction;
struct SelectCommand
{
	std::string id;
	SelectFunction selectFunction;
};

typedef std::function<void(const dpp::form_submit_t&)> FormFunction;
struct FormCommand
{
	std::string id;
	FormFunction formFunction;
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