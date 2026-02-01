cmake_minimum_required(VERSION 3.10)

file(READ "VERSION" VER)
string(STRIP "${VER}" VER)
project(Mojo VERSION ${VER})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Dependencies
find_package(CURL REQUIRED)
find_library(GUMBO_LIBRARY NAMES gumbo gumbo-parser REQUIRED)
find_path(GUMBO_INCLUDE_DIR NAMES gumbo.h REQUIRED)
find_package(Threads REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(yaml-cpp REQUIRED)
find_library(WEBSOCKETS_LIBRARY NAMES websockets REQUIRED)

# Include Directories
include_directories(
    include
    include/vendor
    ${GUMBO_INCLUDE_DIR}
    ${CURL_INCLUDE_DIRS}
)

# Source Files
file(GLOB_RECURSE SOURCES "src/*.cpp")

# Executable
add_executable(mojo ${SOURCES})

# Linking
target_link_libraries(mojo PRIVATE 
    ${CURL_LIBRARIES}
    ${GUMBO_LIBRARY}
    ${WEBSOCKETS_LIBRARY}
    nlohmann_json::nlohmann_json
    yaml-cpp
    Threads::Threads
)
