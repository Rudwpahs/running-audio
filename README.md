# Running Audio — PR1 Firmware

Firmware experiments for a phone-separated outdoor wireless-audio system.

## Current milestone: M0

M0 validates the network transport before audio hardware is added:

- ESP32-S3 transmitter creates a private Wi-Fi SoftAP
- ESP32-S3 receiver uses a fixed IPv4 address
- UDP unicast at 100 packets per second
- 656-byte application datagrams
- deterministic payload verification
- loss, duplicate, out-of-order, and reconnect statistics

## Layout

- `components/pr1_protocol/`: shared packet encoder, decoder, and validators
- `m0_tx/`: SoftAP UDP pattern transmitter
- `m0_rx/`: static-IP UDP validator receiver
- `docs/`: protocol and test procedure
- `tests/`: host-side protocol test
- `AI_HANDOFF.md`: locked architecture and specifications
- `COMMUNICATION.md`: GPT and Claude collaboration rules

## Toolchain

- Board: `ESP32-S3-DevKitC-1-N8R8`
- ESP-IDF: `v6.0.2`
- Target: `esp32s3`
- Language: C

Each firmware directory is an independent ESP-IDF project. Enter either `m0_tx` or `m0_rx`, run `idf.py set-target esp32s3`, and then run `idf.py build`.

## Validation status

The shared protocol component passed a local host compile and assertion test with `-std=c11 -Wall -Wextra -Werror` during drafting. An ESP-IDF build has not yet been run, and no physical two-board test has been performed.
