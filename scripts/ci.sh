#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

echo "== Existing PR1 v0 application-packet simulator =="
python -m py_compile tools/audio_packet_sim.py
python tools/audio_packet_sim.py --packets 10000

echo
echo "== Compact SX1280 protocol and log-analysis tests =="
python -m unittest \
  tools.test_pr1_rf_protocol \
  tools.test_analyze_m0_serial \
  tools.test_analyze_m0_run \
  -v

echo
echo "== RF packet-budget calculator =="
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
echo "PR1 local checks PASS"
