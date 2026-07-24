# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.25)

if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
	set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
endif()

# Project name, version and description
project(Scorch VERSION 1.0 DESCRIPTION "Discord Game Server Manager")

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
    if(MSVC)
        set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "ProgramDatabase")
    endif()
endif()

find_package(OpenSSL REQUIRED)
find_package(dpp CONFIG REQUIRED)

find_package(mongocxx CONFIG REQUIRED)
find_package(bsoncxx CONFIG REQUIRED)

find_package(Opus CONFIG REQUIRED)

find_package(unofficial-sodium CONFIG REQUIRED)

# Create an executable
add_executable(${PROJECT_NAME}
	src/main.cpp
	src/Log.cpp

	src/DiscordBot/DiscordBot.cpp

	src/Components/Log/LogComponent.cpp
	src/Components/Log/LogConfig.cpp

	src/Components/ServerStatus/Server.cpp
	src/Components/ServerStatus/ServerButton.cpp
	src/Components/ServerStatus/ServerConfig.cpp
	src/Components/ServerStatus/ServerStatusComponent.cpp
	src/Components/ServerStatus/StatusWidget.cpp

	src/Components/Yippee/YippeeComponent.cpp

	src/Database/DatabaseManager.cpp
	
	src/Database/MongoDB/MongoDBManager.cpp
 )

set_target_properties(${PROJECT_NAME} PROPERTIES
	CXX_STANDARD 23
	CXX_STANDARD_REQUIRED ON
)

target_link_libraries(${PROJECT_NAME} PRIVATE $<IF:$<TARGET_EXISTS:mongo::bsoncxx_static>,mongo::bsoncxx_static,mongo::bsoncxx_shared>)
target_link_libraries(${PROJECT_NAME} PRIVATE $<IF:$<TARGET_EXISTS:mongo::mongocxx_static>,mongo::mongocxx_static,mongo::mongocxx_shared>)
target_link_libraries(${PROJECT_NAME} PRIVATE Opus::opus)
target_link_libraries(${PROJECT_NAME} PRIVATE dpp::dpp)
target_link_libraries(${PROJECT_NAME} PRIVATE unofficial-sodium::sodium)

if(NOT TARGET spdlog)
	# Stand-alone build
	find_package(spdlog REQUIRED)
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>)

add_compile_options(-Wall -Wextra -Wpedantic -Wpessimizing-move -Wredundant-move)

target_include_directories(${PROJECT_NAME} PRIVATE
	src
	src/Components
	src/Components/Common
	src/Components/Log
	src/Components/ServerStatus
	src/Components/Yippee
	src/DiscordBot
	src/Database
	src/Database/MongoDB
)
