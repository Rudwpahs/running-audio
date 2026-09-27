# PR3 Optical Alpha v0.1 Spec

Date: 2026-09-27
Status: Alpha specification / optical feasibility gate
Scope: near-eye see-through optical alpha only. No CGH product implementation, retinal-projection product path, dynamic holographic video, or eye-tracked pupil steering before the gates in this document pass.

## 1. Decision summary

PR3 Optical Alpha v0.1 answers one question:

> Can a glasses-like, monocular near-eye display deliver a stable and readable AI information layer with adequate eyebox, brightness, image quality, size, mass, power, and heat?

Baseline flow:

`Phone / compute -> renderer -> light engine -> matched HOE or waveguide -> eye`

### v0.1 architecture decision

- **Monocular first.** Binocular is not a v0.1 requirement.
- **Primary path B: microdisplay + matched waveguide.** Full-color MicroLED is preferred when accessible.
- **Path A: LBS + HOE remains a v0.2 comparison path.** Do not discard it, but do not let custom HOE fabrication, scanner tuning, laser safety, speckle, or wavelength/angular matching become first-alpha variables.
- Prefer a **matched projector + waveguide development/evaluation path** over a loose projector plus unrelated waveguide.
- A matched LCoS + waveguide kit is acceptable as an early optical-gate fallback if current MicroLED hardware is inaccessible, but its power/mass results do not qualify the final MicroLED product path.

The B decision is an **alpha-risk decision**, not a claim that current LBS is intrinsically worse in power or form factor. Current LBS engineering samples are already highly competitive on engine size and power; B is selected because it reduces the number of coupled optical variables in the first feasibility build.

## 2. Evidence boundary

Vendor specifications in this document establish sourcing feasibility and comparison points only. Research prototypes establish physically demonstrated ranges only. Neither substitutes for a PR3 measurement.

No PR3 performance claim may be made until it is measured on the PR3 alpha using the procedures below.

## 3. Optical Alpha v0.1 requirements

| Parameter | Target | Minimum gate | Notes / measurement |
| --- | ---: | ---: | --- |
| Display strategy | monocular see-through | monocular see-through | one display eye for v0.1 |
| Primary content | text, icon, arrow, status card | same | no cinematic/video requirement |
| Diagonal usable FOV | 30 deg | 25 deg | measured visible content region |
| Eyebox | 12 x 10 mm | 9 x 8 mm | contiguous usable region at eye-relief plane |
| Nominal eye relief | 18-20 mm | 15 mm | pupil/cornea reference plane to last optical surface |
| Addressable resolution | 800 x 600 class | 640 x 480 class | raster source; system MTF must still be checked |
| Effective angular resolution | >=28 PPD | >=22 PPD | center region; report method and axis/diagonal basis |
| In-eye luminance, representative 20% white UI | >=3,000 cd/m2 | >=1,500 cd/m2 | nominal pupil position |
| Stretch in-eye luminance | >=5,000 cd/m2 | not required | only if thermal/power budget remains valid |
| White-field luminance uniformity | >=70% | >=55% | `Lmin / Lmax`, fixed 9-point FOV grid |
| RGB registration, central 60% FOV | <=1.5 px RMS target | <=3 px RMS | white grid / RGB edge target |
| Secondary ghost/stray peak, central 60% | <=5% target | <=10% | relative to primary feature at same test exposure |
| Optical module bbox, waveguide excluded | <=25 x 15 x 8 mm | <=30 x 20 x 10 mm | engine + required coupling/relay package |
| Optical module volume, waveguide excluded | <=3 cm3 | <=6 cm3 | remote bench controller excluded |
| Display-side temple local thickness | <=10 mm | <=12 mm | future glasses-like integration envelope |
| Total worn alpha mass | <=70 g | <=85 g | off-head bench controller excluded only when not worn |
| Display stack average electrical power | <=0.6 W | <=1.0 W | representative UI workload |
| Whole worn electronics average power | <=1.0 W | <=1.5 W | phone compute excluded |
| Whole worn electronics short peak | <=1.5 W | <=2.0 W | log duration and workload |
| Skin/contact surface after 30 min | <=42 C | <45 C | 25 +/-2 C ambient |
| Continuous optical run | >=30 min | >=30 min | no thermal brightness collapse or unstable image |

