add_executable(integration_tests
    test_integration.cpp
)

target_link_libraries(integration_tests
    PRIVATE
    mojo_engine
    mojo_browser
    mojo_proxy
    GTest::gtest_main
    httplib::httplib
)

include(GoogleTest)
gtest_discover_tests(integration_tests)
