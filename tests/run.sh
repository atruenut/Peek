#!/usr/bin/env bash
# Builds and runs the host-side unit tests (pure C++ parts only). Works on Linux/macOS/MSYS2.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/tests
g++ -std=c++20 -Wall -Wextra -Isrc tests/core_tests.cpp src/core/ini.cpp src/core/hotkey.cpp src/core/settings.cpp -o build/tests/core_tests
build/tests/core_tests
