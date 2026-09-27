# PR1 board-sweep host workflow

Date: 2026-09-27  
Parent measurement baseline: PR #46 / `codex/pr1-preboard-instrumentation-20260920` / `d1b7ec2b1130fd63fb0eb11fd900b0622766f14c`

## Scope and evidence boundary

This tooling prepares and controls the SX1280 fixed-link board sweep without changing the frozen RF baseline. It does **not** enable SPI tuning, DMA, FreeRTOS/core restructuring, a high-priority RF task, re-arm-before-read, AFH, new FEC, ARQ, or adaptive PHY/controller.

The fixed sweep order is:

```text
5000, 1000, 500, 300, 250, 225, 200, 175, 150, 125, 0 us
```

Baseline runs target at least 1,000 RX-observed packet span. Only automatically detected transition-bracketing gaps are marked for a 10,000-packet revalidation pass.

## Machine-readable contracts

- Run metadata: `docs/schemas/pr1_board_sweep_run.schema.json`
- Aggregate JSON results: `docs/schemas/pr1_board_sweep_results.schema.json`
- Flat CSV results: `docs/schemas/pr1_board_sweep_results_csv.schema.json`

The host controller also writes:

- `session.json`: session timestamp, ports, target, frozen firmware SHA, source/build identity
- `build_matrix.json`: safe/RX/per-gap TX build cache/config mapping
- `build_results.json`: optional prebuild outcomes and built `firmware.bin` SHA-256 values
- `runs/<run-id>/run_state.json`: `running`, `complete`, `interrupted`, or `error`
- `runs/<run-id>/partial/<timestamp>/`: archived evidence from interrupted/failed retries

Every accepted run must contain the requested minimum measurements:

- packet count / missing / CRC good / CRC bad / RSSI
- queue depth / max queue depth
- scheduler misses
- `irq_to_spi_us` p99
- `spi_duration_us` p99
- `rx_processing_us` p99
- `spi_end_to_rearm_start_us` p99
- `rx_rearm_us` p99
- `irq_to_rx_ready_us` p99

`trace_overwrites` is retained as an extra diagnostic when observed.

### Percentile boundary

The frozen firmware keeps a `DurationWindow<64>` internally and exposes only each timing field's **p99** in the pull snapshot. Raw timing samples are not emitted over serial. Therefore host-side p50/p95 cannot be reconstructed correctly from the current frozen telemetry and are intentionally **not fabricated**. Adding raw-sample telemetry or p50/p95 firmware fields would change the frozen measurement firmware and is deferred until after this gate.

## Recommended workflow for 2026-09-28

Use the new controller for minimum manual work:

```text
tools/pr1_experiment_controller.py
```

The older `pr1_board_sweep.py` commands remain valid and are used internally for parsing/analysis.

### 1. Prepare once, preferably before the boards are connected

Replace `COM6` / `COM7` with the ports that will be RX and TX:

```bash
python tools/pr1_experiment_controller.py prepare \
  runs/pr1-2026-09-28 \
  firmware/t3s3_sx1280_runtime \
  --rx-port COM6 \
  --tx-port COM7 \
  --target-packets 1000 \
  --prebuild
```

This automatically:

1. creates all 11 run metadata files with timestamp/run ID/gap/target,
2. records frozen firmware SHA and current source/build identity,
3. hashes `platformio.ini` and the controller,
4. creates generated TX configs without editing tracked `platformio.ini`,
5. creates separate PlatformIO build-cache directories for safe, fixed RX, and all 11 TX gaps,
6. when `--prebuild` is supplied, builds them and records any resulting `firmware.bin` SHA-256 values.

Separate build caches make the later upload commands reuse already-built images where PlatformIO considers the cache valid. They do not change RF configuration.

### 2. Verify the physical plan without touching hardware

```bash
python tools/pr1_experiment_controller.py dry-run \
  runs/pr1-2026-09-28 \
  firmware/t3s3_sx1280_runtime \
  --rx-port COM6 \
  --tx-port COM7 \
  --safe-first
```

The plan is intentionally minimal-flash:

```text
safe RX once
safe TX once
fixed RX once
TX 5000 -> capture
RX reset
TX 1000 -> capture
RX reset
...
RX reset
TX 0 -> capture
analyze/report
```

RX is **not reflashed for every gap**. Resetting it between gaps is required to clear its runtime counters/timing windows.

### 3. Run or resume the complete sweep

```bash
python tools/pr1_experiment_controller.py run \
  runs/pr1-2026-09-28 \
  firmware/t3s3_sx1280_runtime \
  --rx-port COM6 \
  --tx-port COM7 \
  --safe-first
```

The controller performs:

