#pragma once

#include "Component.hpp"
#include "MongoDBManager.hpp"

#include <boost/unordered/unordered_flat_map.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <memory>
#include <mutex>

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;
using bsoncxx::v_noabi::document::value;
using bsoncxx::v_noabi::document::view;

struct Server
{
	std::string name;
	std::string ipAddress;

	/*static Server Construct(const view& view)
	{
		return Server{
		}
	}*/
	//static std::vector<Server> ContructArray()
};

struct ServerConfig
{
	uint64_t guildID;
	uint64_t channelID;
	std::vector<Server> servers;

	static ServerConfig Construct(const view& view)
	{
		return ServerConfig{
			.guildID = (uint64_t)view["guildID"].get_int64().value,
			.channelID = (uint64_t)view["channelID"].get_int64().value,
			//.servers = Server::ConstructArray(view["servers"].get_array())
		};
	}

	value value() const
	{
		return make_document(
			kvp("guildID", (int64_t)guildID),
			kvp("channelID", (int64_t)channelID)
		);
	}
};

class ServerStatusComponent
	: public Component
{
public:
	ServerStatusComponent(dpp::cluster& bot);

	void    onSetStatusChannel(const dpp::slashcommand_t& event);
	void	onAddServerCommand(const dpp::slashcommand_t& event);
	void	onAddServerForm(const dpp::form_submit_t& event);
	void	onSelectServer(const dpp::select_click_t& event);

	// Component
	void	onChannelDelete(const dpp::channel_delete_t& event) override;
	// /Component

private:
	std::mutex			m_mutex;
	mongocxx::pool&		m_databasePool;
	boost::unordered_flat_map<uint64_t, std::unique_ptr<ServerConfig>>	m_configs;
};
