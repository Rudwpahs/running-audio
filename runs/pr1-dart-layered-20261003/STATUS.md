# PR1-DART layered campaign — WORK IN PROGRESS (stopped 2026-10-03, operator shutting down PC)

- Gate A step 1 (safe boot, frozen image db3f944b1055) PASSED on both boards: gate-A/A0-safe-boot.
- Gate A traffic runs (100@5000, 1k@5000, 10k@150) were interrupted before completion; any partial logs under gate-A/ are not results.
- Gate B static-AFH firmware is written (pr1_afh_runtime.hpp, FixedLinkRuntime AFH path, main.cpp h/H commands) but NOT yet compiled or flashed; build was interrupted.
- Boards currently hold the frozen fixed-channel images (RX rf_rx, TX tx-150us or tx-5000us).
- Next: compile Gate B, rerun Gate A 3 runs, then Gate B 100@5000 -> 1k@5000 -> 1k@1000 -> 10k@150.
