#pragma once

#include <boost/asio/awaitable.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <deque>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

namespace Mojo {
namespace Browser {
namespace CDP {

class CDPClient {
public:
    explicit CDPClient(boost::asio::io_context& ioc,
                       const std::string&       host = "127.0.0.1",
                       int                      port = 9222);
    ~CDPClient();

    boost::asio::awaitable<std::string> render(const std::string& url);

    boost::asio::awaitable<bool>        connect();
    boost::asio::awaitable<bool>        navigate(const std::string& url);
    boost::asio::awaitable<std::string> evaluate(const std::string& expression);
    boost::asio::awaitable<void>        close();

private:
    std::string                                               host_;
    int                                                       port_;
    boost::beast::websocket::stream<boost::beast::tcp_stream> ws_;
    bool                                                      connected_ = false;

    int         current_id_ = 1;
    std::string tab_id_;

    std::map<int, nlohmann::json> responses_;
    std::deque<nlohmann::json>    events_;

    boost::asio::awaitable<std::string>    get_web_socket_url();
    boost::asio::awaitable<void>           send_message(const nlohmann::json& msg);
    boost::asio::awaitable<nlohmann::json> read_message();
    boost::asio::awaitable<nlohmann::json> wait_for_id(int id);
    boost::asio::awaitable<nlohmann::json> wait_for_event(const std::string& method);
};

}  // namespace CDP
}  // namespace Browser
}  // namespace Mojo
