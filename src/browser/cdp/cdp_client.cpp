#include "cdp_client.hpp"
#include <chrono>
#include <curl/curl.h>
#include <deque>
#include <iostream>
#include <limits>
#include <thread>
#include "logger/logger.hpp"

namespace Mojo {
namespace Browser {
namespace CDP {

using namespace Mojo::Core;

struct CDPClient::Context {
    struct lws_context* lws_ctx = nullptr;
    struct lws*         wsi     = nullptr;

    std::deque<std::string> message_queue;
    std::string             partial_data;
    bool                    connected = false;

    int         current_id = 1;
    std::string tab_id;

    std::string pending_message;
    bool        write_pending = false;
};

static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

CDPClient::CDPClient(const std::string& host, int port)
    : host_(host), port_(port), ctx_(std::make_unique<Context>()) {
    lws_set_log_level(0, nullptr);
}

CDPClient::~CDPClient() {
    if (!ctx_->tab_id.empty()) {
        CURL* curl = curl_easy_init();
        if (curl) {
            std::string url =
                "http://" + host_ + ":" + std::to_string(port_) + "/json/close/" + ctx_->tab_id;
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_perform(curl);
            curl_easy_cleanup(curl);
        }
    }
    if (ctx_->lws_ctx)
        lws_context_destroy(ctx_->lws_ctx);
}

std::string CDPClient::get_web_socket_url() {
    CURL*       curl = curl_easy_init();
    std::string readBuffer;
    if (curl) {
        std::string url = "http://" + host_ + ":" + std::to_string(port_) + "/json/new";
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }

    try {
        auto j       = nlohmann::json::parse(readBuffer);
        ctx_->tab_id = j["id"];
        return j["webSocketDebuggerUrl"];
    } catch (...) {
        return "";
    }
}

static int callback_cdp(struct lws*               wsi,
                        enum lws_callback_reasons reason,
                        void*                     user [[maybe_unused]],
                        void*                     in,
                        size_t                    len) {
    CDPClient::Context* ctx = (CDPClient::Context*)lws_get_opaque_user_data(wsi);
    if (!ctx)
        return 0;

    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            ctx->connected = true;
            break;
        case LWS_CALLBACK_CLIENT_RECEIVE:
            ctx->partial_data.append((char*)in, len);
            if (lws_is_final_fragment(wsi)) {
                ctx->message_queue.push_back(std::move(ctx->partial_data));
                ctx->partial_data.clear();
            }
            break;
        case LWS_CALLBACK_CLIENT_WRITEABLE:
            if (ctx->write_pending) {
                constexpr size_t kMaxPayload = std::numeric_limits<unsigned short>::max();
                auto             buf  = std::make_unique<unsigned char[]>(LWS_PRE + kMaxPayload);
                size_t           mlen = ctx->pending_message.length();
                if (mlen > kMaxPayload)
                    mlen = kMaxPayload;
                memcpy(&buf[LWS_PRE], ctx->pending_message.c_str(), mlen);
                lws_write(wsi, &buf[LWS_PRE], mlen, LWS_WRITE_TEXT);
                ctx->write_pending = false;
            }
            break;
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        case LWS_CALLBACK_CLIENT_CLOSED:
            ctx->connected = false;
            break;
        default:
            break;
    }
    return 0;
}

static struct lws_protocols protocols[] = {{"cdp", callback_cdp, 0, 1048576, 0, NULL, 0},
                                           {NULL, NULL, 0, 0, 0, NULL, 0}};

constexpr int kDefaultTimeout  = 10;
constexpr int kConnectTimeout  = 5;
constexpr int kPageLoadTimeout = 10;

bool CDPClient::connect() {
    std::string ws_url = get_web_socket_url();
    if (ws_url.empty())
        return false;

    struct lws_context_creation_info info;
    memset(&info, 0, sizeof info);
    info.port      = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.gid       = -1;
    info.uid       = -1;

    ctx_->lws_ctx = lws_create_context(&info);
    if (!ctx_->lws_ctx)
        return false;

    size_t path_start = ws_url.find("/", 5);
    if (path_start == std::string::npos)
        return false;
    std::string path = ws_url.substr(path_start);

    struct lws_client_connect_info i;
    memset(&i, 0, sizeof i);
    i.context = ctx_->lws_ctx;
    i.address = host_.c_str();
    i.port    = port_;
    i.path    = path.c_str();

    std::string host_header = host_ + ":" + std::to_string(port_);
    i.host                  = host_header.c_str();

    i.origin           = NULL;
    i.protocol         = NULL;
    i.opaque_user_data = ctx_.get();

    ctx_->wsi = lws_client_connect_via_info(&i);
    if (!ctx_->wsi)
        return false;

    auto start_conn = std::chrono::steady_clock::now();
    while (!ctx_->connected
           && std::chrono::steady_clock::now() - start_conn
                  < std::chrono::seconds(kConnectTimeout)) {
        lws_service(ctx_->lws_ctx, 50);
    }
    return ctx_->connected;
}

bool CDPClient::navigate(const std::string& url) {
    if (!ctx_->connected)
        return false;

    int            enable_id  = ctx_->current_id++;
    nlohmann::json enable_cmd = {{"id", enable_id}, {"method", "Page.enable"}};
    send_message(enable_cmd);

    int            nav_id = ctx_->current_id++;
    nlohmann::json nav = {{"id", nav_id}, {"method", "Page.navigate"}, {"params", {{"url", url}}}};
    send_message(nav);

    bool load_event_fired = false;
    auto start_nav        = std::chrono::steady_clock::now();

    while (std::chrono::steady_clock::now() - start_nav < std::chrono::seconds(kPageLoadTimeout)) {
        lws_service(ctx_->lws_ctx, 50);

        while (!ctx_->message_queue.empty()) {
            std::string msg = ctx_->message_queue.front();
            ctx_->message_queue.pop_front();

            try {
                auto j = nlohmann::json::parse(msg);
                if (j.value("method", "") == "Page.loadEventFired") {
                    load_event_fired = true;
                }

                if (j.value("id", -1) == nav_id && j.contains("error")) {
                    Logger::error("CDP: Navigation failed: " + j["error"].dump());
                    return false;
                }
            } catch (...) {
            }
        }

        if (load_event_fired)
            return true;
    }

    Logger::warn("CDP: Page load timed out: " + url);
    return false;
}

std::string CDPClient::evaluate(const std::string& expression) {
    if (!ctx_->connected)
        return "";

    int            eval_id = ctx_->current_id++;
    nlohmann::json eval    = {
        {"id", eval_id}, {"method", "Runtime.evaluate"}, {"params", {{"expression", expression}}}};
    send_message(eval);

    auto start_eval = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start_eval < std::chrono::seconds(kDefaultTimeout)) {
        lws_service(ctx_->lws_ctx, 50);

        while (!ctx_->message_queue.empty()) {
            std::string msg = ctx_->message_queue.front();
            ctx_->message_queue.pop_front();

            try {
                auto j = nlohmann::json::parse(msg);
                if (j.value("id", -1) != eval_id)
                    continue;

                if (j.contains("result") && j["result"].contains("result")
                    && j["result"]["result"].contains("value")) {
                    return j["result"]["result"]["value"];
                }
                return "";
            } catch (...) {
            }
        }
    }
    return "";
}

std::string CDPClient::render(const std::string& url) {
    if (!connect())
        return "";
    if (!navigate(url))
        return "";
    return evaluate("document.documentElement.outerHTML");
}

void CDPClient::send_message(const nlohmann::json& msg) {
    if (!ctx_->wsi)
        return;
    ctx_->pending_message = msg.dump();
    ctx_->write_pending   = true;
    lws_callback_on_writable(ctx_->wsi);

    auto start = std::chrono::steady_clock::now();
    while (ctx_->write_pending
           && std::chrono::steady_clock::now() - start < std::chrono::seconds(3)) {
        lws_service(ctx_->lws_ctx, 50);
    }
}

}  // namespace CDP
}  // namespace Browser
}  // namespace Mojo
