# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.25)

set(CMAKE_TOOLCHAIN_FILE "${VCPKG_CMAKE_FILE}" CACHE STRING "Vcpkg toolchain file")

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("Discord-Minecraft")

# Project name, version and description
project(MineCord VERSION 1.0 DESCRIPTION "Discord Minecraft Server Mananger")
    
find_package(bsoncxx CONFIG REQUIRED)
find_package(mongocxx CONFIG REQUIRED)

find_package(OpenSSL REQUIRED)
find_package(dpp CONFIG REQUIRED)

# list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
# Create an executable
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/DiscordBot/DiscordBot.cpp
    src/Components/YippeeComponent.cpp
    src/Components/ServerStatusComponent.cpp
	src/Database/MongoDBManager.cpp
	src/Database/DatabaseManager.cpp
	src/Log.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE $<IF:$<TARGET_EXISTS:mongo::bsoncxx_static>,mongo::bsoncxx_static,mongo::bsoncxx_shared>)
target_link_libraries(${PROJECT_NAME} PRIVATE $<IF:$<TARGET_EXISTS:mongo::mongocxx_static>,mongo::mongocxx_static,mongo::mongocxx_shared>)

target_link_libraries(${PROJECT_NAME} PRIVATE dpp::dpp)

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
)
    
set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)