#include <boost/asio/post.hpp>
#include "../../../core/logger/logger.hpp"
#include "../../../utils/url/url.hpp"
#include "../crawler.hpp"

namespace Mojo {
namespace Engine {

std::string Crawler::get_save_filename(const std::string& url, const std::string& extension) {
    std::string filename = tree_structure_ ? Mojo::Utils::Url::to_filename(url)
                                           : Mojo::Utils::Url::to_flat_filename(url);
    if (!extension.empty()) {
        if (filename.size() > 3 && filename.substr(filename.size() - 3) == ".md") {
            filename.resize(filename.size() - 3);
        }
        filename += extension;
    }
    return filename;
}

void Crawler::save_to_storage(const std::string& filename,
                              const std::string& content,
                              bool               is_binary) {
    if (storage_) {
        storage_->save(filename, content, is_binary);
    }
}

void Crawler::handle_binary_content(const std::string& url,
                                    const std::string& content,
                                    const std::string& ext) {
    boost::asio::post(worker_pool_, [this, url, content, ext]() {
        save_to_storage(get_save_filename(url, ext), content, true);
    });
}

}  // namespace Engine
}  // namespace Mojo
