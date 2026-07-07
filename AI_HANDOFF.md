# PR1 AI Handoff Log

> Shared coordination file for GPT and Claude working on the `running-audio` project.
>
> Repository: `Rudwpahs/running-audio`
>
> **Important:** This file is the source of truth for AI-to-AI handoff. Before starting work, read the entire `CURRENT STATE` and the latest message. After finishing work, update this file in the same commit or immediately afterward.

---

## 1. Roles

### GPT — Lead architect and final reviewer

- Owns product definition, architecture, hardware choices, protocol, pin assignments, acceptance criteria, and final approval.
- Produces the first implementation draft, documentation, and test plan.
- Reviews Claude's findings and decides whether any specification change is accepted.

### Claude — Focused implementation reviewer

- Reviews GPT's implementation for compile errors, API misuse, race conditions, buffer errors, and logic defects.
- Returns minimal patches instead of rewriting entire files.
- Must not independently change hardware, transport, packet format, audio format, or pins.
- If a fixed specification appears impossible, report the conflict and alternatives without changing it.

---

## 2. Collaboration Rules

1. Read this file before every task.
2. Treat `LOCKED SPECIFICATIONS` as immutable unless GPT records an approved change.
3. Work on only the task marked `IN PROGRESS`.
4. Do not silently broaden scope.
5. Do not replace existing files wholesale when a minimal diff is sufficient.
6. Record assumptions, files changed, tests run, and unresolved risks.
7. Append messages in chronological order. Do not delete earlier messages.
8. Use UTC+09:00 dates in `YYYY-MM-DD HH:mm KST` format.
9. Every message must have a unique ID:
   - GPT: `GPT-YYYYMMDD-NN`
   - Claude: `CLAUDE-YYYYMMDD-NN`
10. When responding to another message, include its ID in `Reply to`.
11. Neither AI should claim a physical hardware test unless a human actually ran it and supplied the result.
12. Build success may be claimed only when the build command was actually executed in an ESP-IDF environment.

---

## 3. Message Template

```md
### [MESSAGE-ID] Sender -> Recipient

- Date: YYYY-MM-DD HH:mm KST
- Reply to: MESSAGE-ID or `none`
- Status: REQUEST | IN_PROGRESS | DONE | BLOCKED | REVIEW
- Scope: exact files or subsystem

#### Goal
What must be achieved.

#### Fixed constraints
Only constraints relevant to this task.

#### Work completed
What was changed or checked.

#### Evidence
Build commands, test output, logs, datasheet/API references, or `not run`.

#### Files changed
- `path/to/file`

#### Findings / risks
Concrete defects, uncertainties, or compatibility issues.

#### Requested next action
One specific action for the recipient.
```

---

## 4. Locked Specifications — v1.0

Changes require an explicit GPT approval message in this log.

### Hardware

- TX board: `ESP32-S3-DevKitC-1-N8R8`
- RX board: `ESP32-S3-DevKitC-1-N8R8`
- TX audio ADC: PCM1808 module with exposed SCKI, BCK, LRCK, DOUT and configurable MD0/MD1/FMT
- RX amplifier: MAX98357A

### Development environment

- Framework: ESP-IDF
- Version target: `v6.0.2`
- Language: C
- Target: `esp32s3`
- No Arduino framework
- No third-party libraries for M0/M1

### Network

- TX mode: Wi-Fi SoftAP
- RX mode: Wi-Fi Station
- SSID: `PR1_AUDIO_LINK`
- Password: `PR1audio2026`
- Channel: 6
- Bandwidth: HT20
- TX IP: `192.168.4.1`
- RX IP: `192.168.4.2`
- Netmask: `255.255.255.0`
- UDP destination: `192.168.4.2:40100`
- Transport: UDP unicast
- Wi-Fi power save: disabled
- No TCP, RTP, ESP-NOW, ACK/retry protocol, or broadcast in M0/M1

### Packet format

- Packet interval: 10 ms
- Packets per second: 100
- Header: 16 bytes
- Payload: 640 bytes
- Total UDP payload: 656 bytes
- Header multi-byte fields: network byte order
- Magic: ASCII `PR1A`
- Version: `0x01`
- Flags: M0 `0x01`, M1 `0x02`
- Payload length: 640
- Sequence: uint32, starts at 0, increments by 1
- Sample index: uint32, starts at 0, increments by 320

Header offsets:

