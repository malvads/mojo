#include "socks_handshake.hpp"
#include <stdexcept>
#include <vector>
#include "../../binary/reader.hpp"
#include "../../binary/writer.hpp"

namespace Mojo::Network::Proxy {

namespace net = boost::asio;
using namespace Mojo::Binary;

net::awaitable<void> SocksHandshake::perform_socks4(net::ip::tcp::socket& socket,
                                                    const std::string&    host,
                                                    const std::string&    port) {
    boost::system::error_code ec;
    net::ip::address          addr = net::ip::make_address(host, ec);
    if (ec) {
        throw std::runtime_error(
            "SOCKS4 requires IP address, SOCKS4a (hostname) not implemented yet");
    }

    std::vector<uint8_t> req_data;
    Writer               writer(req_data);
    writer.write_uint8(0x04);
    writer.write_uint8(0x01);

    uint16_t p = static_cast<uint16_t>(std::stoi(port));
    writer.write_uint16_be(p);

    auto bytes = addr.to_v4().to_bytes();
    for (auto b : bytes)
        writer.write_uint8(b);
    writer.write_uint8(0x00);

    co_await net::async_write(socket, net::buffer(req_data), net::use_awaitable);

    std::vector<uint8_t> resp_data(8);
    co_await             net::async_read(socket, net::buffer(resp_data), net::use_awaitable);

    Reader reader(resp_data);
    reader.read_uint8();
    if (reader.read_uint8() != 0x5A) {
        throw std::runtime_error("SOCKS4 handshake failed");
    }
    co_return;
}

net::awaitable<void> SocksHandshake::perform_socks5(net::ip::tcp::socket& socket,
                                                    const std::string&    host,
                                                    const std::string&    port) {
    std::vector<uint8_t> greeting;
    Writer               writer(greeting);
    writer.write_uint8(0x05);
    writer.write_uint8(0x01);
    writer.write_uint8(0x00);
    co_await net::async_write(socket, net::buffer(greeting), net::use_awaitable);

    std::vector<uint8_t> choice_data(2);
    co_await             net::async_read(socket, net::buffer(choice_data), net::use_awaitable);

    Reader reader(choice_data);
    if (reader.read_uint8() != 0x05 || reader.read_uint8() == 0xFF) {
        throw std::runtime_error("SOCKS5 handshake failed (auth choice)");
    }

    std::vector<uint8_t> req_data;
    Writer               req_writer(req_data);
    req_writer.write_uint8(0x05);
    req_writer.write_uint8(0x01);
    req_writer.write_uint8(0x00);
    req_writer.write_uint8(0x03);
    req_writer.write_uint8(static_cast<uint8_t>(host.size()));
    req_writer.write_string(host);
    req_writer.write_uint16_be(static_cast<uint16_t>(std::stoi(port)));

    co_await net::async_write(socket, net::buffer(req_data), net::use_awaitable);

    std::vector<uint8_t> header_data(4);
    co_await             net::async_read(socket, net::buffer(header_data), net::use_awaitable);

    Reader header_reader(header_data);
    header_reader.read_uint8();
    if (header_reader.read_uint8() != 0x00) {
        throw std::runtime_error("SOCKS5 connect failed");
    }
    header_reader.read_uint8();
    uint8_t atyp = header_reader.read_uint8();

    size_t len = 0;
    if (atyp == 0x01)
        len = 4;
    else if (atyp == 0x03) {
        uint8_t  domain_len;
        co_await net::async_read(socket, net::buffer(&domain_len, 1), net::use_awaitable);
        len = domain_len;
    }
    else if (atyp == 0x04)
        len = 16;

    if (len > 0) {
        std::vector<uint8_t> addr_port(len + 2);
        co_await             net::async_read(socket, net::buffer(addr_port), net::use_awaitable);
    }

    co_return;
}

}  // namespace Mojo::Network::Proxy