The size, temple, mass, power, and thermal values above are PR3 engineering targets, not vendor claims.

## 4. Monocular versus binocular strategy

### v0.1: monocular

Monocular is sufficient to validate the core PR3 experience: short text, icons, navigation cues, AI status, and notifications. It removes binocular alignment, inter-pupillary variation, vergence, two-channel calibration, duplicated display power, and duplicated optical mass from the first optical experiment.

### Binocular promotion gate

Do not start a binocular product alpha until the monocular build passes Gates 1-6. A later binocular alpha must add explicit inter-eye brightness/color matching, image registration, IPD coverage, binocular comfort, duplicated-power, and mass-balance requirements rather than assuming monocular results transfer automatically.

## 5. Image-quality definitions

### 5.1 Readability

At nominal eye position and eye relief, render all of:

- Korean and Latin high-contrast text;
- thin and thick geometric lines;
- arrows and simple icons;
- grayscale steps;
- RGB primaries and white;
- a geometric alignment grid.

Pass requires reference text to remain readable across the central 60% of usable FOV without repeated frame repositioning.

### 5.2 FOV

Report horizontal, vertical, and diagonal usable FOV. “Usable” excludes regions where clipping, severe blur, severe color separation, or luminance loss prevents the reference content from being read.

### 5.3 Eyebox

Vendor exit-pupil numbers are not accepted as the PR3 eyebox result.

At the nominal eye-relief plane, map an X/Y grid and mark a sample usable only when all are true:

1. center reference text remains visible;
2. >=80% of intended content remains unclipped;
3. local white-field luminance is >=50% of nominal-center luminance;
4. severe color breakup does not prevent reading.

The reported eyebox is the largest contiguous region satisfying the definition.

- Gate: >=9 x 8 mm.
- Target: >=12 x 10 mm.

### 5.4 Uniformity

Use a fixed 9-point FOV grid: center, four cardinal midpoints, four corners.

`white-field uniformity = minimum measured luminance / maximum measured luminance`

- Gate: >=0.55.
- Target: >=0.70.

Also retain all nine raw measurements. A single center-brightness measurement is insufficient.

### 5.5 Rainbow, color breakup, ghosting, and stray light

Use fixed camera exposure, fixed pupil proxy, and white grid / white glyph test patterns.

Minimum gate:

- RGB edge-registration RMS <=3 px in the central 60% FOV;
- strongest secondary ghost/stray peak <=10% of the primary feature in the central 60% under the same exposure;
- no secondary image may form a second readable copy of a reference glyph in the central 60%;
- photographically document edge rainbow/color breakup at the same pupil coordinates used for the FOV grid.

Target: <=1.5 px RMS RGB registration and <=5% secondary peak.

These metrics are internal alpha acceptance metrics, not optical-industry standards.

## 6. Candidate A — LBS + HOE

Architecture:

`renderer -> RGB laser sources -> MEMS beam scanner -> matched/custom HOE -> eye`

### Strengths

- very small light-engine package is possible;
- raster scan can avoid a conventional panel and some relay-optics volume;
- strong long-term form-factor potential;
- wide color gamut and high source brightness are possible;
- current Trixel 3 Cube-class LBS power is already competitive at engine level;
- remains relevant for future pupil-steered or specialized optical architectures.

### First-alpha burdens

- laser wavelength and HOE Bragg/angular matching are tightly coupled;
- scanner calibration and geometric stability become system variables;
- speckle and coherent stray-light behavior require management;
- laser eye-safety work is an immediate human-use requirement;
- a custom HOE adds recording/fabrication NRE and alignment iteration;
- light-engine lumens or scanner brightness do not predict in-eye brightness after coupling and pupil expansion.

### Current reference points

