# PR1 RF comprehensive root-cause investigation — implementation record

Date: 2026-10-10 KST
Status: Host log analyzer implemented; actual hardware diagnosis still open.
Scope: PR1 phone deposit and receiver exchange system. Product geometry is fixed station TX and worn/carried RX; October 7 field geometry was hand-held TX and RX next to a laptop.

## Decision

DIAGNOSE/MODIFY; do not condemn hardware, switch chip, raise TX output, integrate AFH, or claim body shadowing alone. Existing 5m body test fails exploratory V0 targets, but physical antenna, placement, interference, PHY and firmware timing are not isolated.

## Evidence from actual October 7 audio-lane raw files

| Trial | Good | CRC bad | Missing | RF missing / (good + missing) | CRC bad / (good + bad) | Final good RSSI | Audio concealed |
|---|---:|---:|---:|---:|---:|---:|---:|
| Outdoor 1m repeat | 31006 | 44 | 142 | 0.456% | 0.142% | -62 dBm | 0.013% |
| Outdoor 5m LOS first | 29787 | 919 | 1371 | 4.400% | 2.993% | -79 dBm | 1.782% |
| Outdoor 5m LOS retry | 30855 | 207 | 295 | 0.947% | 0.666% | -71 dBm | 0.059% |
| Outdoor 5m body | 28054 | 1945 | 3105 | 9.965% | 6.484% | -82 dBm | 4.052% |

Actual source: https://github.com/Rudwpahs/running-audio/tree/claude/pr1-audio-prototype-20261003/runs/pr1-audio-prototype-20261007-field
Issue: https://github.com/Rudwpahs/running-audio/issues/52

The original 5m first-run “setup error” was not explained: keep it in analysis rather than discarding. RX and TX placements and indoor/outdoor conditions differ.

Measurement traps (confirmed in source):
- RSSI is the LAST successfully decoded FLRC packet's RSSI, not mean/p10/p50/p90. A last-good RSSI of -82 dBm does not reveal failed packet RSSIs. Do not call the observed -42 to -62 dBm desk/outdoor difference pure body loss.
- The displayed p99 duration values use DurationWindow<64>, meaning LAST 64 observations only, not run-wide p99.
- trace_overwrites counts a 128-event RAM ring rollover; it is not missing packets.
- Sequence gap missing and CRC failures can overlap. Do not add their percentages. Compute each with explicit separate denominators.
- The same-condition 5m LOS trials differ greatly and are not evidence of a stable baseline.
- Audio branch current source sets 2404MHz / 1300kbps / CR3/4 / 0dBm and AFH OFF. Gap 150us denotes idle AFTER a blocking transmit.
- AFH C3n2 hotspot mitigation measured on a DIFFERENT branch is not a validated solution to wearable attenuation.
- Spec sheet -100dBm at 1300kbps/CR3/4 is a standardized laboratory criterion, not inferred margin from a single good packet.
- LILYGO SX1280PA variant includes a manufacturer >3dBm setting damage warning; do not change TX output without SKU and local regulatory check.

## Exhaustive hypothesis inventory: 39, not 39 established root causes

