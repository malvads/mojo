#include "browser.hpp"
#include "page.hpp"
#include "logger/logger.hpp"
#include "cdp_client.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <iostream>

static const std::string eval_script = "document.documentElement.outerHTML";

namespace Mojo {

class CDPPage : public Page, public std::enable_shared_from_this<CDPPage> {
public:
    CDPPage(std::unique_ptr<CDPClient> client) : client_(std::move(client)) {}

    bool goto_url(const std::string& url) override {
        if (!connected_) {
            if (!client_->connect()) return false;
            connected_ = true;
        }
        return client_->navigate(url);
    }

    std::string content() override {
        return client_->evaluate(eval_script);
    }

    void close() override {
        client_.reset();
    }
    
    std::string evaluate(const std::string& script) override {
        return client_->evaluate(script);
    }

    bool connected_ = false;
    
    std::unique_ptr<CDPClient> client_;
};

Browser::Browser(const std::string& host, int port) : host_(host), port_(port) {}

std::shared_ptr<Browser> Browser::connect(const std::string& host, int port) {
    return std::shared_ptr<Browser>(new Browser(host, port));
}

std::shared_ptr<Page> Browser::new_page() {
    auto client = std::make_unique<CDPClient>(host_, port_);
    return std::make_shared<CDPPage>(std::move(client));
}

void Browser::close() {}

bool Browser::is_connected() const {
    return true;
}

}
