#pragma once

#include <boost/asio.hpp>
#include <string>

namespace Mojo::Network::Proxy {

/**
 * @brief Shared utility for SOCKS4 and SOCKS5 handshakes.
 *
 * This class provides static methods to perform asynchronous SOCKS handshakes
 * on a TCP socket.
 */
class SocksHandshake {
public:
    /**
     * @brief Performs a SOCKS4 handshake.
     * @param socket The connected socket to the proxy server.
     * @param host Target host (must be an IP address for SOCKS4).
     * @param port Target port.
     */
    static boost::asio::awaitable<void> perform_socks4(boost::asio::ip::tcp::socket& socket,
                                                       const std::string&            host,
                                                       const std::string&            port);

    /**
     * @brief Performs a SOCKS5 handshake (No Auth).
     * @param socket The connected socket to the proxy server.
     * @param host Target host (hostname or IP).
     * @param port Target port.
     */
    static boost::asio::awaitable<void> perform_socks5(boost::asio::ip::tcp::socket& socket,
                                                       const std::string&            host,
                                                       const std::string&            port);
};

}  // namespace Mojo::Network::Proxy
