# PR1 Korea Productization Gates

Date: 2026-07-23

Purpose: prevent a technically successful PoC from becoming a production design
that is unnecessarily difficult to test, certify, manufacture, or sell in
Korea.

This document is a project checklist, **not a substitute for a test laboratory,
certification body, radio authority, or legal opinion**. Classification depends
on the final hardware and operating configuration.

## 1. The key distinction

There are three different objects in PR1:

1. **development board** — the LILYGO T3-S3 MVSR used to discover whether the
   concept works
2. **engineering validation prototype** — a design close enough to production
   to measure RF, EMC, power and safety risks
3. **sale product** — the frozen TX/RX hardware, firmware, antenna, battery,
   charger interface and enclosure that is placed on the Korean market

Do not treat the first object as the third.

## 2. Current official regulatory anchors

The National Radio Research Agency (RRA) states that a person intending to
manufacture, sell, or import broadcasting/communications equipment must obtain
the applicable conformity assessment: conformity certification, conformity
registration, self-confirmation, or provisional certification.

Official RRA overview:
https://www.rra.go.kr/ko/license/A_a_about.do

The Central Radio Management Service (CRMS) states that radio stations other
than those that may be opened by notification or without notification generally
require authorization. It also identifies conformity-assessed specific
low-power radio equipment as an example of equipment that can be used without
opening a separately authorized radio station.

Official CRMS overview:
https://www.crms.go.kr/lay1/S1T41C42/contents.do

RRA also publishes conformity-assessment exemption guidance for cases such as
personal non-sale import, market research, research and technical development.
The exemption category and permitted quantity/purpose must be checked against
the rule in force at the time of import/test; an exemption from conformity
assessment must not be interpreted as permission for arbitrary RF operation.

Official RRA FAQ:
https://www.rra.go.kr/ko/notice/D_e_faq2_1.do

For a portable lithium rechargeable battery, the Korea Agency for Technology
and Standards currently publishes `KC 62133-2`, "Safety requirements for
portable sealed secondary lithium cells and batteries".

Official KATS standard page:
https://www.kats.go.kr/content.do?cid=25751&cmsid=304&mode=view&page=

## 3. Outdoor-test decision gate

Before any open-air PR1 RF test, create a one-page test configuration sheet with:

- exact board and revision
- exact radio chip/module
- exact firmware commit
- radio mode (LoRa / FLRC / GFSK or other)
- center frequency / channel
- occupied bandwidth setting
- configured transmit power
- antenna type and gain
- whether an external PA exists
- test location
- test radius / distance
- test duration
- number of TX/RX units
- shutdown procedure

Then get the exact configuration classified through the appropriate official
route. The possible paths include:

### Path A — equipment/configuration already covered for licence-free use

Use only within the assessed/allowed configuration. Do not assume a certified
MCU or one certified submodule automatically covers an independently operating
SX1280 radio path.

### Path B — controlled laboratory / shielded or conducted test

Use a qualified RF test environment to measure the design without relying on
an uncontrolled open-air transmission.

### Path C — experimental radio-station route

If the intended experimental configuration is not within a notification-free
radio path, consult the competent radio management office about whether an
experimental-station authorization is the appropriate route.

The project must not choose one of these paths by guesswork.

## 4. Prototype-to-production hardware transition

Once P0-P4 technical gates pass, freeze a **production architecture candidate**.

### TX candidate must define

- production MCU
- RF IC/module
- RF matching network
- antenna and permitted variants
- audio-input architecture
- battery / power architecture
- USB/charging/data behavior
- enclosure material around the antenna
- firmware limits on RF mode/power/channel

### RX candidate must define

- production MCU
- RF IC/module
- antenna
- codec/decoder location
- I2S/DAC/amplifier path
- transducer/output connector
- battery / charging circuit
- maximum acoustic output strategy
- enclosure

If TX and RX are electrically different products, plan for the possibility that
each model needs its own applicable assessment/test scope. Ask a designated test
laboratory whether they can be handled as a base/derived model family; do not
assume that selling them as one set makes them one certification object.

## 5. Certification-friendly design rules

Use these rules from the first custom-PCB draft:

- prefer a radio module/architecture with a clear Korean compliance path when it
  does not destroy the product requirement
- avoid adding an RF PA unless measured need and regulatory path justify it
- do not use a high-gain removable antenna just to rescue range late in the
  project
- reserve test points for conducted RF measurements where practical
- make RF power/channel/mode limits explicit in production firmware
- keep a BOM with exact manufacturer part numbers and approved alternates
- keep antenna part number, cable and connector in configuration control
- keep PCB gerbers, schematics and firmware release tied to each test sample
- do pre-compliance before paying for the final formal test campaign

## 6. Suggested pre-compliance package

Before formal conformity assessment, ask a Korean designated test laboratory to
review at least:

- RF output and spectrum for every intended operating mode
- occupied bandwidth
- unwanted/out-of-band emissions
- spurious emissions
- EMC emissions/immunity applicable to the final equipment category
- antenna implementation and gain assumptions
- battery/charging safety scope
- product safety scope for a body-worn/listening device
- marking/manual requirements
- whether SAR/EMF exposure assessment applies to the final form/use distance

The laboratory should see the **production-candidate hardware**, not only the
LILYGO PoC board.

## 7. Battery rule

Do not design the first sale batch around an unknown loose cell bought purely on
price.

Preferred path:

- source a traceable cell/battery pack with the applicable Korean safety
  documentation
- use a proper protection circuit
- validate charging current/voltage limits
- validate over-current/short/temperature behavior required by the selected
  battery architecture
- record the exact cell and pack model in the BOM
- repeat the compliance review before substituting a battery later

Current Korean safety-standard anchor for portable sealed lithium rechargeable
cells/batteries: `KC 62133-2`.

## 8. Product freeze before formal test

Do **not** start the final formal test while these are still moving:

- PCB revision
- RF matching values
- antenna
- enclosure around antenna
- battery model
- charger circuit
- radio firmware settings
- connector/cable affecting emissions

Create a release tag such as:

```text
PR1-EVT1 -> engineering validation
PR1-DVT1 -> design validation / pre-compliance
PR1-CERT1 -> exact sample sent for formal test
PR1-PVT1 -> production validation
```

Every test result must point to one of these frozen configurations.

## 9. Recommended project sequence

```text
LILYGO PoC
  -> packet/link proof
  -> stored audio proof
  -> measured quality/range/latency
  -> go/no-go business decision
  -> production architecture candidate
  -> custom PCB EVT
  -> conducted / bench RF measurements
  -> DVT + enclosure + battery freeze
  -> pre-compliance lab review
  -> design corrections
  -> formal applicable KC / RRA / safety procedures
  -> pilot production validation
  -> sale
```

## 10. What PR1 should NOT spend money on yet

Until the SX1280 PoC demonstrates useful audio under a viable operating
configuration, do not spend meaningful budget on:

- injection moulding
- final industrial design
- mass PCB order
- packaging production
- final mobile app
- certification campaign for the LILYGO dev board as if it were the product
- large battery inventory

The next money should buy **uncertainty reduction**, not appearance.

## 11. Go/no-go before custom PCB

Custom production hardware starts only if all are true:

- [ ] deterministic RF data link works
- [ ] stored coded audio works end-to-end
- [ ] measured latency is acceptable for the chosen use case
- [ ] measured loss/quality trade-off is acceptable
- [ ] there is a plausible lawful/certifiable RF configuration
- [ ] target customer/use case is still supported by market validation
- [ ] rough product BOM and certification budget do not destroy unit economics

If one fails, solve that gate before drawing a production PCB.
