# PR1 RF cause diagnostics — design specification (2026-10-10)

**Owner:** PR1 technical research lane
**Design approval basis:** Approved PR1 RF comprehensive 39-hypothesis cause investigation V2.0 and the explicit request to refine/document/code/save to GitHub.
**Base:** claude/pr1-audio-prototype-20261003 at 5bf586fc1c2d2d2579d71c03f6ecc05023624719
**PR:** separate research branch; do not merge to main or audio baseline automatically.

## Intent and success criteria

Make the existing October 7 evidence reproducible and safe to interpret while preparing the *smallest* discriminating physical experiments. A maintainer must be able to run one offline Python command to derive correct RF missing percentage, CRC-bad fraction with separate denominator, last-successful-packet RSSI, audio concealment, recent64 timing and provenance hashes. Missing metrics remain unknown; the tool makes NO causal claims and never touches serial, firmware or radio settings.

## Interfaces

Input: one or more run directories, each containing summary.json and optionally rx_pull.log from existing audio-run tooling. Both current flat summary and earlier nested summary.rf schema supported.

Core:
- analyze_run(run_directory) -> mapping with label, window_s, crc_good, crc_bad, missing, rf_loss_pct, crc_bad_among_decoded_pct, last_good_packet_rssi_dbm, concealed_pct, recent_64_p99_us, trace_ring_overwrites, source_sha256, warnings, limitations.
- compare_runs(baseline, candidate) -> descriptive percentage-point differences plus explicit noncausal label. This is a library helper; CLI prints independent run rows and does not pretend ABBA inference.
- CLI: python tools/pr1_field_diagnostics.py RUN_DIR [RUN_DIR...] --format md|json, stdout only. Exit 2 with clean error on unreadable or invalid input.

Metrics:
RF missing = 100 * missing/(crc_good+missing); CRC bad among decoded attempts = 100*crc_bad/(crc_good+crc_bad). Can overlap. Last good RSSI is not mean/median. Recent64 p99 is not run-wide p99. Trace overwrite count is not packet loss. Concealed_pct from source only, never reconstructed unless explicitly grounded by raw counter schema.

Evidence scope: host-side Python 3 stdlib; data untouched; SHA256 for source files. No physical data invented. No new firmware instrumentation, build flags, TX power changes, AFH integration, antenna rewiring, equipment procurement or physical test claims.

## Implementation units

1. tests/test_pr1_field_diagnostics.py — contract tests for counter math, missing input failure, legacy schemas, p99/RSSI caveats, JSON/Markdown, nonmutation.
2. tools/pr1_field_diagnostics.py — read-only normalizer and report CLI; no serial dependencies.
3. docs/research/2026-10-10-pr1-rf-root-cause-investigation.md — 39-hypothesis evidence inventory and E0–E4 ABBA pilot protocol.
4. docs/superpowers/plans/2026-10-10-pr1-rf-cause-diagnostics.md — verified change/deferred gates.
5. GitHub Draft PR based on audio branch, not main.

## Review focus and failure handling

- Missing or negative counters must fail closed, not score 0%.
- CRC missing overlap must never be summed.
- Source historical loss percentage disagreement must warn and retain recalculated value.
- Last packet RSSI and limited-duration p99 must not appear as full-run distributions.
- Existing radio/upload/download scripts and immutable raw inputs must not be altered.

## Later-stage design gates (NOT implemented in this PR)

A. E0 real board SKU/revision, photo and hardware-path audit.
B. E1 hand test on fixed geometry ABBA; E2 body-only LOS obstruction; E3 station-fixed TX with wearable RX. Keep tests few, predeclare predictions, preserve raw logs.
C. Depending on evidence, separate reviewed firmware instrumentation for fixed-size cold-path histograms, CRC/no-IRQ buckets, loss bursts, full-run timing; first demonstrate the telemetry does not change critical-path performance.
D. GO/MODIFY/REDESIGN decision after actual hardware tests and measured end-to-end latency, not from host reconstruction.
