add_library(mojo_network
    http/curl_client.cpp
)

target_link_libraries(mojo_network PUBLIC CURL::libcurl mojo_core)
target_include_directories(mojo_network PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
