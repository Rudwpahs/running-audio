# PR1 AI Handoff — Current State

Date: 2026-07-23

> **TECHNICAL SPEC RESET:** The former Wi-Fi SoftAP/UDP `LOCKED SPECIFICATIONS`
> in this file are obsolete and must not be implemented as current PR1.
>
> Current technical source of truth:
> - `docs/ARCHITECTURE_V2.md`
> - `docs/TEST_PLAN_V2.md`
>
> The original handoff specification remains available in Git history, including
> commit `f9028af0eabbc6a91bd794c94d0ca7ad7c1614b1`.

## Why the reset happened

The old handoff was created before the first-prototype hardware direction was
finalized. It locked an ESP32-S3 Wi-Fi/UDP path with a 640-byte raw PCM payload.
The actual first PoC is now based on two LILYGO T3-S3 MVSR boards with the
`SX1280 2.4G Without PA + MVSR [H594-01]` option.

A 640-byte 10 ms PCM frame cannot fit in one SX1280 RF packet, so packet
fragmentation and audio compression must be designed explicitly.

## Current ownership

GPT / Codex-style work session: architecture reset, protocol design, PC unit
checks, hardware test plan, regulatory/productization gates.

## Current completed work

- current PoC hardware restored as the repository source of truth
- obsolete Wi-Fi architecture explicitly retired
- 10-byte PR1 RF application header designed
- fragmentation and bounded reassembly helper added
- unit tests added for header, FLRC/GFSK sizing, duplicate and out-of-order
  fragments
- packet-budget calculator added
- staged board-arrival and RF/audio test plan added

## Current hard facts

- No physical SX1280 hardware test has been performed in this repository yet.
- No ESP32 firmware build has been performed in this architecture-reset branch.
- No outdoor RF test is claimed.
- No final codec is selected.
- No final production RF module/PCB is selected.
- The LILYGO board is a PoC platform, not a locked production design.

## Current working hypothesis

For the first continuous-audio link experiment, test a 16 kHz mono 10 ms
IMA-ADPCM block as a low-complexity PoC candidate. Its purpose is to fit a coded
frame inside one FLRC-sized packet budget and simplify RF-link validation.

This is **not** a final music-quality decision. Higher-quality codec experiments
come only after the packet link is stable and measured.

## Next task after hardware arrival

1. identify both exact board revisions and confirm `SX1280 Without PA`
2. run the unmodified official LILYGO SX1280 PingPong example
3. record actual radio settings and baseline packet exchange
4. introduce deterministic PR1 v2 packets
5. test fragmentation/reassembly
6. test stored audio output separately
7. join stored audio -> codec -> RF -> receiver -> playback

Do not start live microphone work first.

## Review rules for another AI

Before proposing changes:

1. read `docs/ARCHITECTURE_V2.md`
2. read `docs/TEST_PLAN_V2.md`
3. inspect the actual selected board/revision instead of assuming old ESP32 pin
   assignments
4. distinguish PC tests, firmware builds, bench RF tests and physical audio tests
5. never claim hardware success without a human-supplied physical result
6. never change the product architecture solely to make existing code easier to
   reuse

If a specification is contradicted by measured hardware behavior or official
radio documentation, report the conflict and evidence before changing it.
