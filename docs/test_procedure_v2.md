# Test procedure — Prototype v2

## 1. Build-only validation

```bash
python -m pip install platformio
pio run -e tx
pio run -e rx
python -m py_compile host/pc_audio_sender.py
g++ -std=c++17 -Wall -Wextra -Werror -Iinclude tests/protocol_test.cpp -o protocol_test
./protocol_test
```

## 2. Flash

```bash
pio run -e tx -t upload --upload-port COM_TX
pio run -e rx -t upload --upload-port COM_RX
```

Use actual serial port names. Connect an SX1280 antenna to both boards before running the radio firmware.

## 3. Receiver electrical test

1. Wire MAX98357A according to `docs/wiring_v2.md`.
2. Keep the transducer away from the head.
3. Power the receiver and monitor at 115200 baud.
4. Confirm `[PR1RX] READY`.
5. Check that the amplifier and transducer remain cool with no audio.

## 4. PC sender

```bash
python -m venv .venv
.venv\Scripts\activate
python -m pip install -r host/requirements.txt
python host/pc_audio_sender.py --list-devices
python host/pc_audio_sender.py --port COM_TX --volume 0.20
```

Start PC music after the sender is running. Use `--device` when the default playback loopback is not selected correctly.

## 5. Acceptance runs

### Bench audio

- Distance: 1 m
- Duration: 2 minutes
- Expected: recognizable audio, no reset, no persistent queue overflow

### Room test

- Distance: 5 m with normal indoor obstacles
- Duration: 10 minutes
- Record TX sent/error counts and RX received/invalid/queue-drop/missing/late counts.

### Reconnection

- Restart TX while RX remains powered.
- Restart RX while TX remains powered.
- Confirm playback resumes after restarting the PC sender when necessary.

## Pass threshold for the first PoC

- Recognizable continuous audio
- Zero unexpected board resets
- Zero invalid-format packets
- No sustained receiver queue overflow
- Comfortable output at the default limiter
- Total purchased cost at or below KRW 150,000

Audio quality and long-range performance are measured, not assumed.
