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

Mojo is a high-performance, multithreaded web crawler tailored for creating high-quality datasets for Large Language Models (LLMs) and AI training. Written in C++17, it rapidly fetches entire websites and converts them into clean, structured Markdown, making it the ideal tool for building knowledge bases and RAG (Retrieval-Augmented Generation) pipelines.

## Download

You can download the latest pre-compiled binaries for Windows, macOS, and Linux from the [Releases](https://github.com/malvads/mojo/releases) page.

## Key Features

- **High Performance**: Built with C++17 and `libcurl`, Mojo utilizes a thread-pool architecture to maximize I/O throughput, significantly outperforming Python-based crawlers in high-volume tasks due to C++ native performance.
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
    A[Start] --> B["🔴 Launch Browser <br/> w/ Proxy A"]
    B --> C[Visit Page 1]
    C --> D[🔴 Kill Browser]
    D --> E["🔴 Launch Browser <br/> w/ Proxy B"]
    E --> F[Visit Page 2]
    end
    
    subgraph "Mojo (Magic Gateway)"
    H[Start] --> I["🟢 Launch Browser Once <br/> (Proxy = Mojo Localhost)"]
    I --> J[Visit Page 1]
    J -- "Traffic" --> K{Mojo Gateway}
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

## Advanced Proxy Usage

Mojo supports sophisticated proxy configurations to ensure continuous crawling without being blocked.

### 1. Using CLI Arguments
Single proxy:
```bash
./mojo -p socks5://127.0.0.1:9050 https://example.com
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

- **Concurrency**: Proxies are shared across all worker threads (non-exclusive), allowing massive parallel throughput without "exhausting" the pool.
- **Selection**: A linear scan selects the best available proxy based on Priority (SOCKS5 > SOCKS4 > HTTP) and Health (Failures).
- **Auto-Pruning**: Proxies that exceed the retry limit are automatically removed from the rotation.

**Priorities:**
- **SOCKS5 (Priority 2)**: Highest priority. Faster and more anonymous.
- **SOCKS4 (Priority 1)**: Medium priority.
- **HTTP/HTTPS (Priority 0)**: Lowest priority.

## Build Instructions

### Prerequisites
- C++17 Compiler (GCC/Clang/MSVC)
- **libcurl** (Network)
- **libgumbo** (HTML Parsing)
- **libwebsockets** (WebSocket Communication)
- **yaml-cpp** (YAML Parsing)
- **Google Chrome** or **Chromium** (Runtime dependency for JS rendering)

### Linux (Ubuntu/Debian)
```bash
sudo apt-get install build-essential libcurl4-openssl-dev libgumbo-dev libwebsockets-dev libyaml-cpp-dev chromium
make
```

### macOS
```bash
brew install curl gumbo-parser libwebsockets yaml-cpp
brew install --cask google-chrome
make
```

### Windows / Cross-Platform (CMake)
```bash
mkdir build && cd build
cmake ..
cmake --build .
```

