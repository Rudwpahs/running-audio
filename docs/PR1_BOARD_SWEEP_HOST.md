# PR1 board-sweep host workflow

Date: 2026-09-27  
Parent measurement baseline: PR #46 / `codex/pr1-preboard-instrumentation-20260920` / `d1b7ec2b1130fd63fb0eb11fd900b0622766f14c`

## Scope and evidence boundary

This tooling prepares the SX1280 fixed-link board sweep without changing the frozen RF baseline. It does **not** enable SPI tuning, DMA, FreeRTOS/core restructuring, a high-priority RF task, re-arm-before-read, AFH, FEC, ARQ, or adaptive PHY/controller.

The fixed sweep order is:

```text
5000, 1000, 500, 300, 250, 225, 200, 175, 150, 125, 0 us
```

Baseline runs target at least 1,000 RX-observed packet span. Only automatically detected transition-bracketing gaps are marked for a 10,000-packet revalidation pass.

## Machine-readable contracts

- Run metadata: `docs/schemas/pr1_board_sweep_run.schema.json`
- Aggregate JSON results: `docs/schemas/pr1_board_sweep_results.schema.json`
- Flat CSV results: `docs/schemas/pr1_board_sweep_results_csv.schema.json`

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

## 1. Create the Monday plan once

```bash
python tools/pr1_board_sweep.py plan runs/pr1-2026-09-28 \
  --firmware-sha d1b7ec2b1130fd63fb0eb11fd900b0622766f14c
```

This creates the 11 run directories and immutable-per-run metadata before hardware work begins.

## 2. Check progress and prepare only the next run

```bash
python tools/pr1_board_sweep.py status runs/pr1-2026-09-28
```

The status output reports completed/captured/pending runs and the next frozen gap.

Prepare the next TX configuration without modifying the tracked `platformio.ini`:

```bash
python tools/pr1_board_sweep.py next \
  runs/pr1-2026-09-28 \
  firmware/t3s3_sx1280_runtime \
  --tx-port COM7
```

This writes an untracked generated config below `runs/pr1-2026-09-28/generated/` and prints the exact PlatformIO upload command plus the expected RX/TX log paths. Add `--flash` only when the physical TX board is connected.

The older explicit single-gap helper remains available:

```bash
python tools/pr1_board_sweep.py flash-tx firmware/t3s3_sx1280_runtime \
  --gap-us 200 --port COM7 \
  --config-out runs/pr1-2026-09-28/generated/tx-200.ini \
  --dry-run
```

## 3. Capture contract

Save one RX serial log and one TX serial log for every accepted run. Both boot blocks must be present. The parser rejects a run when either board does not match the frozen baseline or when the TX boot log reports the wrong `tx_gap_us`.

Continuous serial telemetry is intentionally avoided because it can perturb the timing under study. At the end of a run, request the telemetry snapshot once with `t`/`T` on each board and include it in the logs.

`scheduler_misses` is currently owned by the TX runtime, so the host parser merges it from `tx.log`.

### Packet-count semantics

The current frozen runtime derives `missing` from gaps between successfully decoded sequence numbers. A CRC-failed packet can later appear inside that missing sequence gap, so **do not** calculate packet count as `crc_good + crc_bad + missing`.

The host contract uses:

```text
packet_count = crc_good + missing
loss_rate    = missing / packet_count
```

`crc_bad` stays a separate RF diagnostic. Packet count is an RX-observed sequence span, not an exact TX-attempt counter; leading loss before the first good packet and trailing loss after the last good packet are not visible.

## 4. Analyze completed runs

```bash
python tools/pr1_board_sweep.py analyze runs/pr1-2026-09-28
```

Outputs:

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

## Current host-only blocker

The frozen TX runtime has no packet-budget stop command and no exact TX-attempt counter in pull telemetry. A host-only tool therefore cannot stop at exactly 1,000 or 10,000 transmitted attempts without changing firmware.

For the frozen gate, use **at least** the requested RX-observed sequence span and repeat under-target runs. Do not add packet-budget firmware control until the frozen measurement gate has been completed or explicitly superseded.
