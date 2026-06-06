# Utils Module

## Overview

The utils module provides a collection of utility classes and functions that are used throughout the Mojo project. These utilities are designed to be reusable, efficient, and well-documented.

## Components

### URL Utilities

The URL utilities provide comprehensive URL parsing, validation, normalization, and manipulation capabilities. The `Url` class handles basic parsing and resolution, while the `UrlHelpers` class provides additional functionality for common URL operations.

### Crypto Utilities

The crypto utilities include implementations of the MurmurHash3 algorithm and a Bloom filter data structure for efficient set membership testing.

### HTTP Parser

The HTTP parser utilities provide functionality for parsing HTTP requests and responses, including header extraction and body handling.

### Text Processing

The text processing utilities include HTML-to-Markdown conversion, table formatting, and general text transformation capabilities.

### Robots.txt

The robots.txt parser implements the Robots Exclusion Protocol for determining which URLs a web crawler is allowed to access.

## Usage

All utility classes are contained within the `Mojo::Utils` namespace. Include the appropriate header file and use the static methods provided by each class.

## Architecture

The utils module follows a modular design pattern where each component is self-contained and can be used independently. This promotes loose coupling and makes it easy to extend the module with new utilities as needed.