TriLite's latest integration-facing Trixel 3 Cube material reports approximately 1 cm3 volume, 1.5 g mass, up to 15 lm, and around 145 mW typical power, with engineering samples available. A separate current Trixel 3 platform flyer published in 2026 lists <1 cm3, <1.5 g, >=30 deg FOV, 1024 x 768, 15 lm, and 320 mW for the stated 20%-pixel-on / 5-lm use condition. These figures describe closely related product/platform revisions and operating definitions and must not be mixed into one imaginary configuration.

For PR3, vendor engineering-sample data is a sourcing/reference point only. The final LBS+HOE result depends on the selected HOE/coupler, wavelengths, incidence geometry, pupil expansion, calibration, and safety constraints.

### Alpha fit

**v0.2 comparison candidate, not v0.1 baseline.**

Use A after B has established the basic optical usability target, or earlier only if a supplier can provide a genuinely matched LBS+HOE evaluation system that removes most custom optical variables.

## 7. Candidate B — microdisplay + waveguide

Architecture:

`renderer -> microdisplay projector -> collimation/coupling optics -> matched waveguide -> eye`

Preferred light engine: full-color MicroLED.

### Strengths

- deterministic raster patterns simplify optical debugging and quantitative measurement;
- mature driver interfaces simplify renderer bring-up;
- current MicroLED projectors are sub-cc and low-power at the engine level;
- matched projector/waveguide or monocular development paths exist;
- waveguide performance can be isolated from scan-calibration variables;
- no coherent-laser speckle term in the preferred MicroLED path.

### Burdens

- coupling optics and waveguide alignment remain sensitive;
- pupil expansion trades optical efficiency against uniformity;
- full-color dispersion/rainbow and ghost orders remain key risks;
- public component pricing and optical prescriptions are commonly unavailable;
- engine power alone understates driver, bridge, and system power.

### Current reference points

JBD Roadrunner II (announced June 2026) publishes 800 x 600 SVGA, 30 deg class optics, 33.3 PPD in its stated configuration, 6 lm, up to 6,000-nit in-eye brightness with a 30 deg diffractive waveguide, 98 mW typical projector power, and 0.18 cm3 projector volume. JBD also introduced a monocular full-color AR development kit.

DigiLens Crystal30 G4 InsightKit publishes a matched waveguide + Avegant LCoS configuration with 30 deg diagonal FOV, >3,000 nits to eye, 12 x 10 mm eyebox, 17 mm eye relief, 720 x 720, and a monocular option. Its listed light engine is not the preferred PR3 MicroLED path, but the complete kit is a useful current feasibility anchor for waveguide-level geometry and brightness.

### Alpha fit

**Selected v0.1 baseline.**

First procurement preference is a matched MicroLED projector + waveguide/dev kit. If that is blocked, a matched non-MicroLED waveguide kit may be used to validate Gates 1-4, while Gates 5-6 remain provisional for the future target engine.

## 8. A vs B comparison

| Factor | A. LBS + HOE | B. microdisplay + waveguide |
| --- | --- | --- |
| Implementation difficulty | very high for first custom system | medium-high with matched kit |
| Component access | LBS engineering samples exist; matched/custom HOE is harder | current projector/dev-kit and waveguide ecosystem is stronger |
| Alpha planning cost | KRW 3M-12M+ | KRW 2M-8M |
| Current engine-level power reference | Trixel 3 Cube ~145 mW; another current Trixel 3 operating point is 320 mW | Roadrunner II ~98 mW typical projector power |
| Current source/system brightness reference | up to 15 lm engine; final in-eye result depends strongly on HOE/waveguide | Roadrunner II vendor states up to 6,000 nits with matched 30 deg waveguide |
| Current FOV reference | current Trixel 3 platform flyer >=30 deg | Roadrunner II 30 deg; Crystal30 G4 InsightKit 30 deg |
| Eyebox path | custom HOE/EPE dependent | 12 x 10 mm is available as a current matched-kit reference point |
| Form-factor potential | excellent | excellent |
| Alignment burden | very high: laser + MEMS + HOE wavelength/angle | high, lower with a matched projector/waveguide kit |
| Speckle concern | material | low for preferred MicroLED path |
| Eye-safety burden | direct laser-source burden | conventional display safety path; product safety still required |
| Measurement/debug simplicity | moderate | high |
| v0.1 suitability | defer | **select** |

