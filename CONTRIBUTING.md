# 작업 규칙

- main에 직접 기능 작업을 하지 않습니다.
- 브랜치는 agent, feature, experiment, docs, fix 접두사를 사용합니다.
- 보드, 통신, 코덱, 핀, 전원을 임의로 변경하지 않습니다.
- 기술 변경 시 docs/decision-log.md를 갱신합니다.
- 구현 전에 측정 가능한 완료 기준을 작성합니다.
- 실패 실험도 삭제하지 않고 기록합니다.

## 커밋 예시

- Document M0 packet test criteria
- Add transmitter packet counter
- Record indoor range test

## 실험 기록

모든 실험은 tests/test-log-template.md를 복사해 보드 모델, 커밋 SHA, 전원, 거리, 장애물, 패킷 설정, 지연, 손실, 끊김과 발열을 기록합니다.

## 저장소에 올리지 않는 것

- Wi-Fi 비밀번호, API 키, 장치 비밀키
- 개인 음원, 영상, 개인정보
- 빌드 산출물과 펌웨어 바이너리
- 대용량 캡처 파일

## PR 완료 조건

- 목적과 범위가 설명되어 있음
- 관련 결정 또는 시험 문서가 갱신됨
- 실행과 시험 방법이 적혀 있음
- 미확정 사항을 확정된 것처럼 표현하지 않음
- 다른 프로젝트 파일이 섞이지 않음
