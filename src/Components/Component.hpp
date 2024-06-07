#pragma once

#include <dpp/dpp.h>

#include <algorithm>
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

class Component
{
public:
	explicit Component(dpp::cluster& bot) : m_bot(bot) {}

	std::vector<SlashCommand>	getSlashCommands()	{ return m_slashCommands; }
	std::vector<ButtonCommand>  getButtonCommands()	{ return m_buttonCommands; }
	std::vector<SelectCommand>	getSelectCommands()	{ return m_selectCommands; }
	std::vector<FormCommand>	getFormCommands()	{ return m_formCommands; }

	virtual void				onChannelDelete(const dpp::channel_delete_t& event)	{}

protected:
	dpp::cluster&				m_bot;
	std::vector<SlashCommand>	m_slashCommands;
	std::vector<ButtonCommand>	m_buttonCommands;
	std::vector<SelectCommand>	m_selectCommands;
	std::vector<FormCommand>	m_formCommands;
};