#pragma once

#include <libwebsockets.h>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

namespace Mojo {
namespace Browser {
namespace CDP {

class CDPClient {
public:
    CDPClient(const std::string& host = "127.0.0.1", int port = 9222);
    ~CDPClient();

    std::string render(const std::string& url);

    bool        connect();
    bool        navigate(const std::string& url);
    std::string evaluate(const std::string& expression);

    struct Context;

private:
    std::string              host_;
    int                      port_;
    std::unique_ptr<Context> ctx_;

    std::string get_web_socket_url();
    bool        connect_to_browser(const std::string& ws_url);
    void        send_message(const nlohmann::json& msg);
};

}  // namespace CDP
}  // namespace Browser
}  // namespace Mojo
