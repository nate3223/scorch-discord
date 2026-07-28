cmake_minimum_required (VERSION 3.25)

if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
	set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
endif()

project(Scorch VERSION 1.0.0 DESCRIPTION "Discord Game Server Manager")

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
    if(MSVC)
        set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "ProgramDatabase")
    endif()
endif()

include(FetchContent)

FetchContent_Declare(
	ScorchServer
	GIT_REPOSITORY https://github.com/nate3223/scorch-server.git
	GIT_TAG main
)
FetchContent_MakeAvailable(ScorchServer)

find_package(OpenSSL REQUIRED)
find_package(dpp CONFIG REQUIRED)

find_package(mongocxx CONFIG REQUIRED)
find_package(bsoncxx CONFIG REQUIRED)

find_package(Opus CONFIG REQUIRED)

find_package(unofficial-sodium CONFIG REQUIRED)

find_package(Boost REQUIRED COMPONENTS asio)

if(NOT TARGET spdlog::spdlog)
	find_package(spdlog REQUIRED)
endif()

add_executable(${PROJECT_NAME}
	src/main.cpp
	src/Log.cpp
	src/Log.hpp

	src/Agents/AgentIdentityStore.hpp
	src/Agents/AgentsManager.cpp	
	src/Agents/AgentsManager.hpp
	src/Agents/AgentsManager_p.hpp	

	src/Components/Component.hpp

	src/Components/Common/Cache.hpp
	src/Components/Common/StaticCache.hpp

	src/Components/Log/LogComponent.cpp
	src/Components/Log/LogComponent.hpp
	src/Components/Log/LogComponent_p.hpp
	src/Components/Log/LogConfig.cpp
	src/Components/Log/LogConfig.hpp

	src/Components/Pair/PairComponent.cpp
	src/Components/Pair/PairComponent.hpp

	src/Components/ServerStatus/Server.cpp
	src/Components/ServerStatus/Server.hpp
	src/Components/ServerStatus/ServerButton.cpp
	src/Components/ServerStatus/ServerButton.hpp
	src/Components/ServerStatus/ServerConfig.cpp
	src/Components/ServerStatus/ServerConfig.hpp
	src/Components/ServerStatus/ServerStatusComponent.cpp
	src/Components/ServerStatus/ServerStatusComponent.hpp
	src/Components/ServerStatus/StatusWidget.cpp
	src/Components/ServerStatus/StatusWidget.hpp

	src/Components/Yippee/YippeeComponent.cpp
	src/Components/Yippee/YippeeComponent.hpp

	src/Database/DatabaseManager.cpp
	src/Database/DatabaseManager.hpp

	src/Database/MongoDB/Document.hpp
	src/Database/MongoDB/MongoDBAgentIdentityStore.cpp
	src/Database/MongoDB/MongoDBAgentIdentityStore.hpp
	src/Database/MongoDB/MongoDBManager.cpp
	src/Database/MongoDB/MongoDBManager.hpp

	src/DiscordBot/DiscordBot.cpp
	src/DiscordBot/DiscordBot.hpp
	src/DiscordBot/DiscordBot_p.hpp

	src/Task/ASIOTaskState.hpp
	src/Task/ReadyTaskState.hpp
)

target_compile_features(${PROJECT_NAME}
	PUBLIC
		cxx_std_23
)

target_link_libraries(${PROJECT_NAME}
	PRIVATE
		Boost::asio
		Scorch::Protocol
		Scorch::Server
		Opus::opus
		dpp::dpp
		unofficial-sodium::sodium
		spdlog::spdlog
		$<$<BOOL:${MINGW}>:ws2_32>
		$<IF:$<TARGET_EXISTS:mongo::bsoncxx_static>,mongo::bsoncxx_static,mongo::bsoncxx_shared>
		$<IF:$<TARGET_EXISTS:mongo::mongocxx_static>,mongo::mongocxx_static,mongo::mongocxx_shared>
)

target_compile_options(${PROJECT_NAME}
	PRIVATE
		$<$<CXX_COMPILER_ID:MSVC>:/W4>
		$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:
			-Wall
			-Wextra
			-Wpedantic
			-Wpessimizing-move
			-Wredundant-move
		>
)

target_include_directories(${PROJECT_NAME}
	PRIVATE
		src
)

set(SERVER_CERTS
	${CMAKE_CURRENT_SOURCE_DIR}/server.crt
	${CMAKE_CURRENT_SOURCE_DIR}/server.key
)

add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SERVER_CERTS}
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMENT "Copying server certificates to output directory..."
)
