#pragma once
#include <string>
#include <memory>
#include <vector>
#include "page.hpp"

namespace Mojo {
namespace Browser {

class Browser {
public:
    static constexpr char kDefaultHost[] = "127.0.0.1";
    static constexpr int kDefaultPort = 9222;

    static std::shared_ptr<Browser> connect(const std::string& host = kDefaultHost, int port = kDefaultPort);
    
    std::shared_ptr<Page> new_page();
    void close();
    bool is_connected() const;

    std::string get_host() const { return host_; }
    int get_port() const { return port_; }

private:
    Browser(const std::string& host, int port);
    std::string host_;
    int port_;
};

}
}


