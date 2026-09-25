# PR3 Optical Alpha v0.1 — Design

Date: 2026-09-25
Status: approved conversational design, written-spec review pending
Scope: first near-eye optical feasibility alpha; no CGH product implementation

## 1. Purpose

PR3 Optical Alpha v0.1 exists to answer a narrower question than “can we build holographic glasses?”

> Can a compact, wearable, monocular near-eye optical module deliver a stable, readable AI information layer with acceptable eyebox, brightness, weight, power, and heat?

This alpha must reduce optical uncertainty before any investment in dynamic computer-generated holography, retinal projection, eye-tracked pupil steering, or a custom product enclosure.

## 2. Alpha strategy

PR3 v0.1 is monocular first.

The baseline architecture is:

`phone / compute -> renderer -> microdisplay light engine -> matched waveguide -> eye`

The selected Alpha path is **B: microdisplay + waveguide**, with full-color MicroLED as the preferred light engine class.

**A: LBS + HOE** remains a v0.2 comparison path and is not discarded. It is deferred because a custom or tightly matched HOE introduces laser wavelength, incidence-angle, scanning, calibration, speckle, eye-safety, and coupling variables before the basic user experience has been validated.

## 3. Why monocular first

The first alpha uses one display eye because monocular validation:

- requires one optical chain instead of two;
- avoids binocular alignment and vergence calibration as first-order variables;
- reduces display/light-engine power;
- lowers mass and temple-volume pressure;
- is sufficient for text, icon, arrow, state, and short AI-card content;
- matches the direction of current commercial and development-kit work for lightweight full-color AI/AR glasses.

Binocular display is a later gate, not a v0.1 requirement.

## 4. Optical Alpha v0.1 target specification

| Parameter | Target | Minimum gate | Measurement / interpretation |
| --- | ---: | ---: | --- |
| Display architecture | monocular see-through | monocular see-through | one eye only |
| Primary content | text, icon, arrow, short AI status | same | not cinematic video |
| Diagonal FOV | 30° | 25° | usable image region |
| Eyebox | 12 x 10 mm | 9 x 8 mm | contiguous region meeting visibility gate |
| Eye relief | 18-20 mm | 15 mm | cornea/reference pupil plane to last optical surface |
| Resolution | 800 x 600 class | 640 x 480 class | rendered addressable image |
| Angular resolution | >=28 PPD target | >=22 PPD | center-region effective value |
| Center eye-level brightness | 3,000 nit target | 1,500 nit | white test image, calibrated measurement |
| Stretch brightness | >=5,000 nit | not required | only if thermal/power budget allows |
| White-field luminance uniformity | >=70% | >=55% | `Lmin / Lmax` over defined 9-point FOV grid |
| Optical display module, excluding waveguide | <=25 x 15 x 8 mm | <=30 x 20 x 10 mm | bounding box of engine + required relay/coupler hardware |
| Optical display module volume | <=3 cm^3 | <=6 cm^3 | excluding remote evaluation electronics |
| Display-side temple thickness | <=10 mm target | <=12 mm | alpha frame local max, excluding temporary lab cable strain relief |
| Total wearable alpha mass | <=70 g target | <=85 g | worn components only; bench controller excluded if cabled |
| Display stack average electrical power | <=0.6 W | <=1.0 W | steady representative UI pattern |
| Whole worn electronics average power | <=1.0 W | <=1.5 W | phone compute excluded |
| Worn electronics peak power | <=1.5 W target | <=2.0 W | short peak allowed |
| Temple/contact-surface temperature after 30 min | <=42°C | <45°C | ~25°C ambient, steady test scene |
| Continuous optical test duration | >=30 min | >=30 min | no brightness collapse or unsafe thermal rise |

These are Alpha acceptance targets, not claims that every currently available component meets all values simultaneously.

## 5. Image-quality requirements

### 5.1 Readability

At nominal eye position and eye relief, the alpha must render:

- high-contrast Latin and Korean UI text;
- simple arrows/icons;
- grayscale steps;
- RGB primaries and white;
- a geometric alignment grid.

The center 60% of the usable FOV must allow recognition of the reference text without repeated head repositioning.

### 5.2 Uniformity

Use a fixed 9-point sampling grid covering center, cardinal midpoints, and corners of the specified usable FOV.

Define:

`white-field uniformity = minimum measured luminance / maximum measured luminance`

Gate pass: >=0.55.

Target: >=0.70.

A single center-brightness measurement is not sufficient evidence of optical usability.

### 5.3 Color and rainbow tolerance

For a white text/grid image:

- central 80% of the usable FOV must not show a persistent color-separated ghost that crosses and materially reduces text legibility;
- edge color variation may be tolerated if reference content remains readable;
- rainbow/stray-light artifacts are recorded photographically at fixed eye-camera positions rather than described only subjectively.

