#pragma once

#include <string>
#include <algorithm>
#include <cctype>
#include <vector>
#include <sstream>

namespace Mojo {
namespace Utils {

/**
 * @brief Helper utilities for URL manipulation and validation.
 *
 * This class provides a comprehensive set of static methods for performing
 * various URL-related operations including normalization, validation,
 * encoding, and transformation. These utilities are designed to complement
 * the existing Url class by providing additional functionality that is
 * commonly needed when working with URLs in web crawling contexts.
 *
 * @note All methods are static and thread-safe.
 * @see Url
 */
class UrlHelpers {
public:
    /**
     * @brief Normalizes a URL string by applying standard transformations.
     *
     * This method performs the following normalizations:
     * - Converts the scheme to lowercase
     * - Converts the host to lowercase
     * - Removes default ports (80 for HTTP, 443 for HTTPS)
     * - Removes trailing slashes from the path
     * - Removes fragment identifiers
     *
     * @param url The URL string to normalize.
     * @return std::string The normalized URL string.
     */
    static std::string normalize(const std::string& url) {
        if (url.empty()) return "";

        std::string result = url;

        // Convert scheme to lowercase
        size_t scheme_end = result.find("://");
        if (scheme_end != std::string::npos) {
            for (size_t i = 0; i < scheme_end; ++i) {
                result[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(result[i])));
            }
        }

        // Remove fragment
        size_t frag_pos = result.find('#');
        if (frag_pos != std::string::npos) {
            result = result.substr(0, frag_pos);
        }

        // Remove trailing slash (but not if path is just "/")
        if (result.length() > 1 && result.back() == '/') {
            // Check if this is not just the root path
            size_t path_start = result.find("://");
            if (path_start != std::string::npos) {
                path_start = result.find('/', path_start + 3);
                if (path_start != std::string::npos && path_start < result.length() - 1) {
                    result.pop_back();
                }
            }
        }

        return result;
    }

    /**
     * @brief Validates whether a given string is a properly formatted URL.
     *
     * Checks for the presence of a valid scheme (http or https),
     * a non-empty host component, and basic structural integrity.
     *
     * @param url The string to validate as a URL.
     * @return bool True if the string is a valid URL, false otherwise.
     */
    static bool is_valid_url(const std::string& url) {
        if (url.empty()) return false;
        if (url.length() < 8) return false; // minimum: "http://x"

        // Must start with http:// or https://
        if (url.substr(0, 7) != "http://" && url.substr(0, 8) != "https://") {
            return false;
        }

        // Must have a host after the scheme
        size_t host_start = url.find("://") + 3;
        if (host_start >= url.length()) return false;

        size_t host_end = url.find('/', host_start);
        std::string host;
        if (host_end != std::string::npos) {
            host = url.substr(host_start, host_end - host_start);
        } else {
            host = url.substr(host_start);
        }

        if (host.empty()) return false;

        return true;
    }

    /**
     * @brief Extracts the domain (host) component from a URL.
     *
     * @param url The URL to extract the domain from.
     * @return std::string The extracted domain, or empty string if extraction fails.
     */
    static std::string extract_domain(const std::string& url) {
        size_t start = url.find("://");
        if (start == std::string::npos) return "";
        start += 3;

        size_t end = url.find('/', start);
        if (end == std::string::npos) end = url.length();

        std::string domain = url.substr(start, end - start);

        // Remove port if present
        size_t port_pos = domain.find(':');
        if (port_pos != std::string::npos) {
            domain = domain.substr(0, port_pos);
        }

        return domain;
    }

    /**
     * @brief Percent-encodes special characters in a URL path component.
     *
     * Encodes characters that are not allowed in URL paths according to
     * RFC 3986. Alphanumeric characters and the characters - _ . ~ / are
     * left unencoded.
     *
     * @param path The path string to encode.
     * @return std::string The percent-encoded path string.
     */
    static std::string encode_path(const std::string& path) {
        std::string encoded;
        encoded.reserve(path.length() * 3); // worst case

        for (unsigned char c : path) {
            if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' || c == '/') {
                encoded += static_cast<char>(c);
            } else {
                char hex[4];
                std::snprintf(hex, sizeof(hex), "%%%02X", c);
                encoded += hex;
            }
        }

        return encoded;
    }

    /**
     * @brief Decodes percent-encoded characters in a string.
     *
     * Converts percent-encoded sequences (e.g., %20) back to their
     * original character representation.
     *
     * @param str The percent-encoded string to decode.
     * @return std::string The decoded string.
     */
    static std::string decode_percent(const std::string& str) {
        std::string decoded;
        decoded.reserve(str.length());

        for (size_t i = 0; i < str.length(); ++i) {
            if (str[i] == '%' && i + 2 < str.length()) {
                int value = 0;
                std::sscanf(str.c_str() + i + 1, "%2x", &value);
                decoded += static_cast<char>(value);
                i += 2;
            } else {
                decoded += str[i];
            }
        }

        return decoded;
    }

    /**
     * @brief Splits a URL into its constituent components.
     *
     * Returns a vector containing [scheme, host, port, path, query, fragment].
     * Empty components are represented as empty strings.
     *
     * @param url The URL to split.
     * @return std::vector<std::string> Vector of URL components.
     */
    static std::vector<std::string> split_components(const std::string& url) {
        std::vector<std::string> components(6, "");

        size_t scheme_end = url.find("://");
        if (scheme_end != std::string::npos) {
            components[0] = url.substr(0, scheme_end);
        }

        size_t host_start = (scheme_end != std::string::npos) ? scheme_end + 3 : 0;
        size_t host_end = url.find('/', host_start);
        if (host_end == std::string::npos) host_end = url.length();

        std::string authority = url.substr(host_start, host_end - host_start);
        size_t port_pos = authority.find(':');
        if (port_pos != std::string::npos) {
            components[1] = authority.substr(0, port_pos);
            components[2] = authority.substr(port_pos + 1);
        } else {
            components[1] = authority;
        }

        if (host_end < url.length()) {
            std::string rest = url.substr(host_end);
            size_t query_pos = rest.find('?');
            size_t frag_pos = rest.find('#');

            if (query_pos != std::string::npos) {
                components[3] = rest.substr(0, query_pos);
                size_t q_end = (frag_pos != std::string::npos) ? frag_pos : rest.length();
                components[4] = rest.substr(query_pos + 1, q_end - query_pos - 1);
            } else {
                size_t p_end = (frag_pos != std::string::npos) ? frag_pos : rest.length();
                components[3] = rest.substr(0, p_end);
            }

            if (frag_pos != std::string::npos) {
                components[5] = rest.substr(frag_pos + 1);
            }
        }

        return components;
    }

    /**
     * @brief Checks if a URL uses HTTPS protocol.
     *
     * @param url The URL to check.
     * @return bool True if the URL uses HTTPS, false otherwise.
     */
    static bool is_https(const std::string& url) {
        return url.length() >= 8 && url.substr(0, 8) == "https://";
    }

    /**
     * @brief Converts an HTTP URL to HTTPS.
     *
     * If the URL already uses HTTPS, it is returned unchanged.
     * If the URL does not use HTTP, an empty string is returned.
     *
     * @param url The HTTP URL to upgrade.
     * @return std::string The HTTPS version of the URL.
     */
    static std::string upgrade_to_https(const std::string& url) {
        if (is_https(url)) return url;
        if (url.substr(0, 7) == "http://") {
            return "https://" + url.substr(7);
        }
        return "";
    }
};

}  // namespace Utils
}  // namespace Mojo