| ID | Candidate | Isolating evidence |
|---|---|---|
| A1 | Antenna connector intermittent | Power-off connector inspection; substitute known-good antenna |
| A2 | Wrong antenna band, gain or radiation pattern | Identify antenna SKU and 2.4 GHz spec |
| A3 | RF antenna-selection resistor routes incorrect port | PCB revision and resistor close-up vs schematic |
| A4 | SMA/U.FL/coax solder/feed/matching damage | Power-off visual audit; VNA if available |
| A5 | SX1280 vs SX1280PA or board revision mistaken | Photo chip/silkscreen; purchase SKU |
| A6 | RF switch TXEN/RXEN variant mismatch | Confirm actual GPIO and schematic |
| A7 | Radio ESD, unit-to-unit variation, thermal issue | Controlled role/antenna swap |
| A8 | Laptop/USB/powerbank/metal/ground interaction | Controlled proximity and cable A/B |
| B1 | Torso direct-path shadowing | Stationary radios; person enters/leaves path only |
| B2 | TX hand detunes antenna | Identical TX coordinate; hand clear vs edge-held |
| B3 | Worn RX absorption and detuning | Fixed TX; supported RX vs waist/chest worn |
| B4 | Antenna orientation/polarization mismatch | Controlled orientation A/B |
| B5 | Low height and ground reflection null | Controlled height A/B |
| B6 | Building/person multipath | Small location sweep with geometry logged |
| B7 | Pocket/bag/clothing/moisture effect | Final product positions A/B |
| B8 | Correlated deep fades longer than redundancy | Loss burst histogram and concealment timeline |
| C1 | Wi-Fi on or near fixed 2404 MHz | Measured channel occupancy and fixed channel A/B |
| C2 | Bluetooth/FHSS interference | Ambient traffic and same-session A/B |
| C3 | Other 2.4GHz noise sources | Spectrum/time occupancy scan where available |
| C4 | ESP32 self interference | Confirm radio stack operational state |
| C5 | USB/laptop/power supply EMI | Controlled cable/host/power distance |
| D1 | 1.3Mbps FLRC sensitivity insufficient | Controlled 1300 vs 650 kbps without other changes |
| D2 | RX High Sensitivity configured differently | Check hardware/firmware and Semtech meaning |
| D3 | Configured output differs from realized EIRP | Variant and measured radiated power if necessary |
| D4 | Sync word/preamble/CRC/errata incompatibility | Review exact RadioLib image/register mapping |
| D5 | Payload zero runs/whitening/scrambling | Identical channel and controlled payload A/B |
| D6 | Frequency offset, clock or temperature drift | Warm-up/temp/frequency measurement |
| D7 | Incorrect IRQ/read/CRC status classification | Explicit packet status/error counters |
| E1 | RX post-read/rearm overruns next preamble | Full-run bounded timing histograms |
| E2 | Shared flash I-cache/USB timing disturbance | C1 regression and hot-path review |
| E3 | TX schedule/airtime/gap inconsistency | TX counters/timestamps and binary hash |
| E4 | IRQ/SPI/BUSY/RF-switch failure | Explicit no-IRQ, CRC and rearm buckets |
| E5 | 64-sample p99 and 128-slot ring hide bursts | Cold-path bounded counters; no hot printf |
| E6 | Sequence wrap/duplicate/audio metrics conflated | Separate denominators and TX reference |
| F1 | Powerbank ripple/sag without MCU reboot | Power A/B; supply waveform if available |
| F2 | Locker/case/phone/metal near antenna | Final physical enclosure/geometry A/B |
| F3 | Held TX differs from product fixed TX/worn RX | Test product topology |
| F4 | Site/time/person changes confound results | ABBA same session + photos |
| F5 | Latency/subjective audio quality unknown | Measure physical latency, listening/burst metrics |

## Minimal controlled diagnostic sequence

**E0, no board changes and no RF runs** — Take TX and RX front/back photos, antenna close-ups, purchased exact SKU, antenna model, RF-board/expansion board revision, existing flashed binary checksum, firmware boot profile, USB/power cable layout. Obtain explanation of excluded 5m LOS first run. Verify RF switch/antenna-selection resistor and whether the model has SX1280PA. Do not desolder RF components or operate with antenna unplugged.

**E1, hand/antenna handling** — Outdoor 1m, fixed positions/height/orientation and same supply. ABBA (supported TX with hand free, same position edge-held, supported, held), 30–60s each. Predictions written beforehand. Assess CRC and missing and, once available, RSSI distribution.

**E2, body-only shadowing** — Both boards on stable supports at fixed 5m. Person alternates clearing LOS vs blocking LOS, but never moves or grips the boards. ABBA. This isolates shadowing better than the earlier experiment where the TX was held to the body.

**E3, actual product use** — TX fixed at phone deposit point. Compare RX on a nonmetal stand vs worn at waist/chest with documented orientation. ABBA. Record sound quality, concealment and loss. A TX-held demo does not validate a worn RX.

**Conditional E4** — Only after finding an effect, adjust ONE variable (orientation, mounting height, RX separation, antenna replacement, fixed channel, or PHY 650kbps). Do not combine RF tuning and environmental A/B. Do not change TX output without board identity and lawful limit verification. Gate C3n2/FEC/ARQ/audio integration remain outside this initial diagnosis.

