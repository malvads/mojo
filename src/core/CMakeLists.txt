add_library(mojo_core
    config/config.cpp
    logger/logger.cpp
)

target_include_directories(mojo_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CLI11_INCLUDE_DIR} ${YAML_CPP_INCLUDE_DIR})
target_link_libraries(mojo_core PUBLIC ${YAML_CPP_LIBRARY} nlohmann_json::nlohmann_json)
