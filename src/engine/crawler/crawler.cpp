#include "crawler.hpp"
#include "../../browser/browser_client.hpp"
#include "../../core/logger/logger.hpp"
#include "../../core/types/constants.hpp"
#include "../../network/http/beast_client.hpp"
#include "../../network/http/http_client.hpp"
#include "../../utils/text/converter.hpp"
#include "../../utils/url/url.hpp"

#include <boost/asio/co_spawn.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace Mojo {
namespace Engine {

using namespace Mojo::Core;
using namespace Mojo::Network::Http;
using namespace Mojo::Browser;

Crawler::Crawler(const CrawlerConfig& config)
    : max_depth_(config.max_depth),
      num_threads_(config.threads),
      num_virtual_threads_(config.virtual_threads > 0 ? config.virtual_threads
                                                      : Constants::DEFAULT_VIRTUAL_THREADS),
      num_worker_threads_(config.worker_threads > 0 ? config.worker_threads
                                                    : Constants::DEFAULT_WORKER_THREADS),
      output_dir_(config.output_dir),
      tree_structure_(config.tree_structure),
      use_proxies_(!config.proxies.empty()),
      proxy_bind_ip_(config.proxy_bind_ip),
      proxy_bind_port_(config.proxy_bind_port),
      cdp_port_(config.cdp_port),
      proxy_pool_(config.proxies, config.proxy_retries, config.proxy_priorities),
      worker_pool_(config.worker_threads > 0 ? config.worker_threads
                                             : Constants::DEFAULT_WORKER_THREADS),
      render_js_(config.render_js),
      browser_path_(config.browser_path),
      headless_(config.headless),
      proxy_connect_timeout_(config.proxy_connect_timeout),
      proxy_threads_(config.proxy_threads),
      user_agent_(config.user_agent) {
}

}  // namespace Engine
}  // namespace Mojo