No excessive blanket board testing. 30–60s per state are pilot diagnostics, not final product qualification. Stop if hardware appears damaged or on reset/I2S errors/sound failure. Preserve all raw files including failures.

## Per-run manifest contract (required for future field data)

Use one manifest for each run, with:
- run_id, datetime_kst, predicted_direction, operator
- TX/RX board ID, exact hardware revision, antenna model, RX/TX binary SHA256
- radio profile (frequency MHz, bit rate kbps, coding rate, configured output, AFH flag, packet size, idle gap us)
- geometry (distance m, TX/RX height cm, orientation degrees, physical mounting, body position, cable/power arrangement)
- ambient/channel occupancy if measured, else null (never guess Wi-Fi channel)
- measurement window seconds, adjacent comparison run IDs, listener report, source file checksums.
- direct raw: good, CRC-bad, forward-sequence missing, concealed, duplicates, late, I2S and reset counters; annotate metric denominators.
- no per-frame USB polling or serial formatting in the RX critical path.

## Instrumentation change intentionally DEFERRED

Future separate firmware PR may implement IRAM-safe, fixed-size RSSI histogram (successful packets ONLY), full-run bounded-latency counts, CRC vs no-IRQ vs decode buckets, and burst timeline; read only after experiment. The tool added in this branch is HOST ONLY and does not fix missing historical per-packet data. Before any firmware change, demonstrate identical RF/CPU timing vs frozen profile. There is no board authorization or physical measurement in this branch.

## Decision gates

GO only after representative fixed-TX/worn-RX conditions satisfy pre-registered loss, concealed-audio, listener and physically measured latency criteria; hardware/regulatory identity verified.
MODIFY when reproducible antenna/mounting/PHY change improves performance within power/size/latency constraints.
REDESIGN only if known-good RF chain plus feasible antenna/mounting/PHY options still repeatedly fail controlled product tests.

Current V0 body thresholds (<5% RF loss, <2% concealed) both failed. They were exploratory, not final commercial targets. 10/20/40m and physical latency have not been measured.

## Read-only code and validation

Command from the repository root:

    python tools/pr1_field_diagnostics.py runs/pr1-audio-prototype-20261007-field/1_m_ref_retry runs/pr1-audio-prototype-20261007-field/5_m_LOS runs/pr1-audio-prototype-20261007-field/5_m_LOS_repeat runs/pr1-audio-prototype-20261007-field/5_m_body --format md

    python -m unittest discover -s tests -p 'test_pr1_field_diagnostics.py' -v

Outputs separate metrics with source SHA256 and caveats; supports old flat and nested summary schemas; never edits raw input or invokes flashing/serial.

## Primary sources

- Runtime: https://github.com/Rudwpahs/running-audio/blob/claude/pr1-audio-prototype-20261003/firmware/t3s3_sx1280_runtime/include/pr1_fixed_link_runtime.hpp
- Metrics: https://github.com/Rudwpahs/running-audio/blob/claude/pr1-audio-prototype-20261003/firmware/t3s3_sx1280_runtime/include/pr1_live_metrics.hpp
- RadioLib 7.7.1: https://github.com/jgromes/RadioLib/blob/7.7.1/src/modules/SX128x/SX128x.cpp
- Semtech SX1280 data: https://www.hy-line-group.com/products/hcp/datasheet/semtech/sx1280-1_datasheet.pdf
- LILYGO MVSR: https://wiki.lilygo.cc/products/t3-series/t3-s3-mvsr/
- Cotton et al. 2011 https://ieeexplore.ieee.org/document/5782245/
- Mellios et al. 2014 https://doi.org/10.4108/ICST.BODYNETS.2014.257000
- Jeon et al. 2015 https://doi.org/10.5515/JKIEES.2015.15.2.97
- Turbic et al. 2017 https://doi.org/10.1186/s13638-017-0956-6
- Su et al. 2023 https://doi.org/10.1109/OJAP.2023.3239369
- Undermind literature search https://app.undermind.ai/projects/b951fe1e-b03a-4897-8827-4fa1080691c5
