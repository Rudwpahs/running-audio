# PR1 RF Cause Diagnostics Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (- [ ]) syntax for tracking.

**Goal:** Produce an inspectable, reproducible, noninvasive PR1 field evidence audit and a stage-gated 39-hypothesis diagnosis.

**Architecture:** Python standard-library host-only CLI normalizes existing flat/nested audio run summary.json and optional RX pull logs. No firmware, RF, I2S or power settings modified. A separate GitHub Draft PR against the frozen audio lane carries the documents and analyzer.

**Tech Stack:** Python3, stdlib unittest, SHA256, GitHub branch.
**Spec:** docs/superpowers/specs/2026-10-10-pr1-rf-cause-diagnostics-design.md

## Global Constraints

- PR1 is a phone deposit ↔ audio receiver exchange system; field test must ultimately use fixed TX/worn RX.
- Main and audio frozen baseline not edited.
- No board flash, output change, antenna rework, AFH/FEC/ARQ, or hardware execution from this task.
- Existing October 7 raw logs stay immutable and failed/excluded run stays in corpus.
- Never infer RSSI distribution from one last-good packet, or whole-run p99 from last 64 samples.
- CRC bad denominator and forward sequence missing denominator remain distinct and potentially overlapping.
- Any physical experiments require E0 checks and documented protocol/forecasts.

## Review Focus

1. Wrong/missing/negative counters produce clean nonzero error.
2. A partial nested audio summary without enough information does not invent concealment rate.
3. A single latest RSSI never gets mislabeled RSSI p10/p50/p90.
4. Trace overwrite count never enters RF loss calculation.
5. Reader never writes to historical raw files or opens serial ports.

---

### Task 1 — Host-only evidence math and schema tests

**Files:** tests/test_pr1_field_diagnostics.py; tools/pr1_field_diagnostics.py

- [x] Write tests for flat/nested summary schemas, separate counter denominators, missing values, RSSI semantics, p99 semantics, discrepancy warning, read-only CLI.
- [x] Run tests in RED phase; expected missing-module import failure was observed.
- [x] Implement analyze_run(), compare_runs() and CLI with standard library only.
- [x] Re-run test module locally; 8 tests, all pass.
- [x] Compile Python sources with python -m py_compile.
- [x] Save test and tool code in isolated research branch.

**Verification:**
    python -m unittest discover -s tests -p 'test_pr1_field_diagnostics.py' -v
    python -m py_compile tools/pr1_field_diagnostics.py tests/test_pr1_field_diagnostics.py

### Task 2 — Audit record and minimal board experiment design

**Files:** docs/research/2026-10-10-pr1-rf-root-cause-investigation.md; docs/superpowers/specs/2026-10-10-pr1-rf-cause-diagnostics-design.md

- [x] Transcribe 39 cause hypotheses with specific distinguishing evidence, not probability guesses.
- [x] Preserve four raw run counters and correct rates; flag undocumented 5m LOS setup error.
- [x] Define E0 photo/firmware identity and E1–E3 few-run ABBA controlled diagnostics.
- [x] Write uncertainty and stop rules for physical tests (no execution).
- [x] Link issue #52, source branch, runtime metric code, datasheet, peer-reviewed studies.

### Task 3 — Traceability and GitHub handoff

**Files:** this plan, test/tool/research docs; isolated GitHub branch and Draft PR.

- [x] Create research/pr1-rf-cause-diagnostics-20261010 from audio branch.
- [x] Push test, implementation and investigation report via GitHub connection.
- [ ] Independently verify the final remote file list and latest commit SHA.
- [ ] Open Draft PR targeting claude/pr1-audio-prototype-20261003 (not main).
- [ ] Report URL, verified tests, unrun full suite/physical tests, and next human action.

### Task 4 — Defer field and firmware experiments until new evidence

- [ ] E0 user/operator supplies clear TX/RX both-side photos, RF board revision, antenna SKU, binary hashes.
- [ ] E1 supported vs edge-held TX; E2 both radios fixed while person alone crosses LOS; E3 fixed station TX with worn RX (ABBA, small pilot).
- [ ] If measurements still cannot discriminate, write a separate reviewed instrumentation spec, add failing host and firmware tests, measure critical-path overhead, and request physical execution authorization.
- [ ] Reconsider AFH/PHY/antenna only after evidence distinguishes body fading from interference and processing.

**Status:** Task 4 is intentionally open. No merge or physical operation is part of the current change.
