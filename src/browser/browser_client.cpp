#include "browser_client.hpp"
#include <algorithm>
#include "../core/logger/logger.hpp"
#include "../core/types/constants.hpp"
#include "../network/http/beast_client.hpp"
#include "browser.hpp"
#include "page.hpp"

namespace Mojo {
namespace Browser {

using namespace Mojo::Core;
using Mojo::Network::Http::BeastClient;

BrowserClient::BrowserClient(boost::asio::io_context& ioc) : ioc_(ioc) {
    Logger::info("Initializing Chromium CDP Browser Engine...");
}

void BrowserClient::set_proxy(const std::string& proxy) {
    proxy_ = proxy;
}

boost::asio::awaitable<bool> BrowserClient::render_to_response(const std::string& url,
                                                               Response&          res) {
    if (url.compare(0, 4, "http") != 0) {
        res.error      = "Invalid URL scheme";
        res.error_type = Network::Http::ErrorType::Other;
        co_return false;
    }

    try {
        auto browser = Browser::connect(ioc_, Browser::kDefaultHost, Browser::kDefaultPort);
        auto page    = co_await browser->new_page();

        if (!page) {
            res.error      = "Failed to create new page (CDP connection failed)";
            res.error_type = Network::Http::ErrorType::Browser;
            co_return false;
        }

        Logger::info("Browser: Navigating to " + url);
        if (!co_await page->goto_url(url)) {
            res.error      = "Browser navigation failed (Timeout or Network)";
            res.error_type = Network::Http::ErrorType::Network;
            co_return false;
        }

        res.body = co_await page->content();
        if (res.body.empty()) {
            res.error      = "Browser returned empty content";
            res.error_type = Network::Http::ErrorType::Render;
            co_return false;
        }

        co_return true;
    } catch (const std::exception& e) {
        res.error      = std::string("Browser Engine Error: ") + e.what();
        res.error_type = Network::Http::ErrorType::Browser;
        co_return false;
    } catch (...) {
        res.error      = "Unknown Browser Error";
        res.error_type = Network::Http::ErrorType::Browser;
        co_return false;
    }
}

boost::asio::awaitable<Response> BrowserClient::get(const std::string& url) {
    {
        BeastClient httpClient(ioc_);
        httpClient.set_proxy(proxy_);
        Response head_res = co_await httpClient.head(url);
        if (head_res.success && Mojo::Core::is_downloadable_mime(head_res.content_type)) {
            Logger::info("Browser: Binary/Document detected (" + head_res.content_type
                         + "), downloading via BeastClient...");
            co_return co_await httpClient.get(url);
        }
    }

    Response               res;
    res.success = co_await render_to_response(url, res);

    if (res.success) {
        res.status_code = static_cast<long>(HTTPCode::Ok);
    }
    else {
        if (res.status_code == 0)
            res.status_code = static_cast<long>(HTTPCode::BrowserError);
        Logger::error("Browser Error [" + url + "]: " + res.error);
    }

    co_return res;
}

boost::asio::awaitable<Response> BrowserClient::head(const std::string& url) {
    BeastClient httpClient(ioc_);
    httpClient.set_proxy(proxy_);
    co_return co_await httpClient.head(url);
}

}  // namespace Browser
}  // namespace Mojo
