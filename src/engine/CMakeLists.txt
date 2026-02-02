add_library(mojo_engine
    crawler/crawler.cpp
)

target_link_libraries(mojo_engine PUBLIC mojo_browser mojo_proxy mojo_core)
target_include_directories(mojo_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
