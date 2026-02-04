#pragma once
#include <chrono>
#include <map>
#include <string>
#include <vector>

namespace Mojo {
namespace Core {

struct Constants {
    static constexpr int         DEFAULT_THREADS         = 2;   // IO Threads
    static constexpr int         DEFAULT_VIRTUAL_THREADS = 16;  // Coroutines
    static constexpr int         DEFAULT_WORKER_THREADS  = 4;   // CPU/Disk Threads
    static constexpr int         DEFAULT_DEPTH           = 2;
    static constexpr const char* DEFAULT_OUTPUT_DIR      = "output";
    static constexpr const char* VERSION                 = "0.1.0";

    static constexpr int         MAX_RETRIES             = 3;
    static constexpr int         REQUEST_TIMEOUT_SECONDS = 10;
    static constexpr const char* USER_AGENT              = "Mojo-Crawler/1.0";

    static constexpr size_t DEFAULT_BLOOM_FILTER_SIZE   = 1000000;
    static constexpr int    DEFAULT_BLOOM_FILTER_HASHES = 7;
    static constexpr int    DEFAULT_PROXY_RETRIES       = 3;
};

inline const std::map<std::string, std::string>& get_mime_map() {
    static const std::map<std::string, std::string> map = {
        {"application/pdf", ".pdf"},
        {"application/msword", ".doc"},
        {"application/vnd.openxmlformats-officedocument.wordprocessingml.document", ".docx"},
        {"application/vnd.ms-excel", ".xls"},
        {"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet", ".xlsx"},
        {"application/vnd.ms-powerpoint", ".ppt"},
        {"application/vnd.openxmlformats-officedocument.presentationml.presentation", ".pptx"},
        {"text/csv", ".csv"},
        {"application/zip", ".zip"},
        {"application/x-tar", ".tar"},
        {"application/gzip", ".gz"},
        {"application/json", ".json"},
        {"application/xml", ".xml"},
        {"text/xml", ".xml"},
        {"image/jpeg", ".jpg"},
        {"image/png", ".png"},
        {"image/gif", ".gif"},
        {"image/webp", ".webp"},
        {"image/svg+xml", ".svg"},
        {"image/x-icon", ".ico"}};
    return map;
}

inline const std::vector<std::string>& get_image_extensions() {
    static const std::vector<std::string> extensions = {
        ".jpg", ".jpeg", ".png", ".gif", ".bmp", ".webp", ".svg", ".ico", ".tiff", ".avif"};
    return extensions;
}

inline const std::vector<std::string>& get_file_extensions() {
    static const std::vector<std::string> extensions = {".pdf",
                                                        ".doc",
                                                        ".docx",
                                                        ".xls",
                                                        ".xlsx",
                                                        ".ppt",
                                                        ".pptx",
                                                        ".csv",
                                                        ".zip",
                                                        ".tar",
                                                        ".gz",
                                                        ".json",
                                                        ".xml"};
    return extensions;
}

inline std::string get_matching_extension(const std::string&              url,
                                          const std::vector<std::string>& extensions) {
    if (url.empty())
        return "";

    std::string url_lower = url;
    for (char& c : url_lower)
        c = std::tolower(c);

    for (const auto& ext : extensions) {
        if (url_lower.size() >= ext.size()
            && url_lower.compare(url_lower.size() - ext.size(), ext.size(), ext) == 0) {
            return ext;
        }
    }
    return "";
}

inline bool has_extension(const std::string& url, const std::vector<std::string>& extensions) {
    return !get_matching_extension(url, extensions).empty();
}

inline std::string get_file_extension(const std::string& content_type, const std::string& url) {
    const auto& mime_map = get_mime_map();

    for (const auto& [mime, extension] : mime_map) {
        if (content_type.find(mime) != std::string::npos) {
            return extension;
        }
    }

    std::string ext = get_matching_extension(url, get_file_extensions());
    if (ext.empty()) {
        ext = get_matching_extension(url, get_image_extensions());
    }
    return ext;
}

inline bool is_downloadable_mime(const std::string& content_type) {
    if (content_type.empty())
        return false;
    const auto& mime_map = get_mime_map();
    for (const auto& [mime, extension] : mime_map) {
        if (content_type.find(mime) != std::string::npos) {
            return true;
        }
    }
    return false;
}

inline std::chrono::milliseconds get_backoff_time(int attempt) {
    if (attempt <= 0)
        return std::chrono::milliseconds(0);
    return std::chrono::milliseconds(1000 * (1 << (attempt - 1)));
}

}  // namespace Core
}  // namespace Mojo
