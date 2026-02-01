#pragma once
#include <string>

namespace Mojo {
namespace Constants {

static constexpr int DEFAULT_THREADS = 4;
static constexpr int DEFAULT_DEPTH = 0;
static constexpr const char* DEFAULT_OUTPUT_DIR = "mojo_out";

static constexpr int MAX_RETRIES = 3;
static constexpr int REQUEST_TIMEOUT_SECONDS = 3;
static constexpr const char* USER_AGENT = "Mojo/1.0";

static constexpr size_t DEFAULT_BLOOM_FILTER_SIZE = 1000000;
static constexpr int DEFAULT_BLOOM_FILTER_HASHES = 7;

} // namespace Constants
} // namespace Mojo
