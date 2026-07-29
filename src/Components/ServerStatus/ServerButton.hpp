#pragma once

#include "Database/MongoDB/Document.hpp"
#include "Components/Common/StaticCache.hpp"

#include <cstdint>
#include <memory>
#include <regex>
#include <string>

class ServerButtonPrivate;
class ServerButton final
	: public Document<ServerButton>
{
public:
								ServerButton();
								ServerButton(uint64_t id, std::string name, std::string endpoint, uint64_t serverId);
								ServerButton(const bsoncxx::document::view& view, uint64_t serverId);
								ServerButton(const ServerButton& other);
								ServerButton(ServerButton&& other) noexcept;
	ServerButton&				operator=(const ServerButton& other);
	ServerButton&				operator=(ServerButton&& other) noexcept;
								~ServerButton();

	bool						press();
	uint64_t					id() const noexcept;
	const std::string&			name() const noexcept;
	const std::string&			endpoint() const noexcept;
	const std::string&			componentId() const noexcept;

// Document i/f:
public:
	bsoncxx::document::value	getValue() const override;

private:
	static std::string			formatComponentId(uint64_t serverId, uint64_t id);

	std::unique_ptr<ServerButtonPrivate>	m_p;
};
