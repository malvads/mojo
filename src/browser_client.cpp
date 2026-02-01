#include "mojo/browser_client.hpp"
#include "mojo/browser.hpp"
#include "mojo/page.hpp"
#include "mojo/logger.hpp"
#include "mojo/statuses.hpp"
#include "mojo/curl_client.hpp"
#include "mojo/constants.hpp"
#include <algorithm>

namespace Mojo {

BrowserClient::BrowserClient() {
    Logger::info("Initializing Chromium CDP Browser Engine...");
}

void BrowserClient::set_proxy(const std::string& proxy) {
    proxy_ = proxy;
}

bool BrowserClient::render_to_response(const std::string& url, Response& res) {
    if (url.find("http") != 0) {
        res.error = "Invalid URL scheme";
        res.error_type = ErrorType::Other;
        return false;
    }

    try {
        auto browser = Browser::connect(Browser::kDefaultHost, Browser::kDefaultPort);
        auto page = browser->new_page();
        
        Logger::info("Browser: Navigating to " + url);
        if (!page->goto_url(url)) {
            res.error = "Browser navigation failed (Timeout or Network)";
            res.error_type = ErrorType::Network; // Likely network if it reached here through magic proxy
            return false;
        }
        
        res.body = page->content();
        if (res.body.empty()) {
            res.error = "Browser returned empty content";
            res.error_type = ErrorType::Render;
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        res.error = std::string("Browser Engine Error: ") + e.what();
        res.error_type = ErrorType::Browser;
        return false;
    } catch (...) {
        res.error = "Unknown Browser Error";
        res.error_type = ErrorType::Browser;
        return false;
    }
}

Response BrowserClient::get(const std::string& url) {
    {
        CurlClient curl;
        curl.set_proxy(proxy_);
        Response head_res = curl.head(url);
        if (head_res.success && Constants::is_downloadable_mime(head_res.content_type)) {
             Logger::info("Browser: Binary/Document detected (" + head_res.content_type + "), downloading via Curl...");
             return curl.get(url);
        }
    }

    Response res;
    res.success = render_to_response(url, res);
    
    if (res.success) {
        res.status_code = static_cast<long>(Status::Ok);
    } else {
        if (res.status_code == 0) res.status_code = static_cast<long>(Status::BrowserError);
        Logger::error("Browser Error [" + url + "]: " + res.error);
    }

    return res;
}

}
