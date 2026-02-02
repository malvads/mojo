cmake_minimum_required(VERSION 3.10)
project(mojo VERSION 0.1.0)

if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()


option(ENABLE_FUZZING "Enable fuzzing instrumentation" OFF)

if(ENABLE_FUZZING)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=fuzzer-no-link,address -g -O1")
endif()

# Enable strict warning flags as a "linter"
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies
find_package(Threads REQUIRED)
find_package(CURL REQUIRED)
find_package(Boost REQUIRED)
find_package(nlohmann_json REQUIRED)

# Global Includes (System)
include_directories(/opt/homebrew/include)
link_directories(/opt/homebrew/lib)

find_library(GUMBO_LIBRARY gumbo PATHS /opt/homebrew/lib /usr/local/lib)
find_library(YAML_CPP_LIBRARY yaml-cpp PATHS /opt/homebrew/lib /usr/local/lib)
find_library(WEBSOCKETS_LIBRARY websockets PATHS /opt/homebrew/lib /usr/local/lib)

if(NOT GUMBO_LIBRARY)
    message(WARNING "Gumbo library not found!")
endif()

if(NOT YAML_CPP_LIBRARY)
    message(WARNING "yaml-cpp library not found!")
endif()

if(NOT WEBSOCKETS_LIBRARY)
    message(WARNING "websockets library not found!")
endif()

# Modules (Order matters for dependencies)
add_subdirectory(src/core)
add_subdirectory(src/utils)
add_subdirectory(src/network)
add_subdirectory(src/proxy)
add_subdirectory(src/browser)
add_subdirectory(src/engine)


if(ENABLE_FUZZING)
    add_subdirectory(tests/fuzzers)
endif()

# Testing
enable_testing()
include(FetchContent)
FetchContent_Declare(
  cpp-httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG        v0.14.3
)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
# For Windows: do not override the cache variables
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest cpp-httplib)

add_subdirectory(tests/integration)
add_subdirectory(tests/unit)

# Main Executable
add_executable(mojo src/main.cpp)

# Link
target_link_libraries(mojo 
    PRIVATE 
    mojo_engine
    Threads::Threads
)
