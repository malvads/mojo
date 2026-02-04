<div align="center">
  <img src="assets/mojo.png" alt="Mojo Logo" width="200"/>
  <h1>Mojo</h1>
  
  <p>
    <b>Extremely Fast Web Crawler for AI & LLM Data Ingestion</b>
  </p>

  <a href="https://github.com/malvads/mojo/actions">
    <img src="https://img.shields.io/github/actions/workflow/status/malvads/mojo/ci.yml?style=for-the-badge&logo=github&label=Build" alt="Build Status">
  </a>
  <a href="https://github.com/malvads/mojo/blob/main/LICENSE">
    <img src="https://img.shields.io/badge/License-MIT-blue?style=for-the-badge&logo=mit" alt="License">
  </a>
  <a href="https://github.com/malvads/mojo/stargazers">
    <img src="https://img.shields.io/github/stars/malvads/mojo?style=for-the-badge&color=yellow" alt="Stars">
  </a>
  <a href="https://github.com/malvads/mojo/issues">
    <img src="https://img.shields.io/github/issues/malvads/mojo?style=for-the-badge&color=orange" alt="Issues">
  </a>
</div>

Mojo is a high-performance, multithreaded web crawler tailored for creating high-quality datasets for Large Language Models (LLMs) and AI training. Written in modern C++20 with coroutines, it rapidly fetches entire websites and converts them into clean, structured Markdown, making it the ideal tool for building knowledge bases and RAG (Retrieval-Augmented Generation) pipelines.

## Installation

