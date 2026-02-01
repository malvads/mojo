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

# Include Directories
include_directories(
    include
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
    Threads::Threads
)

if (WIN32)
    # Windows specific definitions if needed
    add_definitions(-DCURL_STATICLIB)
endif()
