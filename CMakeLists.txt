# Minimum CMake version required, we'll just use the latest version.
cmake_minimum_required(VERSION 3.25)

set(CMAKE_TOOLCHAIN_FILE "${VCPKG_CMAKE_FILE}" CACHE STRING "Vcpkg toolchain file")

# Project name, version and description
project(MineCord VERSION 1.0 DESCRIPTION "Discord Minecraft Server Mananger")
    
# list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
# Create an executable
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/DiscordBot/DiscordBot.cpp
)

find_package(OpenSSL REQUIRED)
find_package(dpp CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE dpp::dpp)

target_include_directories(${PROJECT_NAME} PRIVATE
    src/DiscordBot
)
    
set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)