```text
safe boot verification on both boards
-> fixed RX upload
-> per-gap TX upload
-> frozen boot/profile validation
-> dual serial capture
-> target polling
-> clean settle interval
-> final RX/TX telemetry snapshot
-> result.json
-> next gap
-> aggregate parser/summary/plots/classification/transition detection
```

A gap is never accepted merely because an upload returned success. The serial boot block must match the frozen role/profile/SPI/radio configuration and the TX must report the requested `tx_gap_us`.

### RX reset fallback

Automatic DTR/RTS reset is a host-side best effort only; the controller still requires a **fresh** `PR1_RUNTIME_BOOT` and `PR1_RUNTIME_LIVE_READY` before accepting the next run. If the board/USB path does not reliably reset through DTR/RTS, use:

```bash
python tools/pr1_experiment_controller.py run \
  runs/pr1-2026-09-28 \
  firmware/t3s3_sx1280_runtime \
  --rx-port COM6 \
  --tx-port COM7 \
  --safe-first \
  --manual-rx-reset
```

In that fallback, the controller pauses only at the ten RX-reset boundaries; press the RX board reset button and Enter. No RF/driver change is made.

## Serial capture and partial-result preservation

Live telemetry remains pull-based because continuous serial printing can contaminate the receiver timing being measured.

For each run the controller:

1. writes RX/TX boot and serial data directly to `rx.log` / `tx.log`, flushing each line,
2. waits a conservative host-estimated interval,
3. polls RX with `t` only until the RX-observed packet target is reached,
4. waits a telemetry-free settle interval long enough to replace the firmware's 64-sample timing window under the host scheduling assumption,
5. takes one authoritative final RX snapshot and one TX snapshot,
6. parses and validates the final logs.

If Ctrl-C, a serial error, a validation failure, or another exception occurs, the current logs are left on disk and `run_state.json` is changed to `interrupted` or `error`. On retry, existing partial evidence is moved to `partial/<timestamp>/` before a new attempt starts.

`scheduler_misses` is currently owned by the TX runtime, so the host parser merges it from `tx.log`.

### Packet-count semantics

The current frozen runtime derives `missing` from gaps between successfully decoded sequence numbers. A CRC-failed packet can later appear inside that missing sequence gap, so **do not** calculate packet count as `crc_good + crc_bad + missing`.

The host contract uses:

```text
packet_count = crc_good + missing
loss_rate    = missing / packet_count
```

`crc_bad` stays a separate RF diagnostic. Packet count is an RX-observed sequence span, not an exact TX-attempt counter; leading loss before the first good packet and trailing loss after the last good packet are not visible.

## Results

After a complete controller run, or manually with:

```bash
python tools/pr1_board_sweep.py analyze runs/pr1-2026-09-28
```

outputs are:

```text
report/results.json
report/results.csv
report/summary.md
report/loss_transition.svg
report/timing_p99.svg
```

Each parsed run also receives `result.json`.

## Transition detection and 10k marking

Only actual adjacent points in the frozen sweep are compared. Missing points are never bridged. A pair is a material transition when:

```text
abs(loss-rate delta) >= max(0.5 percentage points, 3 * pooled binomial standard error)
```

Only gaps bracketing a material transition are emitted in `revalidate_10000_gaps_us` and marked `revalidate_10000=true`.

## Bottleneck candidates

Per-run timing classification uses four **non-overlapping** turnaround segments:

- `scheduler_wakeup`: IRQ -> SPI start
- `spi`: SPI transaction duration
- `rx_processing`: SPI end -> RX re-arm start
- `rearm`: RX re-arm duration

A component must account for at least 45% of `irq_to_rx_ready_us` p99 to be called dominant; otherwise the label is `receiver_turnaround_mixed`. `rx_processing_us` remains an overlapping diagnostic from SPI start through packet processing and is graphed/reported, but is not added to the four-part timing decomposition.

At the strongest measured loss transition the aggregate analyzer can also add `receiver_saturation_candidate` when total IRQ-to-RX-ready turnaround rises materially while RSSI and CRC-bad behavior do not provide a competing explanation. This is explicitly a **candidate, not causal proof**.

## Current frozen-baseline blockers

1. The frozen TX runtime has no packet-budget stop command and no exact TX-attempt counter in pull telemetry. A host-only tool therefore cannot stop at exactly 1,000 or 10,000 transmitted attempts without changing firmware. The gate uses **at least** the requested RX-observed sequence span.
2. Raw timing samples are not exposed, so host-side p50/p95 cannot be reconstructed from p99 snapshots.
3. Reliable software reset through DTR/RTS is USB/driver dependent. The controller verifies fresh boot after every reset attempt and refuses to silently continue if reset/boot evidence is missing; the manual reset fallback preserves the frozen firmware.

Do not resolve any of these blockers by changing RF firmware before the frozen sweep unless the measurement gate is explicitly superseded.
