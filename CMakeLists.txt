# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.25)

if(DEFINED $ENV{VCPKG_CMAKE_FILE})
	set(VCPKG_CMAKE_FILE $ENV{VCPKG_CMAKE_FILE})
	include(${VCPKG_CMAKE_FILE})
	set(CMAKE_TOOLCHAIN_FILE "${VCPKG_CMAKE_FILE}" CACHE STRING "Vcpkg toolchain file")
endif()

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

# Project name, version and description
project(Scorch VERSION 1.0 DESCRIPTION "Discord Game Server Manager")

find_package(OpenSSL REQUIRED)
find_package(dpp CONFIG REQUIRED)

find_package(mongocxx CONFIG REQUIRED)
find_package(bsoncxx CONFIG REQUIRED)

find_package(Opus CONFIG REQUIRED)

find_package(unofficial-sodium CONFIG REQUIRED)

# Create an executable
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/DiscordBot/DiscordBot.cpp
	src/Components/LogComponent.cpp
	src/Components/ServerStatusComponent.cpp
    src/Components/YippeeComponent.cpp
	src/Database/MongoDB/MongoDBManager.cpp
	src/Database/DatabaseManager.cpp
	src/Log.cpp
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 20
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


target_include_directories(${PROJECT_NAME} PRIVATE
    src
    src/Components
    src/DiscordBot
	src/Database
	src/Database/MongoDB
)
