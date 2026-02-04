add_library(mojo_engine
    crawler/crawler.cpp
    crawler/impl/lifecycle.cpp
    crawler/impl/worker.cpp
    crawler/impl/storage.cpp
    crawler/impl/robots.cpp
)

target_link_libraries(mojo_engine PUBLIC mojo_browser mojo_proxy mojo_core mojo_storage Boost::headers)
target_include_directories(mojo_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
