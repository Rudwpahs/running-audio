# M0 — SX1280 deterministic packet link

This is the first PR1 firmware after the unmodified vendor PingPong example has
passed on both physical boards.

It is **not audio firmware yet**.

## Why M0 exists

M0 isolates one question:

> Can the two exact SX1280 MVSR boards exchange deterministic PR1 v2 packets and
> produce trustworthy sequence/error/RSSI/SNR logs?

Do not add SD, codec, I2S, microphone, jitter buffer or UI work until this gate
is measured successfully.

## Source basis

Pin mapping follows the LILYGO T3-S3 MVSR `pin_config.h` SX1280 branch:

- SCLK 5
- MISO 3
- MOSI 6
- CS 7
- RST 8
- DIO1 9
- BUSY 36
- RF TX enable 10
- RF RX enable 21

The vendor repository's current `pin_config.h` may have a different radio model
selected by default. PR1 therefore declares the SX1280 pins explicitly rather
than inheriting that mutable default.

Vendor references:

- https://github.com/Xinyuan-LilyGO/T3-S3-MVSRBoard
- `examples/SX128x_PingPong_2/SX128x_PingPong_2.ino`
- `libraries/private_library/pin_config.h`

## Safe-by-default behavior

The checked-in PlatformIO default environment defines:

```text
PR1_ENABLE_RF_TEST=0
```

With that value, the sketch prints configuration information but does **not**
initialize or transmit with the SX1280.

This is intentional. The repository must not silently bake in an unreviewed
open-air frequency/power configuration.

## Before enabling RF

First run the vendor's unmodified SX1280 PingPong example under the exact
approved/legal bench-test configuration and record the result.

Then determine and record:

- test frequency
- test power
- antenna
- radio mode
- test location/environment
- TX/RX board roles
- applicable regulatory/test permission

Only after that, make a **local** PlatformIO environment derived from `safe`.
Example shape:

```ini
[env:bench_tx]
build_flags =
    ${env.build_flags}
    -DPR1_ENABLE_RF_TEST=1
    -DPR1_NODE_ROLE_TX=1
    -DPR1_TEST_FREQ_MHZ=<approved_value>
    -DPR1_TEST_POWER_DBM=<approved_value>

[env:bench_rx]
build_flags =
    ${env.build_flags}
    -DPR1_ENABLE_RF_TEST=1
    -DPR1_NODE_ROLE_TX=0
    -DPR1_TEST_FREQ_MHZ=<same_approved_value>
    -DPR1_TEST_POWER_DBM=<approved_value>
```

Do not commit guessed values merely to make the firmware run.

## Current M0 radio mode

When RF is deliberately enabled, M0 initializes SX1280 FLRC explicitly at:

- 650 kbps
- coding rate 3/4 (`3` in the RadioLib API)
- 16-bit preamble
- Gaussian shaping 0.5

Frequency and output power have **no checked-in RF-test value** and must be
provided explicitly at build time.

This FLRC choice is an engineering test hypothesis, not a final regulatory or
product decision.

## Packet

M0 sends a fixed 70-byte packet:

```text
10 B PR1 v2 header
60 B deterministic test pattern
```

The pattern is a function of `packet_seq`, so the receiver can distinguish:

- valid packet
- invalid PR1 header
- valid header but corrupted application bytes

TX starts at 10 packets/s. Audio-rate traffic is deliberately postponed.

## Serial log examples

TX success:

```text
PR1,tx_ok,packet_seq=12,frame_seq=12,len=70,elapsed_us=...,ok=13,fail=0
```

RX success:

```text
PR1,rx_ok,packet_seq=12,frame_seq=12,len=70,rssi=...,snr=...,ok=13,bad_header=0,bad_pattern=0
```

These logs are designed to be captured to CSV/text later for packet-loss and
link analysis.

## Build status

The portable C++ PR1 protocol core has been compiled and host-tested with
`-Wall -Wextra -Werror -pedantic`.

The Arduino/RadioLib M0 project has **not** been compiled in the current agent
runtime because PlatformIO is not installed and external package download is
unavailable there. Treat the first PlatformIO build as an explicit remaining
check, not as a completed result.

## Required order on hardware arrival

1. inspect exact hardware revision
2. run vendor SX1280 PingPong
3. build/flash M0 `safe`
4. verify printed pins/protocol metadata
5. create approved bench RX/TX environments
6. run 100 packets
7. run 10,000 packets
8. save logs
9. analyze loss/corruption
10. only then move to fragmentation and audio
