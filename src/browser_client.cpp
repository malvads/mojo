#include "mojo/browser_client.hpp"
#include "mojo/browser.hpp"
#include "mojo/page.hpp"
#include "mojo/logger.hpp"
#include "mojo/statuses.hpp"

namespace Mojo {

BrowserClient::BrowserClient() {
    Logger::info("Initializing Chromium CDP Browser Engine...");
}

void BrowserClient::set_proxy(const std::string& proxy) {
    proxy_ = proxy;
}

std::string BrowserClient::get_content(const std::string& url) {
    if (url.find("http") != 0) return ""; 

    try {
        auto browser = Browser::connect(Browser::kDefaultHost, Browser::kDefaultPort);
        auto page = browser->new_page();
        
        Logger::info("Browser: Navigating to " + url);
        if (!page->goto_url(url)) {
            Logger::error("Browser: Failed to navigate to " + url);
            return "";
        }
        
        return page->content();
    } catch (const std::exception& e) {
        Logger::error(std::string("Browser: Error during rendering - ") + e.what());
        return "";
    } catch (...) {
        Logger::error("Browser: Unknown error during rendering");
        return "";
    }
}

Response BrowserClient::get(const std::string& url) {
    Response res;
    res.success = false;
    
    try {
        res.body = get_content(url);
        
        if (!res.body.empty()) {
            res.success = true;
            res.status_code = static_cast<long>(Status::Ok);
        } else {
            res.status_code = static_cast<long>(Status::BrowserError);
            res.error = "Chromium CDP rendering failed or returned empty content";
        }
    } catch (const std::exception& e) {
        res.status_code = static_cast<long>(Status::BrowserError);
        res.error = std::string("CDP Error: ") + e.what();
        Logger::error(res.error);
    }

    return res;
}

}
