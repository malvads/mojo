#pragma once

namespace Mojo {

enum class Status {
    Ok = 200,
    Error = 500,
    Timeout = 504,
    NetworkError = 503,
    ProxyError = 502,
    BrowserError = 501,
    UnknownError = 500
};

enum MaxCode {
    ClientError = 400,
    ServerError = 500
};

} // namespace Mojo