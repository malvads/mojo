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
  - **Auto Pruning**: Automatically detects and prunes dead or rate-limited proxies (403/429 errors) from the pool.
  - **Priority Selection**: Automatically prioritizes SOCKS5 proxies for improved performance.
- **JavaScript Rendering (slower)**:
  - **Full Browser Simulation**: Uses a headless Chromium instance to execute JavaScript and render dynamic content (SPAs, React, Vue, etc.).
  - **Stealth Mode**: Leverages native Chromium with minimal flags for maximum invisibility.
  - **Performance**: While slower than raw HTTP crawling, it ensures 100% fidelity for dynamic sites.

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

### Advanced Proxy Usage

Mojo supports sophisticated proxy configurations to ensure continuous crawling without being blocked.

**1. Using a single SOCKS5 proxy:**
```bash
./mojo -p socks5://127.0.0.1:9050 https://example.com
```

**2. Using a proxy list for rotation:**
Create a `proxies.txt` file with one proxy per line:
```text
socks5://user:pass@10.0.0.1:1080
http://192.168.1.50:8080
socks4://172.16.0.10:1080
```
Then run Mojo with the list:
```bash
./mojo --proxy-list proxies.txt https://target-site.com
```

### How mojo uses proxies?

Inside the engine, Mojo manages proxies using a **Priority Queue**, ensuring the highest quality connections are used first:

- **SOCKS5 (Priority 2)**: Highest priority. It handles all types of traffic (UDP/TCP), is generally faster and more anonymous than other protocols.
- **SOCKS4 (Priority 1)**: Medium priority. Similar to SOCKS5 but lacks authentication and UDP support.
- **HTTP/HTTPS (Priority 0)**: Lowest priority. These add more overhead due to header processing and are more likely to be detected by anti-bot systems.

*Mojo will automatically rotate through these, prioritizing SOCKS5 and removing any that fail or return "Forbidden/Too Many Requests" (403/429) errors from the active pool.*

## Build Instructions

### Prerequisites
- C++17 Compiler (GCC/Clang/MSVC)
- **libcurl** (Network)
- **libcurl** (Network)
- **libgumbo** (HTML Parsing)
- **libwebsockets** (WebSocket Communication)
- **Google Chrome** or **Chromium** (Runtime dependency for JS rendering)

### Linux (Ubuntu/Debian)
```bash
sudo apt-get install build-essential libcurl4-openssl-dev libgumbo-dev libwebsockets-dev chromium
make
```

### macOS
```bash
brew install curl gumbo-parser libwebsockets
brew install --cask google-chrome
make
```

### Windows / Cross-Platform (CMake)
```bash
mkdir build && cd build
cmake ..
cmake --build .
```