| Offset | Size | Field |
|---:|---:|---|
| 0 | 4 | Magic |
| 4 | 1 | Version |
| 5 | 1 | Flags |
| 6 | 2 | Payload length |
| 8 | 4 | Sequence |
| 12 | 4 | Sample index |
| 16 | 640 | Payload |

### M0 payload

`payload[i] = (uint8_t)((sequence + i) & 0xFF)`

### M1 audio

- PCM: signed 16-bit little-endian
- Sample rate: 32,000 Hz
- Channels over network: mono
- Samples per packet: 320
- Audio duration per packet: 10 ms
- Compression: none
- Receiver target jitter depth: 40 ms / 4 packets
- Jitter ring capacity: 16 packets

### TX PCM1808 pins

- I2S port: `I2S_NUM_0`, RX master
- MCLK: GPIO4 -> SCKI
- BCLK: GPIO5 -> BCK
- WS/LRCLK: GPIO6 -> LRCK
- DATA IN: GPIO7 <- DOUT
- LRCLK: 32 kHz
- BCLK: 2.048 MHz
- MCLK: 8.192 MHz / 256 fs
- PCM1808 mode: slave, Philips I2S, 24-bit stereo, 64 BCLK/frame

### RX MAX98357A pins

- I2S port: `I2S_NUM_0`, TX master
- BCLK: GPIO15
- WS/LRCLK: GPIO16
- DATA OUT: GPIO17
- No MCLK
- Network mono sample duplicated into both left and right I2S slots

### M1 initial source

- Default source: generated 1 kHz sine wave
- Sample rate: 32 kHz
- Amplitude: 8192
- PCM1808 capture is enabled only after sine-path validation

---

## 5. Milestones

| Milestone | Description | State |
|---|---|---|
| M0-A | Repository structure and common protocol component | NOT_STARTED |
| M0-B | UDP pattern transmitter | NOT_STARTED |
| M0-C | UDP validator receiver and statistics | NOT_STARTED |
| M0-D | ESP-IDF build verification | NOT_STARTED |
| M0-E | 600-second physical link test | BLOCKED_HARDWARE |
| M1-A | 1 kHz sine -> UDP -> MAX98357A | NOT_STARTED |
| M1-B | PCM1808 capture -> UDP -> MAX98357A | NOT_STARTED |

---

## 6. Current State

- Repository was empty when this coordination file was created.
- Architecture and M0/M1 numerical specifications are locked in section 4.
- No firmware source exists yet.
- No ESP-IDF build has been run.
- No physical hardware result exists.

### Current owner

`GPT`

### Current task

Create the first complete M0 implementation draft:

- `components/pr1_protocol/`
- `m0_tx/`
- `m0_rx/`
- root documentation and M0 test procedure

Claude should not start code generation yet. Claude's first action is to read the resulting M0 files and perform a focused review using the protocol below.

---

## 7. Messages

### GPT-20260707-01 GPT -> Claude

- Date: 2026-07-07 20:00 KST
- Reply to: none
- Status: REQUEST
- Scope: collaboration protocol and upcoming M0 review

#### Goal
Establish GitHub as the shared handoff channel and prevent duplicate full-project generation.

#### Fixed constraints
All specifications in section 4 are locked. Claude must not change them independently.

#### Work completed
Created `AI_HANDOFF.md` with roles, communication rules, fixed specifications, milestones, and task state.

#### Evidence
Repository metadata confirmed that `Rudwpahs/running-audio` exists, is writable, uses `main`, and was empty before this file was added.

#### Files changed
- `AI_HANDOFF.md`

#### Findings / risks
A Markdown file does not create autonomous real-time communication. Each AI must be explicitly prompted by the user or its environment to fetch the latest repository state. Concurrent edits to this same file can conflict, so only the current task owner should update it during active implementation; reviewers append after fetching the latest version.

#### Requested next action
After GPT commits the M0 implementation, fetch the latest `main`, review only the specified source files, and append a concise review message with minimal diffs or `no critical issue found`. Do not regenerate the project.

---

## 8. Claude Review Output Contract

For each defect:

```md
### Finding N
- File and line:
- Severity: critical | high | medium | low
- Cause:
- Runtime/build symptom:
- Minimal fix:
```

Then provide only a unified diff for changed lines.

If no critical issue is found, write:

`No critical issue found within the requested review scope.`

Do not repeat the full specification or entire source files.
