# PR1 Channel Quality + XOR FEC Hardening

Branch: `codex/pr1-quality-fec-hardening-20260831`

This change hardens the host-side/core pieces of issues #26 and #27. It does **not** enable AFH/FEC on the live SX1280 path and it does **not** claim hardware RF validation.

## #26 Channel quality estimator

Implemented/strengthened:

- fast/slow PDR EWMA defaults remain alpha 1/4 and 1/32 and are runtime configurable;
- `ACTIVE -> SUSPECT -> EXCLUDED -> PROBE` with separate recovery hysteresis;
- minimum active-channel floor remains 12 by default;
- protected re-exploration ranking uses probe age, historical slow PDR, neighboring-channel quality, and probe-failure backoff;
- failed probes use exponential backoff from 200 ms up to 3.2 s;
- reinclusion requires at least 2 successes in the last 3 completed probes;
- stale probe results are rejected unless the channel is currently in `Probe` state;
- probe history is reset on a new exclusion cycle so old successes cannot leak into a later reinclusion decision;
- uint32 millisecond wrap is handled by unsigned elapsed-time subtraction;
- dedicated 8-byte micro-probe wire format is provided so channel exploration does not consume XOR parity/audio repair packets.

The estimator only produces state/map decisions. Actual radio scheduling must keep normal audio on ACTIVE/SUSPECT channels while a protected micro-probe temporarily visits an excluded channel.

## #27 XOR FEC core

Implemented/strengthened:

- runtime modes: OFF, 4+1 XOR, experimental 3+1 XOR;
- parity remains over the fixed 100-byte codec payload;
- parity application payload is 104 bytes (4 bytes metadata + 100 bytes XOR) and is statically checked to fit beneath the current 127-byte FLRC packet ceiling when wrapped by the current PR1 header;
- streaming encoder emits parity on the Nth source frame and then immediately advances the group ID;
- explicit reset supports safe runtime mode transitions without leaking a partial previous group;
- detailed recovery outcomes distinguish recovered / no-missing / too-many-missing / invalid metadata / invalid argument;
- optional stats count parity sent, recovered frames, unrecoverable groups, and rejected ambiguous attempts;
- recovery can require an expected group ID so stale parity cannot be applied to another group;
- parity wire encode/decode validates source count and source bitmap;
- deterministic repair-channel selection chooses another ACTIVE AFH channel when at least two are available;
- interleave depth 2 remains an experiment and defaults OFF.

## Host verification

The hardened channel-quality and FEC tests were compiled and executed locally with:

- C++17;
- `-Wall -Wextra -Werror -pedantic`;
- normal optimized build;
- AddressSanitizer + UndefinedBehaviorSanitizer.

Both `test_channel_quality` and `test_fec` passed in normal and sanitizer builds. A template-deduction compile error found during the FEC work was fixed by passing the source-count template argument explicitly.

## Erasure-only FEC simulation

`tools/fec_loss_sim.py` compares OFF, 4+1, and 3+1 under synthetic erasures. This is a structural FEC model, **not an RF/FLRC performance model**.

| Model | OFF post-loss | 4+1 post-loss | 3+1 post-loss |
| --- | ---: | ---: | ---: |
| IID 1% | 1.030% | 0.043% | 0.030% |
| IID 5% | 4.979% | 0.951% | 0.733% |
| IID 10% | 9.967% | 3.452% | 2.711% |
| Burst 2 packets | 2.065% | 1.808% | 1.722% |
| Burst 3 packets | 3.098% | 2.840% | 2.752% |
| Burst 4 packets | 4.130% | 3.873% | 3.780% |

Nominal parity overhead relative to source-frame count is 25% for 4+1 and 33.3% for 3+1.

Interpretation: XOR is highly effective against isolated/IID single erasures, while immediate parity only modestly improves multi-packet bursts. That supports sending parity on a different ACTIVE RF channel where possible and keeping interleaving as a measured experiment rather than enabling it by default.

## Still hardware-gated

Do not close #26/#27 as fully validated yet. Remaining hardware work includes:

- wire the quality estimator/micro-probe scheduler to real SX1280 FLRC timing without interrupting audio;
- measure Wi-Fi-localized exclusion and automatic reinclusion time;
- verify micro-probes do not create audible dropouts;
- measure actual per-channel PDR/RSSI/burst distributions;
- integrate runtime FEC policy with the link controller and real TX queues;
- measure parity airtime and queue pressure at FLRC 1.3M / 650k / 520k;
- compare FEC OFF / 4+1 / 3+1 and optional interleave depth 2 on the two physical boards.

Until those measurements exist, thresholds, FEC mode switching, and repair scheduling remain experimental rather than production defaults.
