add_library(mojo_proxy
    server/proxy_server.cpp
    server/connection.cpp
    pool/proxy_pool.cpp
)

target_link_libraries(mojo_proxy PUBLIC mojo_core mojo_utils Boost::boost)
target_include_directories(mojo_proxy PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
