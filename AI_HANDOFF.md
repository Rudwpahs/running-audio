# PR1 AI Handoff — 실제 프로토타입 v3

## 현재 기준

이 저장소의 실제 보드 구현 기준은 protocol v3입니다. `main`에 남아 있는
Wi-Fi/ESP-IDF v1 잠금 사양과 8-bit v0 시뮬레이터는 과거 조사·비교 자료이며,
현재 H594-01 펌웨어의 구현 기준이 아닙니다. 이전 Draft PR #3의 252바이트 RF
패킷, GPIO43/44/21 외장 I2S 배선, RF switch 미설정도 실제 보드에서 사용할 수
없는 폐기 사양입니다. 변경 전 기록은 Git 이력에 보존되어 있습니다.

## 잠금 사양

- Board: 2 × T3-S3 MVSR H594-01, SX1280 without PA
- Audio: 16 kHz, mono, signed PCM16, 160 samples / 10 ms
- USB: 20-byte header + 320-byte PCM, stream ID, CRC32
- RF: FLRC 650 kbps, CR 3/4, 3 dBm, 2410.5 MHz
- Sync word: `0x50525031` (`PRP1`), CR 3/4 errata 범위 밖
- RF fragments: 127, 127, 123 bytes
- RX mode: single receive (`SetRx(0)`) 재시작; continuous RX 금지
- Restart: sender마다 새 uint32 stream ID, RX 큐·sequence 재동기화
- MVSR amp: BCLK40, LRCLK41, DATA39, SD_MODE38
- RF switch: RX21, TX10
- First output: onboard speaker

## 구현 파일

- `include/pr1_config.h`: 하드웨어·오디오 상수
- `include/pr1_protocol.h`: CRC, USB parser, RF codec, reassembler
- `host/pr1_wire.py`: Python USB encoder
- `host/pc_audio_sender.py`: loopback 및 440 Hz tone sender
- `src/tx_role.cpp`: USB → 3 RF fragments
- `src/rx_role.cpp`: RF reassembly → timed I2S
- `tests/protocol_test.cpp`: 단위·오류 시험
- `tests/e2e_pipeline_test.cpp`: WAV 기반 손실 링크 종단간 시험

## 현재 검증

- TX build: PASS
- RX build: PASS
- Python wire tests: PASS
- Native C++ protocol tests: PASS
- WAV end-to-end lossy-link test: PASS
- Physical RF/audio: 아직 미실시

물리 시험 전에는 실제 거리·음질·연속 재생 성공을 주장하지 않습니다.