You can download the latest pre-compiled binaries from the [Releases](https://github.com/malvads/mojo/releases) page.

### Linux (Binary Packages)

For maximum compatibility, we recommend using the official packages which automatically handle dependencies:

**Debian / Ubuntu / Kali / Mint:**
```bash
sudo apt update
sudo apt install ./mojo-0.1.0-debian.deb
```

**CentOS / RHEL / Fedora:**
```bash
sudo yum install epel-release 
sudo yum install ./mojo-0.1.0-rhel.rpm
```

### macOS
1. Download `mojo-macos-arm` (M1/M2/M3) or `mojo-macos-intel`.
2. Move it to your bin folder and give it execution permissions:
```bash
chmod +x mojo-macos-arm
sudo mv mojo-macos-arm /usr/local/bin/mojo
```
3. Make sure to grant privileges to the binary via security settings, since it is not signed.

### Windows
1. Download `mojo-windows-x64.exe`.
2. Run it from your terminal (CMD/Powershell).

## Key Features

- **High Performance**: Built with C++20 coroutines, Boost.Beast, and Boost.Asio, Mojo utilizes a thread-pool architecture with async I/O to maximize throughput, significantly outperforming Python-based crawlers in high-volume tasks due to C++ native performance.
- **RAG-Ready Data Ingestion**: Automatically transforms noisy HTML into clean, token-efficient Markdown. Perfect for populating vector databases (Pinecone, Milvus, Weaviate) or providing context for LLMs (NotebookLM, Claude, Qwen, etc).
- **Proxies**:
  - **Protocol Support**: Rotates between SOCKS4, SOCKS5, and HTTP proxies.
  - **Auto Pruning**: Automatically detects and prunes dead or rate-limited proxies (403/429 errors) from the pool.
  - **Priority Selection**: Automatically prioritizes SOCKS5 proxies for improved performance.
- **JavaScript Rendering (slower)**:
  - **Full Browser Simulation**: Uses a headless Chromium instance to execute JavaScript and render dynamic content (SPAs, React, Vue, etc.).
  - **Magic Proxy Rotation**: Bypasses Chromium's static proxy limitation using an internal **Reverse Proxy Gateway**. This allows the browser to rotate IPs per-request **without the heavy overhead of restarting the browser process**. This makes it **orders of magnitude faster** than traditional scrapers (Selenium/Puppeteer) which force a full browser reboot (~1-2s overhead) to switch proxies.
  - **Stealth Mode**: Leverages native Chromium with minimal flags for maximum invisibility.
  - **Performance**: While slower than raw HTTP crawling, it ensures 100% fidelity for dynamic sites.

### Architecture Comparison (--render flag)

```mermaid
graph TD
    subgraph "Typical (Selenium/Puppeteer)"
    A[Start] --> B["Launch Browser <br/> w/ Proxy A"]
    B --> C[Visit Page 1]
    C --> D[Kill Browser]
    D --> E["Launch Browser <br/> w/ Proxy B"]
    E --> F[Visit Page 2]
    end
    
    subgraph "Mojo (Magic Gateway)"
    H[Start] --> I["Launch Browser Once <br/> (Proxy = Mojo Localhost)"]
    I --> J[Visit Page 1]
    J -- "Traffic" --> K{"Mojo Gateway (Proxy Pool Rotation)"}
    K -- "Auto-Rotate" --> L[External Proxy A]
    I --> M[Visit Page 2]
    M -- "Traffic" --> K
    K -- "Auto-Rotate" --> N[External Proxy B]
    end

    %% Force subgraphs to be one below the other
    F ~~~ H
```

**Why is this better?**
1.  **Zero Restart Overhead**: Traditional tools must kill and restart the entire Chrome process (1-2s delay) just to change an IP. Mojo keeps the browser open and rotates the connection internally.
2.  **Microsecond Switching**: Mojo switches the upstream proxy at the TCP socket layer instantly for every request.
3.  **Lower CPU Usage**: Avoiding constant browser reboots saves massive amounts of CPU, allowing you to run more concurrent workers.

### Threading Model for --render flag

Mojo operates with two distinct types of threads to ensure maximum throughput:

| Thread Type | Configuration | Responsibility |
| :--- | :--- | :--- |
| **Scraping Workers** | `-t`, `--threads` | **The Decision Makers**: Managing the URL queue, visiting pages, extracting links, and saving results. Scaling this visits more pages simultaneously. |
| **Gateway Workers** | `--proxy-threads` | **The Couriers**: Handling the high-volume background traffic (JS, CSS, images) requested by the browser. Scaling this ensures the browser never stalls. |

**The Hierarchy:**
If you set `-t 8`, Mojo visits 8 pages simultaneously. However, a single web page can trigger 50+ network requests. The **Gateway Workers** ensure those 50+ requests flow smoothly through your proxy rotation without bottlenecking the main scraping agents.

## Video Example

Check out Mojo in action:

[![Mojo Demo](https://img.youtube.com/vi/Ue4Rcsa-4hA/0.jpg)](https://www.youtube.com/watch?v=Ue4Rcsa-4hA)

## Usage Examples

### Basic Crawl
Crawl a documentation site to depth 2 and save it as structured Markdown.
```bash
./mojo -d 2 https://docs.example.com
```

### JavaScript Crawl
Render dynamic content using a headless browser.
> **Note**: This mode is slower than standard crawling as it launches a full Chromium instance to execute JavaScript. Use this for SPAs (Single Page Applications) or sites that require JS to display content.
```bash
./mojo --render https://spa-example.com
```

### Dataset Preparation (Flat Output)
Crawl a blog and save all articles into a single directory for easy embedding.
```bash
./mojo -d 3 -o ./dataset_raw --flat https://techblog.example.com
```

## Blocking Mojo

Mojo respects the [Robots Exclusion Protocol](https://developers.google.com/search/docs/crawling-indexing/robots/intro). To block Mojo from crawling your site, add the following to your `robots.txt`:

```robots.txt
User-agent: Mojo-Crawler/1.0
Disallow: /
```

Or to block all crawlers:

```robots.txt
User-agent: *
Disallow: /
```

## Advanced Proxy Usage

> **Notice:** Always scrape responsibly. Use proxies properly, follow `robots.txt`, respect rate limits, and comply with site terms. If not, Mojo is not the correct fit for you.

Many websites implement IP bans, or geo-restrictions to prevent automated access. By using proxies, Mojo can distribute requests across multiple IP addresses, reducing the risk of blocks and ensuring more reliable crawling.  
**Important:** This feature is intended to **help you scrape responsibly**, not to bypass site rules. Always follow `robots.txt`, respect rate limits, and comply with each site's terms of service.

### 1. Using CLI Arguments
Single proxy with custom gateway threads:
```bash
./mojo -p socks5://127.0.0.1:9050 --proxy-threads 64 https://example.com
```

Proxy List file:
```bash
./mojo --proxy-list proxies.txt https://target-site.com
```

### 2. Using Configuration File (`example_config.yaml`)
You can define all settings in a YAML file for cleaner usage.

Run with config:
```bash
./mojo --config example_config.yaml https://example.com
```

Example `proxies.txt` format:
```text
socks5://user:pass@10.0.0.1:1080
http://192.168.1.50:8080
socks4://172.16.0.10:1080
```

### How mojo uses proxies?

Inside the engine, Mojo manages proxies using a **Priority Selection Vector**, which favors specific protocols while ensuring high concurrency without resource locking:

- **Concurrency**: Proxies are shared across all worker threads. The **Proxy Gateway** uses a configurable **Thread Pool** (`--proxy-threads`) to handle multiple simultaneous requests from the browser efficiently.
- **Selection**: A **Round-Robin** strategy is used within each priority level to distribute load evenly across healthy proxies.
- **Auto-Pruning**: Proxies that exceed the retry limit are automatically removed from the rotation.

**Priorities:**
- **SOCKS5 (Priority 2)**: Highest priority. Faster and more anonymous.
- **SOCKS4 (Priority 1)**: Medium priority.
- **HTTP/HTTPS (Priority 0)**: Lowest priority.

## Build & Packaging Instructions

### Prerequisites
- C++20 Compiler (GCC 12+, Clang 14+, or MSVC 2022+)
- CMake 3.10+
- **Boost** (Asio, Beast, System)
- **libgumbo** (HTML Parsing)
- **libwebsockets** (WebSocket Communication)
- **yaml-cpp** (YAML Parsing)
- **CLI11** (Command Line Parser)
- **nlohmann_json** (JSON Parsing)
- **Google Chrome** is required at runtime for JS rendering.
- **Abseil** (Google Common Libraries)

---

### Linux (Debian / Ubuntu)

**1. Install Dependencies:**
```bash
sudo apt update
sudo apt install build-essential cmake git libcurl4-openssl-dev libgumbo-dev libwebsockets-dev libyaml-cpp-dev libcli11-dev nlohmann-json3-dev libcap-dev libuv1-dev libev-dev zlib1g-dev libabsl-dev
```

**2. Build & Package (DEB):**
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DMOJO_STATIC_BUILD=ON
make -j$(nproc)

# Create .deb package
cpack -G DEB
```
*Output: `mojo-0.1.0-Linux.deb`*

**3. Install Package:**
```bash
sudo dpkg -i mojo-*.deb
```

---

### Linux (Fedora / RHEL / CentOS)

**1. Install Dependencies:**
```bash
sudo dnf install git cmake make gcc-c++ libcurl-devel gumbo-parser-devel libwebsockets-devel nlohmann-json-devel yaml-cpp-devel cli11-devel libcap-devel libuv-devel libev-devel zlib-devel abseil-cpp-devel rpm-build
```

**2. Build & Package (RPM):**
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DMOJO_STATIC_BUILD=ON
make -j$(nproc)

# Create .rpm package
cpack -G RPM
```
*Output: `mojo-0.1.0-Linux.rpm`*

**3. Install Package:**
```bash
sudo rpm -i mojo-*.rpm
```

---

### macOS (Intel & Apple Silicon)

**1. Install Dependencies (Homebrew):**
```bash
brew install cmake curl gumbo-parser libwebsockets nlohmann-json yaml-cpp cli11 abseil
```

**2. Build:**
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)
```

**3. Package & Install:**
```bash
# To verify functionality
./mojo --help

# To install system-wide
sudo cp mojo /usr/local/bin/

# To create a distributable zip
zip mojo-macos.zip mojo
```

---

### Windows (Powershell)

**1. Install Dependencies (vcpkg):**
Assuming you have [vcpkg](https://github.com/microsoft/vcpkg) installed at `C:\vcpkg`:
```powershell
vcpkg install curl gumbo nlohmann-json libwebsockets yaml-cpp cli11 libuv zlib abseil --triplet x64-windows-static
vcpkg integrate install
```

**2. Build:**
```powershell
mkdir build; cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DVCPKG_TARGET_TRIPLET=x64-windows-static -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build . --config Release
```

**3. Package & Install:**
The executable is located at `Release\mojo.exe`.
- **Install**: Add the `Release` folder to your system PATH.
- **Package**: Right-click `mojo.exe` -> Send to -> Compressed (zipped) folder.


## License

Mojo is released under the **MIT License**.

This project incorporates the `google/robotstxt` library, which is licensed under the **Apache License 2.0**.
See [src/utils/robotstxt/LICENSE](src/utils/robotstxt/LICENSE) for more details.
