# PR1 Unit Economics — 2026-07-23

> 모든 가격은 현재 **사업 가설**이다. 인터뷰와 실제 BOM이 나오기 전에는 판매가로 확정하지 않는다.

## 1. 먼저 지켜야 할 수익성 원칙

PR1은 하드웨어 회사처럼 보이지만, 수신기 숫자가 늘수록 재고·AS·배터리·충전·분실 비용도 같이 늘어난다.

따라서 매출보다 다음 세 지표를 먼저 본다.

1. Hardware gross margin ≥ **40%**
2. Rental kit payback ≤ **8~12 paid rentals**
3. 2년 차부터 maintenance / replacement / software 등 반복매출 생성

---

# 2. 10-person Starter Kit — Target Cost Model

## 목표 구성
- transmitter/controller × 1
- receiver/open-ear unit × 10
- charging/storage solution × 1
- mic/accessories
- case/labels/consumables

## 목표 COGS 가설

| Component | Target COGS |
|---|---:|
| Receiver × 10 | ₩450,000 |
| Transmitter/controller | ₩90,000 |
| Charging/storage | ₩100,000 |
| Mic/accessories | ₩50,000 |
| Packaging/labels/QA allowance | ₩30,000 |
| **Total target COGS** | **₩720,000** |

위 값은 양산 견적이 아니라 사업성 역산용 target cost다.

---

# 3. 판매가 가설

| Starter kit selling price | Gross profit | Gross margin |
|---:|---:|---:|
| ₩1,290,000 | ₩570,000 | 44.2% |
| ₩1,390,000 | ₩670,000 | 48.2% |
| ₩1,490,000 | ₩770,000 | 51.7% |

### 현재 테스트 중심값
**₩1,390,000 / 10-person kit**

이 가격을 정답으로 보지 않는다. 구매자 인터뷰에서 다음을 먼저 확인한다.
- 현재 장비 구매/대여 비용
- 구매 승인 한도
- 세트당 필요한 인원 수
- 한 해 사용 횟수
- 장비 수명 기대치

현재 시장에는 저가형 tour-guide system부터 10-person 기준 수천 달러의 전문 시스템까지 매우 넓은 가격대가 존재한다. PR1은 단순 RF 성능이 아니라 open-ear + phone-free + fleet operation의 추가 가치를 증명해야 중간 가격대를 정당화할 수 있다.

---

# 4. Rental Model

## 1일 대여 가설
- Revenue: **₩120,000**
- Variable handling/cleaning/delivery allowance: **₩25,000**
- Contribution per rental: **₩95,000**

Target COGS ₩720,000 기준 장비 원금 회수:

**약 7.6회 유료 대여**

즉 8회 안팎에서 장비 투자비를 회수할 수 있다는 가설이다.

### 반드시 실제로 측정할 비용
- 배송/이동
- 세팅 인력 시간
- 충전 시간
- 소독/소모품
- 분실
- 배터리 교체
- 파손/수리
- 결제 수수료

이 비용을 빼고 계산한 대여 수익은 의미가 없다.

---

# 5. Pilot Pricing Experiment

### 무료 pilot
최대 3곳. 무료 사용 대신 반드시 구매자 인터뷰·측정·사후 인터뷰를 받는다.

### Paid Pilot 1
60~90분 / 10명 이하

**₩50,000** 제안

### Paid Pilot 2
반일 또는 반복 프로그램

**₩100,000~₩120,000** 제안

목적은 수익이 아니라 **가격을 말했을 때 고객 행동이 바뀌는지** 보는 것이다.

---

# 6. Bottom-up Market Scenario — 청소년수련시설만 계산

공식 시설 수를 872개라고 놓고 단순 시나리오를 만든다.

가정:
- kit ASP = ₩1,390,000
- 평균 2 kit/facility

| Facility penetration | Customers | Hardware revenue scenario |
|---:|---:|---:|
| 5% | 44 | ₩122.3M |
| 10% | 87 | ₩241.9M |
| 20% | 174 | ₩483.7M |

이 표는 TAM을 주장하기 위한 시장자료가 아니다. **한 세그먼트만으로 사업이 어느 정도 크기까지 갈 수 있는지 보는 내부 bottom-up 계산**이다.

학교, 민간 스포츠, 관광, 행사, 해외시장은 여기에 포함하지 않는다.

---

# 7. Scalability Rule

고객마다 새 하드웨어를 만들지 않는다.

공통 SKU:
1. PR1 Transmitter
2. PR1 Receiver
3. PR1 Charge/Storage Case

고객별 차이는 가능하면 다음으로 해결한다.
- label / color / accessory
- channel configuration
- software setting
- content
- support plan

---

# 8. 반복매출 후보

하드웨어 판매 후:
- receiver replacement
- battery service
- annual maintenance
- charging/storage upgrade
- fleet management software
- content distribution / multilingual content
- extended warranty

단, 소프트웨어 구독은 고객이 실제 관리 문제를 보여주기 전에는 개발하지 않는다.

---

# 9. Financial Kill Rules

다음 조건이면 제품 구조를 다시 설계한다.

- 실제 COGS가 목표 판매가의 60%를 지속적으로 초과
- warranty/defect/loss 비용 포함 후 GM < 40%
- paid rental payback > 12회
- 고객이 기대하는 가격이 원가보다 낮고 기능 축소로 해결되지 않음
- 10명 세트보다 20~30명 세트에서 급격히 운영비가 증가

---

# 10. 현재 경쟁 가격 참고

- Williams AV 10-person Digi-Wave Tour Guide System: 약 US$3,495 표시
- Retekess 2.4 GHz group systems: 저가 receiver부터 다양한 multi-user bundles 존재

Source:
- https://williamsav.com/product-category/tour-intercom-systems/
- https://www.retekess.com/collections/wireless-receiver

가격은 국가·구성·할인·배송·세금에 따라 달라지므로 PR1 가격의 근거를 경쟁가격 하나에 의존하지 않는다.
