#include "cdp_client.hpp"
#include <chrono>
#include <iostream>
#include "../../core/logger/logger.hpp"
#include "../../core/types/constants.hpp"
#include "../../network/http/beast_client.hpp"

namespace Mojo {
namespace Browser {
namespace CDP {

namespace beast     = boost::beast;
namespace http      = beast::http;
namespace websocket = beast::websocket;
namespace net       = boost::asio;
using tcp           = net::ip::tcp;
using namespace Mojo::Core;

CDPClient::CDPClient(net::io_context& ioc, const std::string& host, int port)
    : host_(host), port_(port), ws_(net::make_strand(ioc)) {
}

CDPClient::~CDPClient() {
}

#ifndef MOJO_DISABLE_COROUTINES

net::awaitable<std::string> CDPClient::get_web_socket_url() {
    try {
        tcp::resolver resolver(co_await net::this_coro::executor);
        auto          results =
            co_await  resolver.async_resolve(host_, std::to_string(port_), net::use_awaitable);

        beast::tcp_stream stream(co_await net::this_coro::executor);
        co_await          stream.async_connect(results, net::use_awaitable);

        http::request<http::string_body> req{http::verb::put, "/json/new", 11};
        req.set(http::field::host, host_);
        req.set(http::field::user_agent, Constants::USER_AGENT);

        co_await http::async_write(stream, req, net::use_awaitable);

        beast::flat_buffer                b;
        http::response<http::string_body> res;
        co_await                          http::async_read(stream, b, res, net::use_awaitable);

        auto j  = nlohmann::json::parse(res.body());
        tab_id_ = j["id"];
        co_return j["webSocketDebuggerUrl"];

    } catch (const std::exception& e) {
        Logger::error("CDP: Failed to get WebSocket URL: " + std::string(e.what()));
        co_return "";
    }
}

net::awaitable<bool> CDPClient::connect() {
    std::string ws_url = co_await get_web_socket_url();
    if (ws_url.empty())
        co_return false;

    try {
        auto        pos  = ws_url.find(std::to_string(port_));
        std::string path = "/";
        if (pos != std::string::npos) {
            auto slash = ws_url.find("/", pos);
            if (slash != std::string::npos)
                path = ws_url.substr(slash);
        }

        tcp::resolver resolver(co_await net::this_coro::executor);
        auto          results =
            co_await  resolver.async_resolve(host_, std::to_string(port_), net::use_awaitable);

        beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));
        co_await beast::get_lowest_layer(ws_).async_connect(results, net::use_awaitable);

        beast::get_lowest_layer(ws_).expires_never();

        ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));
        ws_.set_option(websocket::stream_base::decorator([](websocket::request_type& req) {
            req.set(http::field::user_agent, Constants::USER_AGENT);
        }));

        co_await ws_.async_handshake(host_ + ":" + std::to_string(port_), path, net::use_awaitable);
        connected_ = true;
        co_return true;
    } catch (const std::exception& e) {
        Logger::error("CDP: Connect failed: " + std::string(e.what()));
        co_return false;
    }
}

net::awaitable<void> CDPClient::send_message(const nlohmann::json& msg) {
    std::string str = msg.dump();
    co_await    ws_.async_write(net::buffer(str), net::use_awaitable);
}

net::awaitable<nlohmann::json> CDPClient::read_message() {
    beast::flat_buffer buffer;
    co_await           ws_.async_read(buffer, net::use_awaitable);
    std::string        s = beast::buffers_to_string(buffer.data());

    try {
        auto j = nlohmann::json::parse(s);
        if (j.contains("id")) {
            responses_[j["id"]] = j;
        }
        else if (j.contains("method")) {
            events_.push_back(j);
        }
        co_return j;
    } catch (...) {
        co_return nlohmann::json{};
    }
}

net::awaitable<nlohmann::json> CDPClient::wait_for_id(int id) {
    // Check if response already cached
    if (responses_.count(id)) {
        auto res = responses_[id];
        responses_.erase(id);
        co_return res;
    }

    auto       start   = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::seconds(30);

    while (true) {
        auto j = co_await read_message();

        // read_message already stored it in responses_ if it has an id
        if (responses_.count(id)) {
            auto res = responses_[id];
            responses_.erase(id);
            co_return res;
        }

        if (std::chrono::steady_clock::now() - start > timeout) {
            throw std::runtime_error("CDP Timeout waiting for ID " + std::to_string(id));
        }
    }
}