### 5.4 Ghosting and stray light

A double image, internal reflection, or stray-order artifact that causes the same UI element to appear as a second readable object in the central 60% FOV is a fail condition.

## 6. Eyebox definition and test

The alpha eyebox is not the vendor's nominal exit-pupil specification alone. It is the measured contiguous region at the target eye-relief plane where all of the following remain true:

- center reference text is visible;
- at least 80% of the intended content is not clipped;
- white-field brightness remains >=50% of the nominal-center measurement;
- no severe color breakup prevents reading.

Test using a camera/pupil proxy translated in X and Y at the nominal eye-relief plane.

Gate: >=9 x 8 mm.

Target: >=12 x 10 mm.

## 7. Head/eye movement persistence

Optical Gate 3 is passed only when a wearer can make natural small head/frame movements without the UI repeatedly disappearing.

Bench proxy requirement:

- translate the eye-camera proxy at least +/-4.5 mm horizontally and +/-4 mm vertically around the nominal pupil point for the minimum gate;
- record clipping, luminance drop, color shift, and complete dropout.

Wearer test requirement:

- normal blink/head micro-motion must not produce frequent complete image loss;
- a wearer should not need to hold the glasses in one exact position to read short text.

## 8. Brightness test

Measure eye-level luminance at the nominal center pupil position.

Required scenes:

- full white field;
- 20% area white UI card on transparent/black background;
- white text on transparent/black background.

Gate: center >=1,500 nit for the agreed reference scene.

Target: >=3,000 nit.

The alpha must additionally demonstrate readable text in:

- normal indoor lighting;
- a bright office/window-adjacent condition;
- outdoor shade.

Direct-sun readability is a later stretch target and is not required to pass v0.1.

## 9. Thermal and power test

Power measurements must separate, where practical:

- display/light engine;
- driver electronics;
- local controller/bridge;
- other worn electronics.

The phone's compute power is excluded from the worn-glasses power figure.

Thermal test:

1. ~25°C ambient;
2. representative UI workload;
3. 30 minutes continuous display;
4. measure display-side temple/contact surface at fixed positions at 0, 5, 10, 20, and 30 minutes.

Pass conditions:

- no contact-surface hotspot >=45°C;
- target <=42°C at ordinary skin-contact regions;
- no automatic brightness collapse that invalidates the brightness gate;
- no unstable reboot, image loss, or optical drift caused by heat.

## 10. Form-factor test

The v0.1 enclosure may be a test frame rather than a consumer industrial design, but all worn optical components must be physically placed in a glasses-like geometry.

The bench evaluation controller may remain off-head through a cable for early optical bring-up. However, the weight gate is only passed after the required worn optical/driver components are represented at realistic mass and position.

The alpha records:

- total worn mass;
- left/right mass;
- center of mass relative to bridge;
- display-side temple max thickness;
- optical engine bounding volume;
- cable/connector mass excluded from a future integrated design estimate but included in actual alpha mass if worn.

## 11. Direction A — LBS + HOE

### Architecture

`renderer -> RGB laser sources -> MEMS beam scanner -> HOE/coupler -> eye`

### Advantages

- very small optical engine is achievable;
- focus-free scanning behavior can reduce relay-optics burden;
- high color gamut is feasible;
- long-term form-factor potential is strong;
- a scanner can be attractive for pupil-steered or custom future optical systems.

### Disadvantages

- wavelength and incidence-angle matching with HOE is demanding;
- MEMS scanning/calibration becomes a first-alpha variable;
- laser speckle and stray-order behavior must be managed;
- eye-safety analysis is mandatory for a product path;
- custom HOE exposure/fabrication adds NRE and iteration delay;
- engine brightness does not directly predict eye-level brightness after coupling/expansion losses.

### Current component accessibility

TriLite's current Trixel 3 is an engineering-sample/evaluation-kit class RGB LBS engine. Verified current published figures include <1 cm^3 volume, 1.5 g weight, 15 lm luminous flux, 320 mW power, 24° x 18° FOV, and up to 1152 x 884 resolution in evaluation-kit material.

### Alpha suitability

**Deferred for v0.1; strong v0.2 comparison candidate.**

The engine itself is accessible enough for experimentation, but the coupled LBS+HOE system creates more optical degrees of freedom than required for the first gate.

## 12. Direction B — microdisplay + waveguide

### Architecture

`renderer -> full-color microdisplay/projector -> collimation/coupling optics -> matched waveguide -> eye`

Preferred microdisplay class: full-color MicroLED.

### Advantages

