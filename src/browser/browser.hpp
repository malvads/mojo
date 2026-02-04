#pragma once
#include <memory>
#include <string>
#include <vector>
#include "page.hpp"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>

namespace Mojo {
namespace Browser {

class Browser {
public:
    static constexpr char kDefaultHost[] = "127.0.0.1";
    static constexpr int  kDefaultPort   = 9222;

    static std::shared_ptr<Browser> connect(boost::asio::io_context& ioc,
                                            const std::string&       host = kDefaultHost,
                                            int                      port = kDefaultPort);

    boost::asio::awaitable<std::shared_ptr<Page>> new_page();
    void                                          close();
    bool                                          is_connected() const;

    const std::string& get_host() const {
        return host_;
    }
    int get_port() const {
        return port_;
    }

private:
    Browser(boost::asio::io_context& ioc, const std::string& host, int port);
    boost::asio::io_context& ioc_;
    std::string              host_;
    int                      port_;
};

}  // namespace Browser
}  // namespace Mojo
