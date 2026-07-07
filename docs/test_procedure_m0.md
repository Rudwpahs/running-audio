# M0 Test Procedure

Prerequisites: two ESP32-S3-DevKitC-1-N8R8 boards, ESP-IDF v6.0.2, two USB data cables, and two serial terminals.

## Build

For each project, run `idf.py set-target esp32s3`, then `idf.py build`, then flash and monitor the appropriate serial port. Build `m0_tx` for the transmitter and `m0_rx` for the receiver. Start the transmitter before the receiver.

## Expected behavior

- The transmitter creates `PR1_AUDIO_LINK` on channel 6.
- The receiver connects with static address `192.168.4.2`.
- The transmitter sends 656-byte UDP datagrams to port 40100 at 100 packets per second.
- The receiver prints one statistics line each second.

## Acceptance test

Run both boards for 600 seconds at approximately 5 m clear-line distance while capturing both serial logs.

Pass criteria: zero Wi-Fi disconnects, zero payload errors, zero header or size errors, cumulative loss no greater than 0.1%, and zero unexpected resets.

Additional checks: restart each board separately and temporarily move the receiver out of range. The link should recover, and reconnect attempts should occur at roughly one-second intervals.

Do not claim build or physical-test success until those steps have actually been performed.
