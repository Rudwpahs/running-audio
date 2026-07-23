# PR1 protocol v3

## 1. 오디오 단위

- PCM: signed 16-bit little-endian, mono
- Sample rate: 16,000 Hz
- Frame: 160 samples = 320 bytes = 10 ms
- Logical frame rate: 100 frames/s

## 2. PC → TX USB 프레임

USB 프레임은 고정 340바이트입니다.

| Offset | Size | Field | Encoding |
|---:|---:|---|---|
| 0 | 4 | Magic `PR1U` | bytes |
| 4 | 1 | Version `0x03` | uint8 |
| 5 | 1 | Flags, audio=`0x01` | uint8 |
| 6 | 2 | PCM length=`320` | little-endian |
| 8 | 4 | Stream ID | little-endian |
| 12 | 4 | Frame sequence | little-endian |
| 16 | 4 | PCM CRC32 | little-endian |
| 20 | 320 | PCM | signed 16-bit LE |

TX parser는 임의 잡음 속에서도 `PR1U`를 다시 찾고, version·flags·length·CRC가
모두 맞는 프레임만 RF로 보냅니다.

## 3. TX → RX RF 조각

Semtech SX1280 FLRC의 payload length 범위는 6–127바이트입니다. 각 RF 조각
헤더는 19바이트입니다.

| Offset | Size | Field | Encoding |
|---:|---:|---|---|
| 0 | 2 | Magic `P3` | bytes |
| 2 | 1 | Version `0x03` | uint8 |
| 3 | 1 | Flags, audio=`0x01` | uint8 |
| 4 | 4 | Stream ID | big-endian |
| 8 | 4 | Frame sequence | big-endian |
| 12 | 1 | Fragment index | 0, 1, 2 |
| 13 | 1 | Fragment count | 3 |
| 14 | 1 | Fragment PCM bytes | 108, 108, 104 |
| 15 | 4 | Full-frame PCM CRC32 | big-endian |
| 19 | 가변 | PCM fragment | raw bytes |

실제 패킷 크기는 다음과 같습니다.

| Fragment | Header | PCM | Total |
|---:|---:|---:|---:|
| 0 | 19 | 108 | 127 |
| 1 | 19 | 108 | 127 |
| 2 | 19 | 104 | 123 |

한 논리 프레임당 RF 전송량은 377바이트이며, 초당 100프레임에서 payload 기준
37,700 bytes/s, 301.6 kbps입니다. FLRC 650 kbps·coding rate 3/4를 사용해
헤더·CRC·preamble을 위한 여유를 둡니다. 실제 마진은 두 보드 시험에서
`rf_errors`, `incomplete`, `underrun`으로 판정합니다.

SX1280 정오표는 패킷 트래픽이 초당 약 220개를 넘는 환경에서 continuous RX를
사용하면 수신기가 멈출 수 있다고 경고합니다. PR1은 초당 300조각을 보내므로
RX는 `SetRx(0)`에 해당하는 single RX 모드로 동작하고, 각 수신 완료 후 즉시
다시 single RX를 시작합니다.

## 4. 오류 처리

- SX1280 hardware CRC: 손상된 개별 RF 조각 폐기
- Application CRC32: 잘못 섞이거나 손상된 전체 PCM 프레임 폐기
- Duplicate fragment: 첫 조각만 사용하고 duplicate count 증가
- Missing fragment: 다음 sequence가 오면 불완전 프레임 폐기
- Old fragment: stale/late로 기록하고 폐기
- Missing playout frame: 동일 시간 길이의 무음 10 ms 출력
- Sequence: uint32 modulo arithmetic, `0xFFFFFFFF → 0` 허용
- Stream restart: sender 실행마다 무작위 uint32 stream ID를 만들고, ID가
  바뀌면 RX가 이전 큐와 sequence 기준을 폐기한 뒤 새 스트림으로 시작

재전송은 지연을 늘리므로 첫 실시간 PoC에는 넣지 않습니다.

## 5. FLRC sync-word 안전 조건

CR 3/4의 SX1280 errata는 MSW `0x8C38`, `0x630E`와 LSW
`0x0000..0x3EFF`를 금지합니다. v3의 `0x5052_5031`(`PRP1`)은 이 범위
밖이며, `pr1_config.h`의 compile-time assertion이 잘못된 값으로의 회귀를
차단합니다.
