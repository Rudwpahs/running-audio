# M0 Validation Record

Date: 2026-07-23

This file records what has actually been validated for the first SX1280 M0
firmware. Do not convert a planned test into a completed claim.

## Completed before the PlatformIO CI run

- Canonical application-packet CI on the PR head passed:
  - Python 10,000 packet round trips
  - portable C++ 10,000 packet round trips
  - invalid-input cases
  - Python tool compilation
- RF transport arithmetic was independently checked:
  - M0: 90 B deterministic payload + 10 B transport header = 100 B RF packet
  - voice v0 application packet: 16 B app header + 160 B audio = 176 B
  - 176 B -> 1 fragment with 255 B RF ceiling
  - 176 B -> 2 fragments with 127 B RF ceiling
  - historical 656 B application packet -> 3 fragments at 255 B / 6 at 127 B
- Portable RF-transport C++ logic was compiled with strict host flags during the
  work session.
- RadioLib's current SX1280 API documentation was checked for:
  - `beginFLRC`
  - binary `transmit`
  - binary `receive`
  - `setRfSwitchPins(rxEn, txEn)`

## PlatformIO safe-build gate

Status at creation of this record: **PENDING CI**

The temporary PR workflow is intended to run:

```bash
cd firmware/m0_sx1280_packet_link
pio run -e safe
```

The safe environment defines `PR1_ENABLE_RF_TEST=0`, so a successful build does
not authorize or initiate RF transmission.

After the CI result is known, update this record with the exact result/run and
restore the repository workflow to match `main` so this implementation PR does
not keep an unrelated workflow conflict.

## Still requires physical hardware

Even after a successful PlatformIO compile, these remain unvalidated:

- exact delivered board revision and radio variant
- flash/boot on the physical boards
- vendor PingPong baseline
- SPI/radio initialization on the actual board
- RF packet exchange
- RSSI/SNR behavior
- MAX98357A playback
- prerecorded voice over RF
- 20 m LOS T1 result
- any open-air regulatory configuration