- conventional raster image path is straightforward to debug;
- stable test patterns simplify quantitative optical measurement;
- full-color MicroLED projectors are now extremely compact and low power;
- current vendors offer matched or development-kit paths aimed at monocular AR glasses;
- the architecture isolates waveguide/eyebox performance more cleanly than a first custom LBS+HOE build.

### Disadvantages

- collimation and coupling optics still require careful alignment;
- waveguide efficiency/uniformity/rainbow remain difficult;
- matched waveguide sourcing may be vendor-controlled;
- quoted projector power does not include the full driver/waveguide system;
- consumer-grade brightness, thermal, and uniformity still require system measurement.

### Current component accessibility

JBD's Hummingbird II published specification is 0.2 cc, 0.5 g, 500 x 380, 25° FOV, 3 lm luminous flux, and typical power as low as 95 mW; JBD states up to 4,000-nit eye-level brightness when paired with waveguides.

JBD announced Roadrunner II in June 2026 as an SVGA polychrome MicroLED projector with 0.18 cc volume and introduced a monocular full-color AR-glasses development kit based on it.

### Alpha suitability

**Selected for PR3 Optical Alpha v0.1.**

The first procurement attempt should prioritize a matched development/evaluation path instead of a loose projector plus an unrelated waveguide.

## 13. A vs B comparison

| Factor | A. LBS + HOE | B. Microdisplay + waveguide |
| --- | --- | --- |
| Long-term optical-engine size | excellent | excellent |
| First-alpha optical variables | high | moderate |
| Image/test-pattern stability | moderate; scanner/calibration dependent | high |
| HOE/waveguide matching burden | very high for custom HOE | high, reduced by matched kit |
| Speckle concern | significant | lower for incoherent/microdisplay path |
| Laser eye-safety burden | direct and early | lower at first alpha, still requires product safety work |
| Component access | evaluation LBS available; HOE integration harder | current MicroLED projectors/dev kits favorable |
| Published engine power example | ~320 mW Trixel 3 | ~95 mW Hummingbird II typical projector figure |
| Published FOV example | 24° x 18° Trixel 3 engine | 25° Hummingbird II projector; system waveguide determines final FOV |
| Custom NRE | high | moderate-high |
| v0.1 suitability | defer | select |

The power figures above are vendor engine figures, not directly comparable system-level worn-glasses measurements.

## 14. Current optical evidence boundary

Recent research demonstrates why the alpha gates focus on eyebox and uniformity rather than headline FOV alone.

A 2025 Nature Photonics synthetic-aperture waveguide holography prototype reported 38° diagonal FOV, 9 x 8 mm eyebox, and 23-33 mm eye-relief range in a compact experimental system. This demonstrates that a 9 x 8 mm eyebox is a technically meaningful lower gate, not that PR3 already has such performance.

A 2026 dual-layer full-color volume-holographic waveguide paper reported 28° diagonal FOV, 14 x 16 mm eyebox, 15 mm eye relief, and 53.9% full-FOV white-light brightness uniformity. This illustrates that usable eyebox and color can coexist while uniformity remains a major engineering challenge.

PR3 must treat these as external evidence points only. Laboratory research results and vendor specifications are not substitutes for PR3 measurements.

## 15. Procurement strategy

### Preferred v0.1 path

Procure or obtain evaluation access to:

1. a current full-color MicroLED projector or monocular AR development kit;
2. a waveguide specifically matched to that engine/coupler where possible;
3. the vendor's driver/evaluation electronics;
4. mechanical fixtures sufficient for repeatable pupil/eye-relief positioning;
5. photometric measurement access appropriate for eye-level luminance and uniformity.

A loose unmatched projector + random waveguide combination is not the preferred first-alpha route.

### BOM / procurement envelope

Because current AR light engines and waveguides are commonly B2B/RFQ parts, v0.1 uses a procurement envelope instead of presenting a false production BOM.

Planning envelope for B, one monocular optical prototype:

- **KRW 2M-8M** for light engine/dev hardware, matched waveguide/sample optics, fixtures, cabling, and prototype integration consumables.

Planning envelope for A, later LBS + custom/matched HOE prototype:

- **KRW 3M-12M+**, with larger uncertainty because HOE fabrication/exposure NRE and iteration can dominate.

These are planning allowances rather than vendor quotes. A production BOM is not authorized until a selected optical path passes v0.1/v0.2 gates and at least three relevant vendor quotes or equivalent sourcing data are available.

## 16. Optical Gate 1 — image visibility

Pass when:

- a full RGB test pattern is visible at the nominal eye position;
- Korean/Latin reference text is readable at nominal position;
- geometric grid edges are recognizable across the intended usable FOV;
- focus/alignment does not require continuous manual adjustment during a short test.

No pass is claimed from a single photographed bright spot or partial image.

