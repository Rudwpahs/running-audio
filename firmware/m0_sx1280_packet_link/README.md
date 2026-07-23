# M0 — SX1280 deterministic packet link

M0 is the first PR1 custom firmware **after** the unmodified LILYGO SX1280
PingPong example has passed on both physical boards.

It is not audio firmware yet.

## Relationship to the current MVP

`docs/DECISIONS_2026-07-23.md` and `docs/TECHNICAL_MVP.md` remain the higher-level
source of truth.

M0 implements the T1 data-link preparation in a way that makes corruption and
packet loss measurable:

- 100-byte deterministic packet, matching the T1 packet-size target
- 10-byte compact PR1 transport header
- 90-byte test pattern derived from `packet_seq`
- explicit TX/RX logs
- safe-by-default RF disable

The current voice diagnostic profile remains **8 kHz / 8-bit / mono / 20 ms raw
PCM**. M0 does not change that decision and does not add an audio codec.

## Pin source

For the LILYGO T3-S3 MVSR SX1280 branch:

- SCLK 5
- MISO 3
- MOSI 6
- CS 7
- RST 8
- DIO1 9
- BUSY 36
- RF TX enable 10
- RF RX enable 21

These match the vendor MVSR `pin_config.h` SX1280 definitions. The vendor file
can have another radio selected by default, so M0 declares the SX1280 pins
explicitly rather than inheriting a mutable preprocessor selection.

## Safe-by-default behavior

The checked-in PlatformIO default environment sets:

```text
PR1_ENABLE_RF_TEST=0
```

With that value the board prints build/pin metadata but does not initialize or
transmit with SX1280.

No guessed frequency or power is checked in.

## Before enabling RF

1. Inspect both physical boards and confirm exact SX1280 Without-PA variant.
2. Pass the unmodified vendor SX1280 PingPong baseline.
3. Confirm the exact bench-test configuration and applicable Korean regulatory
   route/conditions.
4. Record antenna, mode, frequency/channel, output-power setting and location.
5. Only then create local TX/RX environments with explicit values.

Shape only:

```ini
[env:bench_tx]
build_flags =
    ${env.build_flags}
    -DPR1_ENABLE_RF_TEST=1
    -DPR1_NODE_ROLE_TX=1
    -DPR1_TEST_FREQ_MHZ=<confirmed_value>
    -DPR1_TEST_POWER_DBM=<confirmed_value>

[env:bench_rx]
build_flags =
    ${env.build_flags}
    -DPR1_ENABLE_RF_TEST=1
    -DPR1_NODE_ROLE_TX=0
    -DPR1_TEST_FREQ_MHZ=<same_confirmed_value>
    -DPR1_TEST_POWER_DBM=<confirmed_value>
```

Do not commit made-up values merely to make RF run.

## Initial M0 modulation hypothesis

When deliberately enabled, M0 currently initializes FLRC at:

- 650 kbps
- coding rate 3/4 in RadioLib API
- 16-bit preamble
- Gaussian shaping 0.5

This is one **bench baseline hypothesis**, not the final modulation choice. The
current Technical MVP requires comparing LoRa/FLRC/GFSK using measured results.

## Logs

TX example:

```text
PR1,tx_ok,packet_seq=12,frame_seq=12,len=100,elapsed_us=...,ok=13,fail=0
```

RX example:

```text
PR1,rx_ok,packet_seq=12,frame_seq=12,len=100,rssi=...,snr=...,ok=13,bad_header=0,bad_pattern=0
```

Analyze one controlled run:

```bash
python tools/analyze_m0_run.py tx.log rx.log
python tools/analyze_m0_serial.py rx.log
```

## Build status

The portable C++ protocol core has been host-compiled with strict warnings in
the development session. The Arduino/RadioLib M0 project itself still requires
a real PlatformIO build in the user's environment because PlatformIO/package
installation was unavailable in the agent runtime.

Do not mark M0 compiled or hardware-tested until that evidence exists.
