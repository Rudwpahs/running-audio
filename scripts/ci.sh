#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

echo "== Existing PR1 v0 application-packet simulator =="
python -m py_compile tools/audio_packet_sim.py
python tools/audio_packet_sim.py --packets 10000

if [[ -f tests/test_packet_codec.cpp ]]; then
  echo
  echo "== Canonical C++ application-packet codec =="
  g++ -std=c++17 -Wall -Wextra -Werror -pedantic \
    tests/test_packet_codec.cpp \
    -o /tmp/pr1_application_packet_test
  /tmp/pr1_application_packet_test
else
  echo
  echo "== Canonical C++ application-packet codec =="
  echo "SKIP: tests/test_packet_codec.cpp is not present in this branch checkout"
fi

echo
echo "== SX1280 RF transport and log-analysis tests =="
python -m unittest \
  tools.test_pr1_rf_protocol \
  tools.test_analyze_m0_serial \
  tools.test_analyze_m0_run \
  -v

echo
echo "== RF packet-budget calculator =="
python tools/rf_packet_budget.py

echo
echo "== Portable C++ RF transport build =="
g++ -std=c++17 -Wall -Wextra -Werror -pedantic \
  firmware/host_tests/test_pr1_rf_protocol.cpp \
  -o /tmp/pr1_rf_transport_test

echo
echo "== Portable C++ RF transport run =="
/tmp/pr1_rf_transport_test

echo
echo "PR1 local checks PASS"