net::awaitable<nlohmann::json> CDPClient::wait_for_event(const std::string& method) {
    // Check if event already in queue
    auto find_event = [&]() {
        return std::find_if(
            events_.begin(), events_.end(), [&](const auto& ev) { return ev["method"] == method; });
    };

    auto it = find_event();
    if (it != events_.end()) {
        auto ev = *it;
        events_.erase(it);
        co_return ev;
    }

    auto       start   = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::seconds(30);

    while (true) {
        auto j = co_await read_message();

        // Check if the event we want just arrived (read_message stores it in events_)
        auto it2 = find_event();
        if (it2 != events_.end()) {
            auto ev = *it2;
            events_.erase(it2);
            co_return ev;
        }

        if (std::chrono::steady_clock::now() - start > timeout) {
            throw std::runtime_error("CDP Timeout waiting for event " + method);
        }
    }
}

net::awaitable<bool> CDPClient::navigate(const std::string& url) {
    if (!connected_)
        co_return false;

    int      enable_id = current_id_++;
    co_await send_message({{"id", enable_id}, {"method", "Page.enable"}});
    co_await wait_for_id(enable_id);

    int      nav_id = current_id_++;
    co_await send_message(
        {{"id", nav_id}, {"method", "Page.navigate"}, {"params", {{"url", url}}}});

    auto ack = co_await wait_for_id(nav_id);
    if (ack.contains("error")) {
        Logger::error("CDP: Navigate error: " + ack["error"].dump());
        co_return false;
    }

    co_await wait_for_event("Page.loadEventFired");
    co_return true;
}

net::awaitable<std::string> CDPClient::evaluate(const std::string& expression) {
    int      id = current_id_++;
    co_await send_message({{"id", id},
                           {"method", "Runtime.evaluate"},
                           {"params", {{"expression", expression}, {"returnByValue", true}}}});

    auto msg = co_await wait_for_id(id);
    if (msg.contains("result") && msg["result"].contains("result")
        && msg["result"]["result"].contains("value")) {
        co_return msg["result"]["result"]["value"].get<std::string>();
    }
    co_return "";
}

net::awaitable<std::string> CDPClient::render(const std::string& url) {
    if (!co_await connect())
        co_return "";
    if (!co_await navigate(url))
        co_return "";
    co_return co_await evaluate("document.documentElement.outerHTML");
}

net::awaitable<void> CDPClient::close() {
    if (tab_id_.empty())
        co_return;

    try {
        tcp::resolver resolver(co_await net::this_coro::executor);
        auto          results =
            co_await  resolver.async_resolve(host_, std::to_string(port_), net::use_awaitable);

        beast::tcp_stream stream(co_await net::this_coro::executor);
        co_await          stream.async_connect(results, net::use_awaitable);

        http::request<http::empty_body> req{http::verb::get, "/json/close/" + tab_id_, 11};
        req.set(http::field::host, host_);
        req.set(http::field::user_agent, Constants::USER_AGENT);

        co_await http::async_write(stream, req, net::use_awaitable);

        beast::flat_buffer                b;
        http::response<http::string_body> res;
        co_await                          http::async_read(stream, b, res, net::use_awaitable);

        if (connected_) {
            co_await ws_.async_close(websocket::close_code::normal, net::use_awaitable);
            connected_ = false;
        }
    } catch (const std::exception& e) {
        Logger::error("CDP: Failed to close tab: " + std::string(e.what()));
    }
}

#else

net::awaitable<std::string> CDPClient::get_web_socket_url() {
    Logger::error("CDP: Coroutines disabled - built with GCC < 12. Browser rendering unavailable.");
    co_return "";
}

net::awaitable<bool> CDPClient::connect() {
    Logger::error("CDP: Coroutines disabled - built with GCC < 12. Browser rendering unavailable.");
    co_return false;
}

net::awaitable<void> CDPClient::send_message(const nlohmann::json&) {
    co_return;
}

net::awaitable<nlohmann::json> CDPClient::read_message() {
    co_return nlohmann::json{};
}

net::awaitable<nlohmann::json> CDPClient::wait_for_id(int) {
    co_return nlohmann::json{};
}

net::awaitable<nlohmann::json> CDPClient::wait_for_event(const std::string&) {
    co_return nlohmann::json{};
}

net::awaitable<bool> CDPClient::navigate(const std::string&) {
    co_return false;
}

net::awaitable<std::string> CDPClient::evaluate(const std::string&) {
    co_return "";
}

net::awaitable<std::string> CDPClient::render(const std::string&) {
    Logger::error("CDP: Browser rendering unavailable - built with GCC < 12 (coroutines disabled)");
    co_return "";
}

net::awaitable<void> CDPClient::close() {
    co_return;
}

#endif  // MOJO_DISABLE_COROUTINES

}  // namespace CDP
}  // namespace Browser
}  // namespace Mojo
