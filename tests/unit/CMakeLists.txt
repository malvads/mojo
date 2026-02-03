add_executable(unit_tests
    test_url.cpp
    test_http_parser.cpp
    test_config.cpp
    test_logger.cpp
    test_text.cpp
    test_crypto.cpp
    test_proxy_pool.cpp
    test_browser_launcher.cpp
    test_robotstxt.cpp
)

target_link_libraries(unit_tests
    PRIVATE
    mojo_browser
    mojo_proxy
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(unit_tests)
