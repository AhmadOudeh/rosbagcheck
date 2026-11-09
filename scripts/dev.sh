#!/usr/bin/env bash
# Runs a command in the development container (Ubuntu 24.04, the same as CI).
#   scripts/dev.sh build    compile
#   scripts/dev.sh test     C++ unit tests, then Python component tests
#   scripts/dev.sh <cmd>    any other command, e.g. scripts/dev.sh bash
set -euo pipefail
cd "$(dirname "$0")/.."
docker build -q --target dev -t rosbagcheck-dev . >/dev/null

case "${1:-bash}" in
  build) set -- bash -c "cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j" ;;
  test)  set -- bash -c "ctest --test-dir build --output-on-failure && pytest" ;;
esac

interactive=""
if [ -t 0 ]; then interactive="-it"; fi
exec docker run --rm $interactive -v "$PWD":/src -u "$(id -u):$(id -g)" -e HOME=/tmp rosbagcheck-dev "$@"
