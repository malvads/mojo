#include <iostream>
#include "../../../core/logger/logger.hpp"
#include "../crawler.hpp"

namespace Mojo {
namespace Engine {

Crawler::~Crawler() {
    shutdown();
}

void Crawler::start(const std::string& start_url) {
    done_ = false;

    Logger::info("Crawler: Starting for " + start_url);
    auto parsed   = Mojo::Utils::Url::parse(start_url);
    start_domain_ = parsed.host;
    Logger::info("Crawler: Domain set to " + start_domain_);

    add_url(start_url, 0);
    Logger::info("Crawler: Start URL added");

    init_io_services();
    init_signals();
    init_proxies();
    init_browser();
    init_storage();
    spawn_workers();
    Logger::info("Crawler: Workers spawned, awaiting completion...");
    await_completion();
    shutdown();
}

void Crawler::init_storage() {
    storage_ = std::make_unique<Mojo::Storage::DiskStorage>(output_dir_);
}

void Crawler::init_io_services() {
    if (ioc_.stopped())
        ioc_.restart();
    work_guard_ =
        std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(
            ioc_.get_executor());
    for (int i = 0; i < num_threads_; ++i) {
        io_threads_.emplace_back([this]() {
            try {
                ioc_.run();
            } catch (const std::exception& e) {
                Logger::error("IO Thread Exception: " + std::string(e.what()));
            } catch (...) {
                Logger::error("IO Thread Unknown Exception");
            }
        });
    }
    Logger::info("Started " + std::to_string(num_threads_) + " IO threads.");
    Logger::info("Concurrency: " + std::to_string(num_virtual_threads_) + " Virtual, "
                 + std::to_string(num_worker_threads_) + " Worker.");
}

void Crawler::await_completion() {
    std::unique_lock<std::mutex> lock(done_mutex_);
    done_cv_.wait(lock, [this] { return done_.load(); });
}

void Crawler::trigger_done() {
    {
        std::lock_guard<std::mutex> lock(done_mutex_);
        done_ = true;
    }
    done_cv_.notify_all();
}

void Crawler::init_signals() {
    signals_.clear();
    signals_.add(SIGINT);
    signals_.add(SIGTERM);
    signals_.async_wait([this](const boost::system::error_code& error, int signal_number) {
        if (!error) {
            Logger::info("Signal " + std::to_string(signal_number)
                         + " received. Triggering stop...");
            trigger_done();
        }
    });
}

void Crawler::init_proxies() {
    if (proxy_pool_.empty())
        return;

    Logger::info("Initialized Proxy Pool.");
    if (!render_js_)
        return;

    proxy_server_ = std::make_unique<ProxyServer>(
        proxy_pool_, proxy_bind_ip_, proxy_bind_port_, proxy_threads_);
    proxy_server_->start();
    Logger::info("Local Proxy Gateway on port " + std::to_string(proxy_server_->get_port()));
}

void Crawler::init_browser() {
    if (!render_js_)
        return;

    std::string p_url;
    if (proxy_server_) {
        p_url = proxy_bind_ip_ + ":" + std::to_string(proxy_server_->get_port());
    }

    std::string path = browser_path_;
    if (path.empty())
        path = BrowserLauncher::find_browser();

    if (path.empty()) {
        Logger::error("No suitable browser found. Specify --browser.");
        return;
    }

    if (!BrowserLauncher::launch(path, cdp_port_, headless_, p_url)) {
        Logger::error("Failed to launch browser.");
    }
}

void Crawler::spawn_workers() {
    for (int i = 0; i < num_virtual_threads_; ++i) {
        boost::asio::co_spawn(ioc_, worker_loop(), boost::asio::detached);
    }
}

void Crawler::shutdown() {
    if (is_shutdown_.exchange(true))
        return;

    done_ = true;
    Logger::info("Shutting down resources...");

    work_guard_.reset();
    ioc_.stop();

    if (proxy_server_)
        proxy_server_->stop();

    for (auto& t : io_threads_) {
        if (t.get_id() == std::this_thread::get_id())
            continue;
        if (t.joinable())
            t.join();
    }
    io_threads_.clear();

    worker_pool_.stop();
    worker_pool_.join();

    if (render_js_)
        BrowserLauncher::cleanup();
    Logger::success("Shutdown complete.");
}

}  // namespace Engine
}  // namespace Mojo
