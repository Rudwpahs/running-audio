# PR1 — Phone ↔ Receiver Exchange Audio System

PR1은 이어폰을 하나 더 만드는 프로젝트가 아닙니다. 핵심은 **휴대폰을 잠시 손에서 떼어놓고도 필요한 소리는 계속 들을 수 있게 만드는 교환 시스템**입니다.

사용자는 스터디카페나 공원의 PR1 station에 휴대폰을 맡기고 작은 receiver를 받아 사용합니다. 끝나면 receiver를 반납하고 자기 휴대폰을 돌려받습니다.

```text
phone in → receiver out → audio only → receiver back → phone back
```

## 왜 만들고 있나

공부하거나 걸을 때 음악, 백색소음, 강의, 타이머 같은 소리는 필요할 수 있습니다. 그런데 기존 이어폰은 소리를 들을 수 있게 해주는 대신 휴대폰도 계속 손 닿는 곳에 남겨 둡니다.

PR1은 이 문제를 소프트웨어 차단이 아니라 **물리적으로 화면과 거리를 만드는 방식**으로 풀어보는 실험입니다.

## 운영 알고리즘

PR1에서 가장 중요한 알고리즘은 무선 기술보다 먼저 **교환과 매칭**입니다.

```text
사용자 도착
   ↓
휴대폰을 station에 맡김
   ↓
휴대폰 ↔ receiver 식별자를 한 쌍으로 등록
   ↓
해당 receiver를 사용자에게 전달
   ↓
사용 중에는 audio만 receiver로 전달
   ↓
receiver 반납
   ↓
등록된 pair 검증
   ↓
맞는 휴대폰 반환
```

여기서 매칭이 틀리면 제품 전체가 성립하지 않기 때문에, 실제 서비스 단계에서는 음질보다 먼저 **누구의 휴대폰과 어떤 receiver가 연결돼 있는지 안전하게 유지하는 것**이 핵심입니다.

## 오디오 처리 흐름

현재 기술 PoC는 다음 구조를 검증합니다.

```text
phone audio
   ↓
transmitter가 audio 입력 수신
   ↓
전송 가능한 packet / stream으로 변환
   ↓
무선 링크로 receiver에 전달
   ↓
receiver에서 audio 복원
   ↓
사용자에게 출력
```

ESP32-S3, SX1280, audio module은 이 흐름을 확인하기 위한 초기 PoC 도구이며 최종 제품 부품으로 확정한 것이 아닙니다.

## PR1이 아닌 것

- AirPods / Buds / Shokz 대체 이어폰
- MP3 player
- 일반 야외 스피커
- tour-guide radio를 그대로 옮긴 제품
- 앱 사용시간을 알려주는 digital wellness app

설명할 때 `작은 무선 오디오 기기`부터 시작하면 기존 제품과 차이가 사라집니다. 항상 **phone deposit + receiver handoff**부터 설명합니다.

## 우선 검증할 것

1. 사용자가 실제로 휴대폰을 맡길 의향이 있는가?
2. 화면 없이 audio만 남겨도 사용 가치가 있는가?
3. 휴대폰과 receiver를 반복 운영하면서 안전하게 매칭할 수 있는가?
4. receiver를 충분히 작고 안정적이며 저렴하게 만들 수 있는가?

이 네 가지가 확인되기 전에는 최종 산업 디자인이나 대규모 구매를 먼저 하지 않습니다.

## 문서

### Product / business

- `docs/PR1_CANONICAL_POSITIONING.md`
- `docs/WEBSITE_COPY_KR.md`
- `docs/BUSINESS_PLAN_V0.md`
- `docs/MARKET_VALIDATION.md`
- `docs/PILOT_ONE_PAGER_KR.md`

### Technical

- `docs/TECHNICAL_MVP.md`
- `docs/HARDWARE_ROLE_MATRIX.md`
- `docs/ARCHITECTURE_OPTIONS.md`
- `docs/REGULATORY_GATE.md`

현재 제품 원칙은 한 문장으로 정리하면 이렇습니다.

> 이어폰을 만드는 게 아니라, 휴대폰과 거리를 만드는 교환 시스템을 만든다.
