# PR1 board-sweep host workflow

Date: 2026-09-25
Parent measurement baseline: PR #46 / `codex/pr1-preboard-instrumentation-20260920`

## Scope

This host tooling prepares the Monday SX1280 fixed-link sweep without changing the frozen RF baseline. It does not enable SPI tuning, DMA, FreeRTOS/core restructuring, a high-priority RF task, re-arm-before-read, AFH, FEC, ARQ, or adaptive PHY.

The sweep order is fixed to:

```text
5000, 1000, 500, 300, 250, 225, 200, 175, 150, 125, 0 us
```

Baseline runs target 1,000 packets. Only automatically detected transition-bracketing gaps are marked for a 10,000-packet revalidation pass.

## Run layout

Create the run tree once:

```bash
python tools/pr1_board_sweep.py plan runs/pr1-2026-09-28 \
  --firmware-sha d1b7ec2b1130fd63fb0eb11fd900b0622766f14c
```

This creates:

```text
runs/pr1-2026-09-28/
  manifest.json
  runs/
    01-gap-5000us/
      metadata.json
      rx.log       # captured Monday
      tx.log       # captured Monday
    ...
```

The machine-readable metadata contract is `docs/schemas/pr1_board_sweep_run.schema.json`.

## Faster TX flashing without editing the baseline config

The checked-in `platformio.ini` remains untouched. For each gap, the host tool copies its text to an untracked generated config and changes only the frozen `PR1_TX_GAP_US=5000` macro in that generated file.

Dry-run first:

```bash
python tools/pr1_board_sweep.py flash-tx firmware/t3s3_sx1280_runtime \
  --gap-us 200 --port COM7 \
  --config-out runs/pr1-2026-09-28/generated/tx-200.ini \
  --dry-run
```

Remove `--dry-run` on Monday to invoke PlatformIO upload. The command uses PlatformIO's custom project-config and upload-port options; the tracked baseline file is never rewritten.

## Capture contract

Save one RX serial log and one TX serial log for every accepted run. Both boot blocks must be present. At the end of the run, request telemetry once with `t`/`T` on each board and include the response in the log.

Required RX result fields:

- `crc_good`, `crc_bad`, `missing`, `rssi_dbm`
- `queue_depth`, `max_queue_depth`
- `irq_to_spi_us`, `spi_duration_us`, `rx_processing_us`
- `spi_end_to_rearm_start_us`, `rx_rearm_us`, `irq_to_rx_ready_us`
- `trace_overwrites`

`scheduler_misses` is currently observed by the TX runtime, not the RX runtime, so the host parser merges that value from `tx.log`.

### Packet-count semantics

For the current frozen runtime, `missing` comes from gaps between successfully decoded sequence numbers. A CRC-failed packet can therefore later be represented inside `missing`. Do **not** calculate packet count as `crc_good + crc_bad + missing`.

The host result uses:

```text
packet_count = crc_good + missing
loss_rate    = missing / packet_count
```

`crc_bad` remains a separate diagnostic and may overlap `missing`. The packet count is an RX-observed sequence span, not an exact transmitter-attempt counter; leading loss before the first good packet and trailing loss after the last good packet are not visible.

## Analyze all completed runs

```bash
python tools/pr1_board_sweep.py analyze runs/pr1-2026-09-28
```

Outputs:

```text
report/results.json
report/results.csv
report/loss_transition.svg
report/timing_p99.svg
```

Each run also receives `result.json`.

The parser rejects a run when the boot evidence does not match the frozen baseline or when the TX boot log reports the wrong `tx_gap_us`.

## Transition detection and 10k marking

Adjacent gaps are compared in descending sweep order. A pair is treated as a material transition when:

```text
abs(loss-rate delta) >= max(0.5 percentage points, 3 * pooled binomial standard error)
```

Only the gaps bracketing a material transition are emitted in `revalidate_10000_gaps_us` and marked `revalidate_10000=true` in the report.

## Bottleneck classification

Per-run classification is deliberately conservative. When loss is present, the parser compares the four non-overlapping p99 contributors against `irq_to_rx_ready_us`:

- IRQ -> SPI start: `scheduler_wakeup`
- SPI duration: `spi_transaction`
- SPI end -> re-arm start: `post_spi_processing`
- re-arm duration: `rx_rearm`

A component must account for at least 45% of total p99 turnaround to be labeled as the dominant contributor; otherwise the result is `receiver_turnaround_mixed`. This is low-confidence on one run. If the label is present on the more-lossy side of the strongest statistically material transition, the sweep summary upgrades that evidence to medium confidence. It still does not claim causality.

`rx_processing_us` is overlapping diagnostics and is never added to the four-part turnaround sum.

## Current host-only blocker

The frozen TX runtime has no packet-budget/stop command and no TX-attempt counter in pull telemetry. Therefore the host cannot stop at exactly 1,000 or 10,000 transmitted attempts without changing firmware. The Monday protocol should treat the target as **at least** the requested RX-observed sequence span, request telemetry only once at the end, and repeat any under-target run. Adding firmware run-budget control is intentionally out of scope while the baseline is frozen.
