#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

echo "== Python protocol and log-analysis tests =="
python -m unittest \
  tools.test_pr1_rf_protocol \
  tools.test_analyze_m0_serial \
  tools.test_analyze_m0_run \
  -v

echo
echo "== Packet budget smoke test =="
python tools/rf_packet_budget.py

echo
echo "== Portable C++ protocol build =="
g++ -std=c++17 -Wall -Wextra -Werror -pedantic \
  firmware/host_tests/test_pr1_rf_protocol.cpp \
  -o /tmp/pr1_protocol_test

echo
echo "== Portable C++ protocol run =="
/tmp/pr1_protocol_test

echo
echo "PR1 protocol checks PASS"
