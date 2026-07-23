# PR1 Technical MVP

## 목표

첫 기술 목표는 고음질 음악이 아니라 **야외에서 사람의 안내 음성이 실시간으로 이해 가능하게 전달되는지** 검증하는 것이다.

현재 654-byte PCM 프레임 구조를 그대로 RF packet 하나에 보내는 설계는 폐기한다.

SX1280 LoRa payload 한도는 최대 255 bytes이므로, 최초 voice PoC는 packet 안에 자연스럽게 들어가는 audio frame부터 시작한다.

---

## v0 Audio Profile

### 첫 시험값
- Sample rate: **8 kHz**
- Channels: **mono**
- Sample format: **unsigned PCM 8-bit**
- Frame duration: **20 ms**
- Samples/frame: 160
- Audio bytes/frame: **160 bytes**
- Frames/second: 50
- Raw audio bitrate: **64 kbps**

이 프로파일은 음질 목표가 아니라 **통신/지연/패킷손실/재생 파이프라인을 가장 단순하게 검증하는 진단 프로파일**이다.

### 이유
8 kHz × 1 byte × 0.020 s = 160 bytes

즉, packet header를 추가해도 SX1280의 255-byte payload 아래에 둘 수 있어 첫 단계에서 fragmentation/reassembly가 필요 없다.

### 다음 단계 옵션
raw PCM이 RF margin을 너무 많이 사용하면 다음 순서로 검토한다.
1. IMA ADPCM 4-bit (약 32 kbps)
2. frame duration/packet cadence 조정
3. LoRa high-rate / FLRC / GFSK modulation 비교

처음부터 codec을 넣지 않는다. 먼저 raw PCM으로 병목을 측정한다.

---

## Application Packet v1

초기 제안. 실제 구현 전에 field size를 `static_assert`로 확인한다.

```text
magic          uint16   // PR1 packet 식별
version        uint8
flags          uint8
stream_id      uint16
sequence       uint16   // packet loss 계산
sample_rate    uint16   // 8000
payload_len    uint16
capture_ms     uint32   // 상대 timestamp
payload        bytes    // v0 = 160 bytes PCM
```

고정 header 16 bytes + 160-byte audio = 176 bytes 목표.

### v0에서 넣지 않는 것
- ACK
- retransmission
- per-receiver addressing
- encryption
- fragmentation fields

실시간 broadcast에서 늦게 재전송된 packet은 가치가 낮다. 우선 sequence gap을 기록하고 손실된 audio frame은 silence 또는 간단한 concealment로 넘긴다.

---

# Stage Gates

## T0 — 보드 Bring-up

### 해야 할 것
- USB serial 정상
- board revision 확인
- microSD read/write
- speaker output
- microphone capture
- SX1280 ping-pong

### PASS
각 기능의 독립 example이 최소 10분 동안 오류 없이 동작하고 로그가 저장됨.

---

## T1 — RF Data 1→1

### 시험
- 20m LOS
- 100-byte packet
- 1,000 packets
- sequence number 포함

### 기록
- TX count
- RX count
- packet loss %
- RSSI
- SNR
- selected modulation parameters

### PASS target
packet loss ≤ 2%를 20m LOS에서 달성.

이 수치는 제품 사양이 아니라 다음 audio 단계로 갈지 판단하기 위한 내부 gate다.

---

## T2 — Prerecorded Audio 1→1

### 시험
microSD 또는 memory의 8 kHz / 8-bit mono speech를 20ms frame으로 송신.

### PASS target
- 5분 연속 재생
- receiver crash/reboot 없음
- packet sequence/loss 기록 가능
- 사람이 문장 내용을 이해 가능

---

## T3 — Live Microphone 1→1

### 파이프라인
Digital mic → frame buffer → RF TX → RF RX → jitter buffer → I2S speaker/open-ear output

### 측정
- end-to-end latency
- packet loss
- jitter/buffer underflow
- CPU/memory usage

### PASS target
- 10분 연속 동작
- median end-to-end latency ≤ 250 ms
- 체감상 지도자의 짧은 명령 전달에 방해가 없는 수준

250ms는 최종 제품 보증치가 아니라 초기 개발 gate다.

---

## T4 — Broadcast 1→2, 이후 1→5

ACK 없는 broadcast로 동일 stream을 여러 receiver가 동시에 수신한다.

### PASS target
- receiver 수 증가 때문에 transmitter packet cadence가 변하지 않음
- 각 receiver가 독립적으로 loss를 기록
- 1→2에서 먼저 통과 후 1→5로 확장

실제 보드가 2대뿐이라면 1→2/1→5 단계는 추가 hardware 확보 전 simulator/logical receiver test로 준비하고, 추가 구매는 T3 통과 뒤 결정한다.

---

## T5 — Outdoor Range Matrix

같은 장소·같은 높이·같은 안테나 상태에서 다음 거리 측정.

| Distance | Test | Record |
|---:|---|---|
| 20m | 5 min speech | loss/RSSI/SNR/latency |
| 50m | 5 min speech | loss/RSSI/SNR/latency |
| 100m | 5 min speech | loss/RSSI/SNR/latency |

추가로 사람이 transmitter와 receiver 사이를 통과하는 NLOS/몸차폐 시험을 별도로 한다.

### MVP business test target
50m LOS에서 일반적인 야외 지도 음성이 반복적으로 사용 가능해야 한다.

---

# Modulation Benchmark

SX1280은 LoRa뿐 아니라 FLRC/GFSK도 지원한다. 음성에는 range와 throughput의 trade-off가 있으므로 감으로 고르지 않는다.

최소 비교:

| Mode | Purpose |
|---|---|
| LoRa high bandwidth / low SF | range + robustness 후보 |
| FLRC | higher throughput / lower latency 후보 |
| GFSK | throughput baseline |

각 mode에서 동일한 payload size와 packet cadence를 사용해 20m/50m loss와 latency를 비교한다.

---

# Measurement Log CSV

```csv
run_id,date,location,mode,freq_mhz,bw,sf_or_bitrate,cr,power_dbm,distance_m,tx_packets,rx_packets,loss_pct,rssi_avg,snr_avg,latency_p50_ms,latency_p95_ms,notes
```

Audio subjective test:

```csv
run_id,listener_id,phrase_id,heard_correctly,comfort_1_5,surroundings_audible_1_5,dropout_noticed,notes
```

---

# Technical Kill / Pivot Rules

- T1이 안정화되지 않으면 audio code를 추가하지 않는다.
- raw PCM이 bandwidth 때문에 실패하면 compression 전에 modulation benchmark를 수행한다.
- 50m voice가 반복적으로 실패하면 antenna/config/protocol을 분리 진단한다.
- SX1280에서 목표 UX가 나와도 그것만으로 최종 radio architecture를 확정하지 않는다.
- Auracast/LE Audio와 OEM 방식은 구매자 문제 검증 후 별도 Architecture Review에서 비교한다.

---

# 공식 참고자료

- LILYGO T3-S3 MVSR documentation: https://wiki.lilygo.cc/products/t3-series/t3-s3-mvsr/
- LILYGO LoRa examples: https://github.com/Xinyuan-LilyGO/LilyGo-LoRa-Series
- Semtech SX1280: https://www.semtech.com/products/wireless-rf/lora-connect/sx1280
- Bluetooth Auracast: https://www.bluetooth.com/auracast/
