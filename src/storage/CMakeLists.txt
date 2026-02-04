add_library(mojo_storage
    disk_storage.cpp
)

target_link_libraries(mojo_storage PUBLIC mojo_core)
target_include_directories(mojo_storage PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
