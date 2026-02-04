add_library(mojo_browser
    browser_client.cpp
    browser.cpp
    launcher/browser_launcher.cpp
    cdp/cdp_client.cpp
)

target_link_libraries(mojo_browser 
    mojo_core 
    mojo_utils 
    mojo_network 
    nlohmann_json::nlohmann_json 
    Boost::headers
    OpenSSL::SSL
    OpenSSL::Crypto
)
target_include_directories(mojo_browser PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