Do not interpret the table as a product-level power ranking. The figures are vendor light-engine operating points under different conditions. B wins v0.1 on **integration-risk isolation and measurement simplicity**, not because A is assumed to consume more power.

## 9. Optical Gates

No CGH product path, retinal projection, dynamic holographic video, or eye-tracked pupil steering is admitted before **all six** gates pass on one coherent alpha configuration.

### Gate 1 — stable image visibility

Pass when:

- full RGB pattern is visible at nominal eye position;
- Korean and Latin reference text is readable;
- geometric grid is recognizable across the usable FOV;
- image remains stable for a 10-minute static run without continuous manual optical adjustment.

A bright spot or partial image is not a pass.

### Gate 2 — sufficient eyebox

Pass when the measured contiguous usable eyebox is >=9 x 8 mm at >=15 mm eye relief using Section 5.3.

Target: >=12 x 10 mm at nominal 18-20 mm eye relief.

### Gate 3 — head/eye movement persistence

Bench pass:

- sweep the pupil-camera proxy through at least +/-4.5 mm X and +/-4.0 mm Y at the gate eye-relief plane;
- perform at least three horizontal and three vertical traversals inside the accepted eyebox;
- no complete image loss lasting >100 ms is allowed inside the accepted region;
- total complete-dropout time must be <1% of recorded traverse time.

Wearer check:

- normal blink, small head motion, and ordinary frame micro-slip must not require the wearer to hold one exact pose to read a short card.

### Gate 4 — usable brightness

Use calibrated photometric measurement at the nominal pupil position.

Required scenes:

- full white field;
- 20% area white UI card on transparent/black background;
- white text on transparent/black background.

Pass:

- representative 20% white UI >=1,500 cd/m2 center;
- target >=3,000 cd/m2;
- after the 30-minute thermal run, reference-scene luminance must remain >=80% of its stabilized early-run value and remain above the minimum gate;
- text must be practically readable indoors, near a bright window, and outdoors in shade.

Direct-sun readability is a later stretch test, not a v0.1 gate.

### Gate 5 — wearable size and mass

Pass when:

- total worn alpha mass <=85 g;
- display-side temple local maximum <=12 mm;
- optical engine is represented in a glasses-like location rather than a remote tabletop surrogate;
- worn optical module fits within the minimum bounding/volume envelope;
- a 30-minute wear check shows no immediate mechanical showstopper such as unstable slipping or a localized pressure point that prevents continued use.

Target: <=70 g and <=10 mm temple local maximum.

### Gate 6 — power and thermal

At 25 +/-2 C ambient with the representative UI workload:

- display stack average <=1.0 W; target <=0.6 W;
- whole worn electronics average <=1.5 W; target <=1.0 W;
- short peak <=2.0 W; target <=1.5 W;
- no ordinary skin/contact surface may reach 45 C;
- target skin/contact surfaces <=42 C after 30 minutes;
- no reboot, image dropout, optical drift, or brightness collapse attributable to heat.

Phone compute power is reported separately and excluded from worn-glasses power.

## 10. Alpha bench configuration

### 10.1 Required categories

1. **Compute/render source**
   - phone, PC, or SBC capable of deterministic RGB test patterns;
   - MIPI/HDMI/USB bridge as required by the selected evaluation hardware.

2. **Light engine**
   - preferred: full-color MicroLED projector/evaluation module;
   - acceptable early fallback: matched alternative microdisplay engine supplied with the waveguide kit.

3. **Optical combiner**
   - waveguide specifically matched to the chosen projector/coupler;
   - avoid a random loose waveguide as the first experiment.

4. **Mechanical alignment**
   - rigid optical base;
   - XYZ translation for eye-camera proxy;
   - Z adjustment for eye relief;
   - fine tip/tilt for projector/coupler where exposed;
   - repeatable glasses-like frame fixture.

