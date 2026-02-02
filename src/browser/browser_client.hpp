#include "../network/http/http_client.hpp"

namespace Mojo {
namespace Browser {

using namespace Mojo::Network::Http;

class BrowserClient : public HttpClient {
public:
    explicit BrowserClient();
    ~BrowserClient() override = default;
    
    void set_proxy(const std::string& proxy) override;
    Response get(const std::string& url) override;

private:
    std::string proxy_;
    bool render_to_response(const std::string& url, Response& res);
};

}
}
