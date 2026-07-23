# PR1 Korea Regulatory Gate

> 이 문서는 법률 의견서가 아니라 개발 단계에서 빠뜨리지 않기 위한 체크리스트다. 실제 판매 전에는 국립전파연구원(RRA) 또는 지정시험기관에 제품 구성으로 확인한다.

## 1. 판매 단계의 기본 원칙

국내에서 방송통신기자재를 제조·판매하거나 수입하려는 경우 전파법 제58조의2에 따른 해당 적합성평가를 받아야 한다.

RRA는 적합성평가를 다음 유형으로 안내한다.
- 적합인증
- 적합등록
- 자기적합확인
- 잠정인증

PR1 완제품이 어느 유형에 해당하는지는 최종 RF module, antenna, enclosure, power, interfaces, product category를 기준으로 사전 확인한다.

Official source:
https://www.rra.go.kr/ko/license/A_a_about.do

---

## 2. 연구·개발용과 판매용을 구분한다

RRA 안내상 적합성평가 면제 범위에는 예를 들어 다음이 포함된다.
- 적합성평가 시험/품질·성능 검사 목적 기자재: 10대 이하
- 연구 및 기술개발 목적 기자재: 1,500대 이하
- 판매 목적이 아닌 국내 시장조사용 견본품: 3대 이하
- 개인 사용 목적 반입: 1대(반입 후 1년 내 판매 제한 조건 안내)

수입 시 면제 확인이 필요한 경우 관세청 UNI-PASS를 통한 절차가 안내되어 있다.

Official source:
https://www.rra.go.kr/ko/popup/popup_100430.jsp

### PR1에 적용하는 원칙
현재 프로토타입/연구 목적과, 나중에 고객에게 판매·유통하는 완제품을 같은 단계로 취급하지 않는다.

**연구용으로 면제 가능하다는 사실은 판매용 완제품 인증이 필요 없다는 뜻이 아니다.**

---

## 3. Prototype 단계 체크

- [ ] 제품/모듈 수입 목적 기록
- [ ] 정확한 radio version과 antenna configuration 기록
- [ ] output power 설정값 로그
- [ ] 시험 장소/목적 기록
- [ ] 판매·양도하지 않는 prototype inventory 분리
- [ ] 국내 실외 시험 전 사용 주파수/출력/기술기준 재확인

---

## 4. Productization 단계 체크

T3/T5 기술 gate와 buyer validation을 통과한 뒤 지정시험기관에 다음 자료를 들고 사전상담한다.

- block diagram
- schematic/BOM
- radio IC/module datasheet
- antenna type/gain
- frequency/channel plan
- maximum output power
- power supply/battery
- enclosure drawing
- intended use
- transmitter/receiver 구성
- Bluetooth/Wi-Fi를 켤지 여부
- charging interface

질문:
1. PR1 송신기/수신기의 각 적합성평가 분류는?
2. 기존 인증 module 사용 시 완제품에서 다시 필요한 시험은?
3. antenna 변경 시 영향은?
4. ESP32 Wi-Fi/Bluetooth 기능을 firmware에서 사용하지 않는 경우 평가 범위는?
5. 2.4GHz SX1280 사용 mode/power에 적용되는 기술기준은?
6. 사용자 착용 수신기의 전자파/안전 관련 추가 평가가 있는가?

---

## 5. 인증 비용을 너무 일찍 쓰지 않는다

인증 상담은 일찍 하되, 본 시험/양산비 지출은 다음 이후로 미룬다.

- buyer 문제 검증
- technical T3/T5 통과
- final radio architecture 후보 축소
- enclosure/antenna 구조가 크게 바뀌지 않는 시점

그 전에는 설계가 바뀔 가능성이 높아 재시험 비용 위험이 있다.

---

## 6. 공식 문의 기준점

국립전파연구원 홈페이지는 현재 무선기기·복합기기·적합성평가면제 관련 민원 연락처를 별도로 안내하고 있다.

Official:
https://www.rra.go.kr/ko/index2.do

판매 준비 단계에서는 인터넷 게시글보다 RRA/지정시험기관의 제품별 답변을 최종 근거로 보관한다.
