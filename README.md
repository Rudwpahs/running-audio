# PR1 — Running Audio / LUN

PR1 is LUN's current prototype project for a **screen-light, separated wireless
audio system**.

## Current status — 2026-07-23

The project is in **technical proof-of-concept**, not production.

Current first-prototype hardware:

- 2 x LILYGO T3-S3 MVSR
- exact option: `SX1280 2.4G Without PA + MVSR [H594-01]`
- onboard MAX98357A audio output
- Adafruit Bone Conductor Transducer #1674 (8 ohm / 1 W)
- microSD as the first controlled audio source

The development board is **not the planned final product PCB**. If the RF/audio
PoC succeeds, the production design will be rebuilt around a smaller,
certification-friendly architecture.

## Important architecture reset

The repository originally contained a locked **Wi-Fi SoftAP + UDP** architecture.
That specification is obsolete for the current PR1 direction.

The current technical source of truth is:

- [`docs/ARCHITECTURE_V2.md`](docs/ARCHITECTURE_V2.md)
- [`docs/TEST_PLAN_V2.md`](docs/TEST_PLAN_V2.md)

Historical collaboration files remain for audit/context, but their old Wi-Fi
technical specification must not be implemented as the current PR1 product.

## The key technical problem

The old learning format used 32 kHz, 16-bit, mono PCM in 10 ms frames:

```text
32,000 samples/s * 2 bytes/sample * 0.010 s = 640 bytes/frame
```

A 640-byte raw frame does not fit in one SX1280 FLRC or GFSK packet. PR1 v2
therefore treats **fragmentation and audio compression as first-class design
problems** instead of assuming a UDP-sized payload.

## Current PC-testable protocol

PR1 v2 uses a 10-byte application header with separate RF-packet and audio-frame
sequence numbers plus fragmentation fields.

Files:

- `tools/pr1_rf_protocol.py` — packet header, fragmentation, reassembly
- `tools/test_pr1_rf_protocol.py` — unit tests
- `tools/rf_packet_budget.py` — reproducible PCM/codec packet-budget calculator

Run:

```bash
python -m unittest tools.test_pr1_rf_protocol -v
python tools/rf_packet_budget.py
```

## Audio strategy

We are **not locking the final codec yet**.

Order of proof:

1. deterministic small RF packets
2. raw known bytes + fragmentation/reassembly
3. stored audio playback without RF
4. low-complexity coded stored audio over RF
5. higher-quality codec comparison
6. controlled range testing after regulatory/test conditions are confirmed

A 16 kHz mono IMA-ADPCM 10 ms block is currently a **PoC candidate**, because it
can reduce a 320-byte raw PCM frame to roughly 80-84 bytes and may fit in one
FLRC-sized RF packet with the PR1 header. It is not a promise of final music
quality.

## First task when the boards arrive

**Do not start by debugging audio.**

1. verify the exact board/radio revision
2. confirm both units are `SX1280 Without PA`
3. flash the official LILYGO SX1280 PingPong example
4. prove deterministic short-packet RF exchange
5. then introduce the PR1 packet format

See [`docs/TEST_PLAN_V2.md`](docs/TEST_PLAN_V2.md).

## Legal / production boundary

A successful development-board PoC is not evidence of Korean product
certification or permission for arbitrary open-air RF operation. The exact
mode, power, antenna, bandwidth and equipment status must be checked before
outdoor tests. Final sale hardware must follow the applicable Korean conformity
and safety procedures after the production design is frozen.

## Project rule

**Measure one gate before opening the next one.**

Do not redesign the enclosure, app, brand experience, or mass-production board
while the fundamental RF + audio link is still unverified.
