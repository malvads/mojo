add_library(mojo_network
    http/beast_client.cpp
    proxy/socks_handshake.cpp
)

target_link_libraries(mojo_network PUBLIC 
    mojo_core
    Boost::headers
    OpenSSL::SSL
    OpenSSL::Crypto
)
target_include_directories(mojo_network PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
