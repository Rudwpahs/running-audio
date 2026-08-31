#!/usr/bin/env bash
set -euo pipefail
CXX=${CXX:-g++}
FLAGS=(-std=c++17 -Wall -Wextra -Werror -pedantic -O2)
SAN_FLAGS=(-std=c++17 -Wall -Wextra -Werror -pedantic -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer)
for src in tests/test_*.cpp; do
  name=$(basename "$src" .cpp)
  "$CXX" "${FLAGS[@]}" "$src" -o "/tmp/$name"
  "/tmp/$name"
done
for src in tests/test_*.cpp; do
  name=$(basename "$src" .cpp)
  "$CXX" "${SAN_FLAGS[@]}" "$src" -o "/tmp/${name}_san"
  ASAN_OPTIONS=detect_leaks=1 "/tmp/${name}_san"
done
