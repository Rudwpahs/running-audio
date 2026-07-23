# Windows 실제 보드 시험 절차

## 0. 사전 조건

- T3-S3 MVSR H594-01 두 대
- 두 보드에 연결된 2.4 GHz 안테나
- 데이터 전송이 되는 USB 케이블 두 개
- Windows PowerShell
- 첫 출력은 온보드 스피커

안테나가 연결되지 않았으면 플래시 이후의 무선 실행 단계로 넘어가지 않습니다.

## 1. 개발 환경 설치

저장소 루트에서 실행합니다.

```powershell
py -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install platformio==6.1.18
python -m pip install -r host\requirements.txt
```

## 2. 자동검증

```powershell
pio run -e tx
pio run -e rx
python -m py_compile host\pr1_wire.py host\pc_audio_sender.py
python -m unittest discover -s tests -p "test_*.py" -v
```

TX와 RX가 모두 `SUCCESS`, Python 시험이 `OK`여야 합니다.

## 3. COM 포트 구분

```powershell
pio device list
```

보드를 하나씩 뺐다 꽂아 각 COM 번호를 기록합니다.

```text
TX 포트: COM____
RX 포트: COM____
```

## 4. 펌웨어 플래시

아래의 `COM_TX`, `COM_RX`를 실제 번호로 바꿉니다.

```powershell
pio run -e tx -t upload --upload-port COM_TX
pio run -e rx -t upload --upload-port COM_RX
```

업로드 포트가 잡히지 않으면 BOOT 버튼을 누른 상태에서 RESET을 한 번 누른 뒤
BOOT을 놓고 다시 시도합니다.

## 5. 수신기 단독 확인

```powershell
pio device monitor --port COM_RX --baud 115200
```

다음 로그가 나와야 합니다.

```text
[PR1RX] READY protocol=3 onboard_amp=enabled
```

`beginFLRC failed`, `setCRC failed`, `startReceive failed`가 나오면 다음 단계로
넘어가지 않습니다.

## 6. 440 Hz 테스트 톤

수신기 monitor 창은 그대로 두고 PowerShell 창을 하나 더 엽니다.

```powershell
.\.venv\Scripts\Activate.ps1
python host\pc_audio_sender.py --port COM_TX --tone-hz 440 --volume 0.10
```

정상일 때 대략 다음 증가율이 보입니다.

- sender 시작 시 새 `Stream ID: 0x........`가 1회 출력
- TX `frames`: 초당 약 100 증가
- TX `fragments`: 초당 약 300 증가
- RX `frames`: 초당 약 100 증가
- 안정 상태의 `rf_errors`, `invalid`, `crc_fail`, `queue_drop`: 0

처음에는 스피커를 귀에서 떨어뜨리고 톤이 식별되는지만 확인합니다.

## 7. Windows 재생음

장치 목록을 먼저 확인합니다.

```powershell
python host\pc_audio_sender.py --list-devices
```

기본 재생 loopback이 잡히면 다음을 실행합니다.

```powershell
python host\pc_audio_sender.py --port COM_TX --volume 0.10
```

기본 장치가 아니면 장치 이름 일부를 지정합니다.

```powershell
python host\pc_audio_sender.py --port COM_TX --device "Speakers" --volume 0.10
```

## 8. 단계별 합격 시험

### A. 책상 위 1 m

- 시간: 2분
- 출력: 온보드 스피커
- 확인: 식별 가능한 오디오, 리셋 0, 지속 queue overflow 0

### B. 실내 5 m

- 시간: 10분
- 일반적인 실내 장애물 포함
- 시작·종료 TX/RX 로그와 RSSI 기록

### C. 재시작 복구

1. RX를 켠 상태에서 TX 재시작
2. TX를 켠 상태에서 RX 재시작
3. PC sender 재실행
4. sender의 stream ID가 이전 실행과 달라졌는지 확인
5. RX `stream_changes`가 증가하고 오디오가 복귀하는지 기록

## 9. 시험 기록

```text
날짜/장소:
보드 표기 TX:
보드 표기 RX:
TX COM / RX COM:
거리 / 시간:
TX frames / fragments / rf_errors:
RX fragments / frames / invalid / crc_fail:
RX duplicate / incomplete / queue_drop / late / underrun:
RSSI 범위:
리셋 횟수:
소리 식별 여부:
발열·왜곡:
최종 PASS / FAIL:
```

실내 10분 시험을 통과하기 전에는 야외 범위 시험으로 넘어가지 않습니다.
