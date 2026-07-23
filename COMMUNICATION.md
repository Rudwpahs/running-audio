# PR1 작업 상태

## 2026-07-23 — Codex prototype v3

- 목적: pre-study가 아닌 실제 T3-S3 MVSR 두 대용 펌웨어 완성
- 이전 Draft PR #3의 252-byte FLRC 오류 확인
- Semtech 127-byte 제한에 맞춘 3-fragment protocol 구현
- LILYGO 공식 MVSR amp/RF switch 핀 반영
- PC test tone·loopback sender 구현
- CRC·중복·누락·순서변경·sequence wrap 시험 구현
- sender 재시작용 stream ID와 RX 재동기화 시험 구현
- SX1280 고트래픽 정오표에 따라 single RX 재시작 모드 적용
- TX/RX PlatformIO build PASS
- WAV lossy-link end-to-end PASS

다음 사람 작업은 새 기능 추가가 아니라
`docs/test_procedure_v3.md`에 따른 두 보드 플래시와 실측입니다.
