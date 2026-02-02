add_library(mojo_utils
    text/converter.cpp
    text/html2md.cpp
    text/table.cpp
    crypto/murmur3.cpp
    url/url.cpp
    http/parser.cpp
)

target_link_libraries(mojo_utils PUBLIC ${GUMBO_LIBRARY} ${YAML_CPP_LIBRARY} mojo_core)
target_include_directories(mojo_utils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