## 17. Optical Gate 2 — eyebox

Pass when the measured contiguous usable eyebox is >=9 x 8 mm at >=15 mm eye relief using the definition in Section 6.

Target: 12 x 10 mm at 18-20 mm eye relief.

## 18. Optical Gate 3 — movement persistence

Pass when normal small wearer motion and the bench pupil-translation test do not cause frequent complete image dropout within the minimum eyebox region.

Record the full usable region as a map rather than a single center point.

## 19. Optical Gate 4 — brightness

Pass when:

- center eye-level brightness reaches >=1,500 nit for the defined reference pattern;
- text remains readable indoors, near a bright window, and outdoors in shade;
- brightness is retained after the 30-minute thermal test within the limits below.

Target: >=3,000 nit center.

## 20. Optical Gate 5 — wearability

Pass when:

- total worn mass <=85 g;
- display-side temple local max thickness <=12 mm;
- optical module is represented in a glasses-like location;
- a 30-minute wear test does not reveal an immediate mechanical showstopper such as unstable slipping or one localized pressure point severe enough to prevent continued use.

Target: <=70 g and <=10 mm display-side temple thickness.

## 21. Optical Gate 6 — thermal/power

Pass when:

- 30-minute representative display operation completes;
- contact-surface temperature remains <45°C, with <=42°C the target at ordinary skin-contact regions;
- worn electronics average power remains <=1.5 W for the alpha gate, with <=1.0 W the target;
- no thermal shutdown, unstable image, or brightness collapse invalidates the optical measurements.

## 22. Gate order

The gates are executed in this order:

1. image exists;
2. eyebox is sufficient;
3. movement persistence is sufficient;
4. brightness is usable;
5. size/weight is wearable;
6. heat/power is manageable.

A failed earlier gate should be corrected before adding more complex rendering or custom enclosure work.

## 23. Out of scope for v0.1

The following are explicitly not authorized as product implementation in this alpha:

- dynamic CGH video pipeline;
- retinal projection product design;
- holographic SLM-based product renderer;
- eye-tracked pupil steering;
- varifocal/multifocal product optics;
- binocular calibration;
- custom silicon;
- custom full industrial-design frame;
- custom prescription-lens integration;
- production laser-safety certification;
- mass-production tooling.

Research may be read to inform later work, but these features do not enter the v0.1 build plan.

## 24. Alpha artifacts

A complete Optical Alpha v0.1 evidence package contains:

- optical stack diagram with actual component identifiers;
- measured FOV;
- measured eyebox map;
- measured eye relief;
- resolution/reference-pattern captures;
- 9-point luminance table and uniformity ratio;
- rainbow/ghost artifact captures from fixed pupil positions;
- power log;
- 30-minute temperature log;
- worn mass and temple-thickness measurements;
- 30-minute wearer notes;
- gate-by-gate pass/fail record with raw measurements attached.

No marketing-style “works” conclusion replaces these measurements.

## 25. Decision after v0.1

If B passes the six gates, proceed to one of two evidence-driven next steps:

- **v0.2-A comparison:** acquire LBS + matched/custom HOE and run the same gates to determine whether its size/form-factor advantage is worth additional complexity;
- **v0.2-B integration:** reduce B's driver/mechanical volume and move toward a more self-contained wearable if B already meets product needs.

The decision must use the same metrics rather than choosing an architecture based on novelty.

## 26. Repository boundary

This is an optical/hardware research specification, not PR1 firmware work.

It is committed only on the isolated documentation branch `docs/pr2-pr3-alpha-specs-20260925` for review. It must not cause PR3 optical work or product code to be merged into PR1 `main`.

## 27. Current technology basis verified 2026-09-25

Before writing this spec, the following current public evidence was checked:

- TriLite Trixel 3: <1 cm^3, 1.5 g, 15 lm, 320 mW, 24° x 18° FOV; evaluation-kit material lists up to 1152 x 884.
- JBD Hummingbird II: 0.2 cc, 0.5 g, 25° FOV, 500 x 380, 3 lm, 95 mW typical; JBD states up to 4,000-nit eye-level brightness with waveguides.
- JBD Roadrunner II: announced June 2026, SVGA polychrome MicroLED projector, 0.18 cc, accompanied by a monocular full-color AR-glasses development kit.
- Nature Photonics synthetic-aperture waveguide holography: 38° diagonal FOV, 9 x 8 mm eyebox, 23-33 mm eye-relief range in the reported experimental system.
- Laser & Photonics Reviews dual-layer full-color volume holographic waveguide: 28° diagonal FOV, 14 x 16 mm eyebox, 15 mm eye relief, 53.9% full-FOV white-light brightness uniformity.

All vendor values remain vendor specifications until independently measured in PR3 hardware.
