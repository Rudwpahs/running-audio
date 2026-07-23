# PR1 — Screen-free Group Audio

PR1은 야외 교육·캠프·스포츠 코칭을 위한 **휴대폰 없는 무선 오디오** 프로젝트입니다.
현재 개발 관문은 사전 조사나 제품 완성 단계가 아니라, 보유한 실제 하드웨어 두 대로
PC 오디오가 무선 전송되고 스피커에서 연속 재생되는지 증명하는 1:1 프로토타입입니다.
이 관문을 통과한 뒤에만 1:N 수신, 배터리, 착용 구조와 유료 파일럿을 확장합니다.

```text
Windows 재생음/440 Hz 테스트 톤
  -> USB-C
  -> LILYGO T3-S3 MVSR H594-01 송신기
  -> SX1280 FLRC (2.4 GHz)
  -> LILYGO T3-S3 MVSR H594-01 수신기
  -> 온보드 MAX98357A
  -> 온보드 스피커
  -> 이후 Adafruit 골전도 트랜스듀서 #1674
```

## 실제 프로토타입 v3 상태

| 관문 | 상태 |
|---|---|
| TX 펌웨어 PlatformIO 빌드 | PASS |
| RX 펌웨어 PlatformIO 빌드 | PASS |
| Python USB 프레임·신호 시험 | PASS |
| C++ 프로토콜·CRC·재시작 시험 | PASS |
| WAV → USB → RF 조각 → 손실 링크 → PCM 복원 | PASS |
| 실제 두 보드 플래시·무선 수신 | 부품 연결 후 확인 필요 |
| 온보드 스피커 10분 연속 재생 | 부품 연결 후 확인 필요 |
| 골전도 트랜스듀서 출력 | 온보드 스피커 성공 후 확인 |

자동시험 통과는 실제 RF·스피커 성공을 뜻하지 않습니다. 실제 성공 판정은
[물리 시험 절차](docs/test_procedure_v3.md)를 두 보드에서 완료한 뒤에만 합니다.

## 고정 구현 사양

- 보드: 2 × LILYGO T3-S3 MVSR H594-01, SX1280 without PA
- 오디오: signed PCM16 little-endian, mono, 16 kHz, 10 ms/160 samples
- RF: FLRC 650 kbps, coding rate 3/4, 2410.5 MHz, 3 dBm
- RF 패킷: 19바이트 헤더 + PCM `108/108/104`바이트, 총 `127/127/123`바이트
- 무결성: SX1280 CRC16 + 논리 오디오 프레임 CRC32
- 재시작: 실행별 32비트 stream ID로 sequence 0 복귀와 지터 큐 재동기화
- 출력: 온보드 MAX98357A, 40 ms 시작 버퍼, 누락 프레임은 10 ms 무음

## 이전 구현에서 고친 실제 결함

이전 Draft PR #3의 252바이트 FLRC 패킷은 SX1280 FLRC의 127바이트 한계를
초과했습니다. v3는 320바이트 PCM 프레임을 3개 패킷으로 나눕니다.

또한 다음 하드웨어·정오표 조건을 코드에 반영했습니다.

- 온보드 MAX98357A: BCLK 40, LRCLK 41, DATA 39, SD_MODE 38
- SX1280 RF 스위치: RX 21, TX 10
- FLRC CR 3/4 금지 범위를 피한 sync word `0x50525031` (`PRP1`)
- 초당 300패킷에서 continuous RX 정지를 피하는 `SetRx(0)` 단발 수신 재시작

## 빠른 자동검증

개발 기준은 PlatformIO Core 6.1.x, Espressif32 6.13.0, Arduino framework,
RadioLib 7.7.1과 Python 3.11 이상입니다.

```bash
pio run -e tx
pio run -e rx

python -m py_compile host/pr1_wire.py host/pc_audio_sender.py
python -m unittest discover -s tests -p "test_*.py" -v

g++ -std=c++17 -Wall -Wextra -Werror -pedantic \
  -Iinclude tests/protocol_test.cpp -o protocol_test
./protocol_test

g++ -std=c++17 -Wall -Wextra -Werror -pedantic \
  -Iinclude tests/e2e_pipeline_test.cpp -o e2e_pipeline_test
./e2e_pipeline_test
```

## 실제 보드 시작점

- [프로토타입 범위와 합격 기준](docs/prototype_v3.md)
- [USB·RF 프로토콜 규격](docs/protocol_v3.md)
- [MVSR 연결 기준](docs/wiring_v3.md)
- [Windows 플래시·실물 시험 절차](docs/test_procedure_v3.md)
- [확정 하드웨어 구성](docs/bom_v3.md)

## 제품·파일럿 자료

프로토타입 구현과 별개로 기존 시장·현장 자료는 유지합니다.

- [의사결정 기록](docs/DECISIONS_2026-07-23.md)
- [구매자 검증과 유료 파일럿 계획](docs/MARKET_VALIDATION.md)
- [첫 구매자 후보 목록](docs/BUYER_LEADS_2026-07-23.md)
- [30일 실행 로드맵](docs/30_DAY_EXECUTION.md)
- [현장 파일럿 절차](docs/FIELD_PILOT_PROTOCOL.md)
- [한국 규제 관문](docs/REGULATORY_GATE.md)

**한 번에 한 관문만 통과하고, 실측하기 전에는 범위·음질·안정성을 주장하지 않습니다.**
