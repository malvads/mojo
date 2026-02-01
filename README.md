<div align="center">
  <img src="assets/mojo.png" alt="Mojo Logo" width="200"/>
  <h1>Mojo</h1>
  
  <p>
    <b>The Lightning-Fast Web Crawler for AI & LLM Data Ingestion</b>
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

Mojo is a high-performance, multithreaded web crawler tailored for creating high-quality datasets for Large Language Models (LLMs) and AI training. Written in C++17, it rapidly fetches entire websites and converts them into clean, structured Markdown, making it the ideal tool for building knowledge bases, RAG (Retrieval-Augmented Generation) pipelines, and offline documentation.

## Download

You can download the latest pre-compiled binaries for Windows, macOS, and Linux from the [Releases](https://github.com/malvads/mojo/releases) page.

## Key Features

- **Extreme Performance**: Built with C++17 and libcurl, Mojo utilizes multithreading to crawl pages at maximum speed, significantly outperforming Python-based alternatives.
- **AI-Ready Output**: Automatically converts complex HTML into clean, token-efficient Markdown using `html2md`. This format is optimized for vector databases and LLM context windows.
- **Data Integrity**: Intelligent proxy rotation and retry mechanisms ensure robust data collection even from difficult sources, protecting your scraping clusters from bans.
- **Smart Structure**: 
  - **Tree Mode**: Preserves the original website hierarchy for context-aware RAG applications.
  - **Flat Mode**: Flattens structure for simple bulk ingestion pipelines.
- **Noise Reduction**: Filters out non-content URLs (like `javascript:`) and handles redirects to ensure only unique, valid content is indexed.

## Usage Examples

### Basic Crawl
Crawl a documentation site to depth 2 and save it as structured Markdown.
```bash
./mojo -d 2 https://docs.example.com
```

### Dataset Preparation (Flat Output)
Crawl a blog and save all articles into a single directory for easy concatenation or embedding.
```bash
./mojo -d 3 -o ./dataset_raw --flat https://techblog.example.com
```

### Stealth Crawling (Proxy List)
Use a list of proxies to crawl aggressively without being blocked.
```bash
./mojo -d 5 -t 16 --proxy-list rotating_proxies.txt https://example.com
```

## Build Instructions

### Prerequisites
- C++17 Compiler (GCC/Clang/MSVC)
- **libcurl** (Network)
- **libgumbo** (HTML Parsing)

### Linux (Ubuntu/Debian)
```bash
sudo apt-get install build-essential libcurl4-openssl-dev libgumbo-dev
make
```

### macOS
```bash
brew install curl gumbo-parser
make
```

### Windows / Cross-Platform (CMake)
```bash
mkdir build && cd build
cmake ..
cmake --build .
```