5. **Eye/pupil proxy imaging**
   - camera with manual exposure/focus;
   - interchangeable ~3-4 mm aperture/pupil stop where practical;
   - translation range at least +/-10 mm X/Y around nominal pupil.

6. **Photometry**
   - calibrated luminance meter / imaging photometer preferred;
   - calibrated spot luminance measurement is acceptable with repeated stage positions;
   - a phone camera alone is not accepted for absolute cd/m2 gate measurements.

7. **Power and thermal**
   - bench supply or inline power analyzer with logging;
   - thermocouples or calibrated temperature probes at fixed contact locations;
   - IR camera optional for hotspot discovery, with emissivity limitations documented.

8. **Physical metrology**
   - digital scale;
   - calipers;
   - simple center-of-mass/balance jig;
   - ambient illuminance meter for repeatable readability conditions.

### 10.2 Suggested bench sequence

1. Bring up image with vendor/matched optics; do not optimize industrial design.
2. Lock deterministic test patterns and camera exposure.
3. Establish nominal eye relief and optical axis.
4. Measure usable FOV and Gate 1.
5. Map X/Y eyebox in 1 mm steps, refining edges to 0.5 mm if needed.
6. Run movement persistence sweeps and record video/dropout timestamps.
7. Measure nine-point luminance uniformity and artifact metrics.
8. Measure absolute representative-scene in-eye luminance.
9. Put the same optical stack into a glasses-like mass/geometry fixture.
10. Log 30-minute power, luminance retention, and contact temperatures.
11. Freeze the result as PASS/FAIL per gate; do not average away a failed gate.

## 11. Measurement record schema

Every run should record at minimum:

- run ID, date, operator;
- light-engine vendor/model/serial or sample ID;
- waveguide/coupler ID and revision;
- renderer resolution/refresh/test pattern;
- drive/brightness setting;
- ambient temperature and illuminance;
- eye relief;
- horizontal/vertical/diagonal usable FOV;
- measured eyebox width/height and full X/Y pass map;
- nine luminance values and calculated uniformity;
- representative-scene center luminance;
- RGB registration RMS;
- ghost/stray peak ratio;
- display-stack average/peak power;
- whole-worn average/peak power;
- temperatures at 0, 5, 10, 20, 30 minutes;
- luminance at stabilized start and 30 minutes;
- optical module dimensions/volume;
- total worn mass, left/right mass, temple thickness;
- each Gate 1-6 result with a linked evidence photo/video/log.

## 12. BOM / procurement envelope

Public list pricing is insufficient for a production BOM because current AR projectors and waveguides are commonly RFQ/evaluation products.

### v0.1 planning allowance — B

**KRW 2M-8M** for one monocular optical prototype, assuming photometric instruments can be borrowed, rented, or otherwise accessed rather than purchased new.

Categories:

- matched light engine / waveguide / dev hardware: primary cost;
- driver/bridge/control electronics;
- mechanical mounts and glasses-like fixtures;
- cables, adapters, power instrumentation consumables;
- optical test targets/apertures/ND material;
- measurement equipment rental/access if required.

### v0.2 planning allowance — A

**KRW 3M-12M+** for LBS + matched/custom HOE exploration. Custom HOE exposure/fabrication NRE and repeated optical iteration make the upper bound uncertain.

These are PR3 internal planning envelopes, not vendor quotes. A production BOM is blocked until:

1. the selected path passes the optical gates;
2. a realistic integrated architecture is frozen;
3. at least three relevant supplier quotes or equivalent sourcing records are obtained.

## 13. Evidence anchors reviewed 2026-09-27

### Official/vendor technical material

