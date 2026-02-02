add_library(mojo_core
    config/config.cpp
    logger/logger.cpp
)

target_include_directories(mojo_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
