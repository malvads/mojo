#include "browser.hpp"
#include "cdp/cdp_client.hpp"

namespace Mojo {
namespace Browser {

class PageImpl : public Page {
public:
    PageImpl(std::shared_ptr<Mojo::Browser::CDP::CDPClient> cdp) : cdp_(cdp) {
    }

    bool goto_url(const std::string& url) override {
        return cdp_->navigate(url);
    }

    std::string content() override {
        return cdp_->evaluate("document.documentElement.outerHTML");
    }

    void close() override {
        // Optional: close CDP connection if needed
    }

    std::string evaluate(const std::string& script) override {
        return cdp_->evaluate(script);
    }

private:
    std::shared_ptr<Mojo::Browser::CDP::CDPClient> cdp_;
};

Browser::Browser(const std::string& host, int port) : host_(host), port_(port) {
}

std::shared_ptr<Browser> Browser::connect(const std::string& host, int port) {
    return std::shared_ptr<Browser>(new Browser(host, port));
}

std::shared_ptr<Page> Browser::new_page() {
    auto cdp = std::make_shared<Mojo::Browser::CDP::CDPClient>(host_, port_);
    if (!cdp->connect()) {
        return nullptr;
    }
    return std::make_shared<PageImpl>(cdp);
}

void Browser::close() {
    // nothing to do for simple implementation
}

bool Browser::is_connected() const {
    return true;  // Simple stub
}

}  // namespace Browser
}  // namespace Mojo