- JBD, Roadrunner II launch (2026-06-17): 800 x 600, 30 deg, 33.3 PPD, 6 lm, 0.18 cm3, 98 mW typical projector power, up to 6,000-nit in-eye claim with a 30 deg diffractive waveguide, and a monocular development-kit announcement. https://www.jb-display.com/newsdetails/88.html
- JBD current product site: Hummingbird II is listed at 0.5 g, 0.2 cm3, and 95 mW; useful as a secondary current MicroLED reference. https://www.jb-display.com/
- TriLite, Trixel 3 Cube official material (2025-2026): approximately 1 cm3, 1.5 g, up to 15 lm, ~145 mW typical power, integrated MEMS driver electronics, and engineering-sample availability. https://www.trilite-tech.com/trilite-unveils-trixel-3-cube-projection-display-for-ar-glasses-and-automotive-applications/
- TriLite, current Trixel 3 product flyer: <1 cm3, <1.5 g, >=30 deg FOV, 1024 x 768, 15 lm, 320 mW at the stated typical 20%-pixel-on / 5-lm operating condition. https://www.trilite-tech.com/wp-content/uploads/product-flyer-trixel-3.pdf
- DigiLens, Crystal30 G4 InsightKit: 30 deg diagonal, >3,000 nits to eye, 12 x 10 mm eyebox, 17 mm eye relief, 720 x 720, 60 fps, monocular option. https://www.digilens.com/wp-content/uploads/2025/07/DL-Crystal30-G4-v2.35.pdf
- RayNeo X3 Pro official specifications: binocular full-color MicroLED/diffractive-waveguide consumer reference, 640 x 480, 30 deg FOV, 3,500-nit average / 6,000-nit peak stated brightness, and 76 +/-1 g. https://www.rayneo.com/products/x3-pro-ai-display-glasses

### Research evidence

- Choi et al., *Synthetic aperture waveguide holography for compact mixed-reality displays with large etendue*, Nature Photonics (2025), DOI: 10.1038/s41566-025-01718-w. Demonstrated 38 deg diagonal FOV, 9 x 8 mm eyebox, and 23-33 mm eye-relief range in a research prototype. This supports the lower eyebox gate as physically meaningful; it does not authorize CGH in v0.1.
- Lyu et al., *2D Pupil Expansion Full-Color Volume Holographic Waveguide AR Display*, Laser & Photonics Reviews (first published 2025; 2026 issue), DOI: 10.1002/lpor.202502085. Reported 28 deg diagonal FOV, 14 x 16 mm eyebox, 15 mm eye relief, and 53.9% full-FOV white-light brightness uniformity. This is an evidence point for the difficulty of simultaneously achieving eyebox, color, and uniformity.
- Qin et al., *Hybrid Reflective-Diffractive Waveguide Display with High Optical Efficiency*, Laser & Photonics Reviews (2025). Reported a 30 deg prototype and 12 x 10 mm eyebox; used here as an additional research-scale feasibility point rather than a PR3 claim.

## 14. Blockers before procurement / build

1. **Matched hardware access.** Roadrunner II-class MicroLED projectors and appropriate waveguides are not normal retail parts; availability, lead time, and pricing need supplier confirmation.
2. **Public optical prescription gap.** Coupler/waveguide matching parameters are generally not public enough to justify buying unrelated loose parts.
3. **Absolute photometry.** Gate 4 cannot be credibly closed with a phone camera alone; calibrated luminance measurement access is required.
4. **BOM uncertainty.** Public RFQ products do not support a production BOM today.
5. **LBS human-use safety.** Path A requires laser eye-safety analysis before human-viewing experiments beyond supplier-certified evaluation conditions.
6. **Industrial-design targets remain internal.** 10-12 mm temple thickness and <=70/85 g mass must be validated after real parts are selected.

## 15. Freeze rule

Until Gates 1-6 pass, PR3 work may include:

- deterministic raster renderer/test patterns;
- static calibration and color/uniformity compensation experiments;
- bench alignment tooling;
- data logging and measurement automation;
- sourcing of matched evaluation hardware.

Do **not** add product implementation for:

- CGH rendering pipeline;
- dynamic holographic video;
- retinal projection;
- eye-tracked pupil steering;
- custom dynamic HOE/SLM product architecture;
- binocular product integration.

The next engineering decision after this spec is procurement/bench feasibility, not holographic feature implementation.
