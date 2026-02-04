#include "browser.hpp"
#include "cdp/cdp_client.hpp"

namespace Mojo {
namespace Browser {

class PageImpl : public Page {
public:
    explicit PageImpl(std::shared_ptr<Mojo::Browser::CDP::CDPClient> cdp) : cdp_(cdp) {
    }

    boost::asio::awaitable<bool> goto_url(const std::string& url) override {
        co_return co_await cdp_->navigate(url);
    }

    boost::asio::awaitable<std::string> content() override {
        co_return co_await cdp_->evaluate("document.documentElement.outerHTML");
    }

    boost::asio::awaitable<void> close() override {
        co_await cdp_->close();
    }

    boost::asio::awaitable<std::string> evaluate(const std::string& script) override {
        co_return co_await cdp_->evaluate(script);
    }

private:
    std::shared_ptr<Mojo::Browser::CDP::CDPClient> cdp_;
};

Browser::Browser(boost::asio::io_context& ioc, const std::string& host, int port)
    : ioc_(ioc), host_(host), port_(port) {
}

std::shared_ptr<Browser>
Browser::connect(boost::asio::io_context& ioc, const std::string& host, int port) {
    return std::shared_ptr<Browser>(new Browser(ioc, host, port));
}

boost::asio::awaitable<std::shared_ptr<Page>> Browser::new_page() {
    auto cdp = std::make_shared<Mojo::Browser::CDP::CDPClient>(ioc_, host_, port_);
    if (!co_await cdp->connect()) {
        co_return nullptr;
    }
    co_return std::make_shared<PageImpl>(cdp);
}

void Browser::close() {
    // nothing to do for simple implementation
}

bool Browser::is_connected() const {
    return true;  // Simple stub
}

}  // namespace Browser
}  // namespace Mojo
