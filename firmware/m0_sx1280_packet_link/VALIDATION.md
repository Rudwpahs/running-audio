# M0 Validation Record

Date: 2026-07-23

This file records what has actually been validated for the first SX1280 M0
firmware. Do not convert a planned test into a completed claim.

## Completed

### Canonical application-packet CI

PR workflow run **#65** completed successfully on the integration work:

- Python tool compilation
- 10,000 Python application-packet round trips
- buyer-interview template analyzer smoke test
- RF-log template analyzer smoke test
- portable C++ application-packet codec compile
- 10,000 C++ application-packet round trips and invalid-input cases

### RF transport design checks

The lower RF-transport layer was independently checked during the work session:

- M0: 90 B deterministic payload + 10 B transport header = 100 B RF packet
- voice v0: 16 B application header + 160 B audio = 176 B application packet
- 176 B -> 1 fragment with a 255 B RF payload ceiling
- 176 B -> 2 fragments with a 127 B RF payload ceiling
- historical 656 B application packet -> 3 fragments at 255 B / 6 at 127 B
- duplicate completed frames are suppressed by bounded reassembly history
- TX/RX controlled-run loss calculation was independently exercised

The portable RF-transport C++ header/test logic was also compiled with:

```text
-std=c++17 -Wall -Wextra -Werror -pedantic
```

The final regression basis uses the **full 656-byte historical application
packet** (640 B old raw PCM + 16 B application header), not 640 B alone.

### RadioLib API review

Current RadioLib SX1280 documentation was checked for the calls used by M0:

- `beginFLRC`
- binary `transmit`
- binary `receive`
- `setRfSwitchPins(rxEn, txEn)`

The positional-argument `beginFLRC(...)` overload used by M0 currently exists,
but RadioLib documents that overload as deprecated in favor of the
configuration-structure form. A later cleanup should migrate to `ConfigFLRC_t`
after the first safe build is established; this is not a reason to alter the
radio experiment before measurement.

## PlatformIO safe-build gate

Status: **NOT YET EXECUTED — TOOLCHAIN UNAVAILABLE IN THE AGENT ENVIRONMENT**

Two attempts were made to obtain this validation without physical hardware:

1. A temporary GitHub Actions workflow was prepared with:

   ```bash
   cd firmware/m0_sx1280_packet_link
   pio run -e safe
   ```

   The connector/API-authored workflow changes did not produce a new runnable
   PR workflow execution, so no build result was claimed from that attempt.

2. PlatformIO installation was attempted directly in the agent container with
   `python -m pip install platformio`. The container's available internal Python
   package index did not provide a PlatformIO distribution, so the toolchain
   could not be installed there.

The repository workflow was restored to match `main` after the temporary
validation attempt so this PR does not retain a CI/workflow conflict.

The remaining software gate is therefore explicit:

```bash
cd firmware/m0_sx1280_packet_link
pio run -e safe
```

A successful `safe` build only proves compilation. The safe environment defines
`PR1_ENABLE_RF_TEST=0`, so it does not authorize or initiate RF transmission.

## Still requires physical hardware

Even after a successful PlatformIO compile, these remain unvalidated:

- exact delivered board revision and radio variant
- flash/boot on the physical boards
- vendor PingPong baseline
- SPI/radio initialization on the actual board
- RF packet exchange
- RSSI/SNR behavior
- MAX98357A playback
- prerecorded raw voice v0 over RF
- 20 m LOS T1 result
- any open-air regulatory configuration
