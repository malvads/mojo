#pragma once
#include <boost/asio/awaitable.hpp>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

namespace Mojo {

class Page {
public:
    virtual ~Page() = default;

    virtual boost::asio::awaitable<bool>        goto_url(const std::string& url) = 0;
    virtual boost::asio::awaitable<std::string> content()                        = 0;
    virtual boost::asio::awaitable<void>        close()                          = 0;

    virtual boost::asio::awaitable<std::string> evaluate(const std::string& script) = 0;
};

}  // namespace Mojo
