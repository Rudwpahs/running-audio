# PR3 Optical Alpha v0.1 Spec

Date: 2026-09-27
Status: **FINAL FOR BENCH FEASIBILITY**
Scope: near-eye see-through optical feasibility alpha only. No CGH product implementation, retinal-projection product path, dynamic holographic video, eye-tracked pupil steering, or binocular product integration before the eight gates in this document pass.

## 1. Purpose and decision

PR3 Optical Alpha v0.1 answers one question:

> Can a glasses-like, monocular near-eye optical chain produce a stable, readable image with sufficient eyebox, movement tolerance, brightness, uniformity, artifact control, size, power, and thermal behavior to justify further PR3 product work?

The Alpha is **not** a finished pair of glasses. Its purpose is optical feasibility and measurement discipline.

Common optical chain:

`Phone / compute -> renderer -> light source/display -> collimation / beam conditioning -> coupling -> HOE or waveguide -> eye / camera`

Architecture decision for v0.1:

- **Monocular first.** Binocular is a later promotion step.
- **Primary path B: microdisplay + matched waveguide.** Full-color MicroLED is preferred when accessible.
- **Path A: LBS + HOE remains the comparison path.** It is not rejected; it is deferred as the default first build because it couples laser wavelength, MEMS scan calibration, HOE Bragg/angular matching, speckle, and laser-safety variables into the first experiment.
- Prefer a **matched projector + waveguide / HOE evaluation path** over a loose projector and unrelated combiner.
- A matched LCoS + waveguide kit may be used to close optical Gates 1-6 when MicroLED access is blocked, but its size/power result does not close Gates 7-8 for the intended MicroLED path.

The choice of B is therefore an **alpha-risk-isolation decision**, not a statement that LBS is inferior in long-term size, power, or brightness.

## 2. Evidence boundary

Vendor specifications establish sourcing feasibility and engineering reference points only. Research prototypes establish physically demonstrated ranges only. Neither is a PR3 result.

Rules:

1. Do not combine values from different product revisions or operating conditions into one imaginary configuration.
2. Do not treat projector brightness or lumens as in-eye brightness.
3. Do not use a phone camera as an absolute luminance instrument.
4. Do not claim eyebox from a vendor number alone; measure the contiguous usable region on the PR3 bench.
5. Do not claim optical efficiency unless input luminous flux and output luminance are measured or supplied under compatible conditions.
6. Every gate must be closed on one coherent Alpha configuration; passing different gates on unrelated setups is not a full Alpha pass.

## 3. PR3 Optical Alpha v0.1 target specification

| Parameter | Target | Minimum gate / rule | Measurement |
| --- | ---: | ---: | --- |
| Display strategy | monocular see-through | monocular see-through | one display eye |
| Primary content | text, icon, arrow, short status card | same | no cinematic/video requirement |
| Diagonal usable FOV | 30 deg | >=25 deg | measure H/V/D usable region |
| Eyebox | 12 x 10 mm | >=9 x 8 mm | contiguous usable region |
| Nominal eye relief | 18-20 mm | >=15 mm | pupil plane to last optical surface |
| Addressable resolution | 800 x 600 class | >=640 x 480 class | source raster; also report MTF if available |
| Effective angular resolution | >=28 PPD | >=22 PPD | center region, axis definition recorded |
| Representative 20% white UI, in-eye luminance | >=3,000 cd/m2 | >=1,500 cd/m2 | nominal pupil position |
| Stretch in-eye luminance | >=5,000 cd/m2 | not required | only if power/thermal remain valid |
| Optical luminous efficiency | >=750 nits/lm average preferred | diagnostic unless calibrated input lm available | report average nits/lm; center-only value must be labeled |
| White-field luminance uniformity | >=70% | >=55% | Lmin/Lmax, 9-point FOV grid |
| RGB registration, central 60% FOV | <=1.5 px RMS | <=3 px RMS | RGB edge / white-grid target |
| Secondary ghost/stray peak, central 60% | <=5% | <=10% | relative to primary feature, same exposure |
| Optical engine + collimation/coupling bbox | <=25 x 15 x 8 mm | <=30 x 20 x 10 mm | waveguide/HOE substrate excluded |
| Engine-side optical volume | <=3 cm3 | <=6 cm3 | off-head controller excluded |
| Combiner footprint | <=60 x 45 mm preferred | <=65 x 50 mm | width x height of worn HOE/waveguide |
| Combiner thickness | <=2.0 mm preferred | <=3.0 mm | maximum worn optical substrate stack |
| Display-side temple local thickness | <=10 mm | <=12 mm | future integration envelope |
| Whole worn Alpha mass | <=70 g | <=85 g | off-head controller excluded only if truly off-head |
| Display stack average electrical power | <=0.6 W | <=1.0 W | representative UI workload |
| Whole worn electronics average power | <=1.0 W | <=1.5 W | phone compute excluded |
| Whole worn electronics short peak | <=1.5 W | <=2.0 W | duration/workload logged |
| Skin/contact surface after 30 min | <=42 C | <45 C | 25 +/-2 C ambient |
| Continuous run | >=30 min | >=30 min | no unstable image or thermal collapse |

The size, mass, power, thermal, artifact, and alignment thresholds are PR3 internal Alpha engineering criteria, not industry standards.

## 4. Standardized comparison: A vs B

### Candidate A — LBS + HOE

Chain:

`renderer -> RGB laser diodes -> MEMS scanner / LBS engine -> beam conditioning / optional relay -> matched HOE incoupling / combiner -> eye`

Current reference class: TriLite Trixel 3 / Trixel 3 Cube.

### Candidate B — microdisplay + waveguide

Chain:

`renderer -> full-color microdisplay projector -> collimator -> input coupler -> waveguide / pupil expansion -> output coupler -> eye`

Preferred light-engine class: full-color MicroLED. Current references include JBD Roadrunner II / Hummingbird II and matched waveguide kits such as DigiLens Crystal30 G4.

### Comparison table

| Metric | A. LBS + HOE | B. microdisplay + waveguide | PR3 standardization |
| --- | --- | --- | --- |
| FOV | Current Trixel material is ~24 x 18 deg to >=30 deg class depending revision/material | Roadrunner II 30 deg; Crystal30 G4 30 deg | report H/V/D usable FOV on final coupled setup |
| Eyebox | Determined mainly by HOE/exit-pupil-expansion design; no single engine number | Current matched waveguide references reach ~10 x 10 to 12 x 10 mm | map contiguous usable X/Y region at fixed eye relief |
| Eye relief | HOE/system dependent | Crystal30 reference 17 mm; recent research commonly ~15 mm+ | gate >=15 mm; target 18-20 mm |
| Resolution | Trixel public material spans XGA-class / 1024 x 768-class depending current revision | Roadrunner II 800 x 600; Hummingbird II 500 x 380; Crystal30 InsightKit 720 x 720 | use actual addressable source and effective PPD |
| Brightness | Engine output up to 15 lm; in-eye brightness depends strongly on HOE and EPE | Roadrunner II vendor reference up to 6,000 nits with 30-deg waveguide; Crystal30 >3,000 nits to eye | calibrated in-eye cd/m2 at nominal pupil |
| Optical efficiency | Public matched LBS+HOE system value not sufficiently standardized for PR3 | Crystal30 waveguide reference: >750 nits/lm average unpolarized, >1500 nits/lm average polarized; 30-deg full-color research/commercial references are ~1300 nits/lm class and above | when calibrated input lumens exist, report average nits/lm; otherwise do not invent a value |
| Image uniformity | Sensitive to HOE recording, Bragg detuning, scan calibration, wavelength | Waveguide EPE efficiency/uniformity tradeoff is a central risk; research full-color VHOE result 53.9% white-field uniformity; Roadrunner projector material reports >95% source-image brightness uniformity but that is not the final waveguide uniformity | 9-point final-system Lmin/Lmax |
| Rainbow/artifact | Speckle, coherent stray light, HOE ghost orders, scan distortion | Diffractive rainbow, color nonuniformity, pupil-dependent ghost/stray orders | fixed exposure, RGB registration, ghost-peak ratio, photos at fixed pupil points |
| Light-engine size | Trixel 3 / Cube approximately <=1 cm3 class, ~1.5 g | Roadrunner II 0.18 cm3; Hummingbird II 0.2 cm3 and 0.5 g | measure actual engine + mandatory coupling optics, not bare chip only |
| HOE/waveguide size | Custom/sample dependent; lens-like substrate required for wearable Alpha | Crystal30 G4 public reference is about 59 x 45 mm, 1.2 mm thick, 6 g | report W x H x T and mass of worn combiner |
| Alignment tolerance | High integration sensitivity: laser wavelength, scanner geometry, HOE angle/Bragg condition | High projector/coupler pupil sensitivity; substantially reduced by matched/keyed kits | perform relative-pose sweep and 3x reseat repeatability test |
| Power | Trixel 3 Cube current official material ~145 mW typical; other current Trixel 3 operating points <320 mW / ~320 mW depending definition | Roadrunner II ~98 mW typical projector; Hummingbird II 95 mW typical | measure complete display stack, not engine only |
| Thermal | Engine low-power potential is strong; laser/driver hotspots must be mapped | Engine power is very low but bridge/driver/system can dominate | 30-min contact-surface and component logging |
| Weight | LBS engine ~1.5 g class; HOE/mount/driver determine system | Hummingbird II 0.5 g; commercial binocular full-color system reference RayNeo X3 Pro is 76 +/-1 g total | weigh engine, combiner, mounts, driver, and whole worn Alpha separately |
| BOM | LBS samples obtainable; custom/matched HOE and calibration create uncertainty | Projector/dev-kit ecosystem stronger; matched waveguide remains B2B/RFQ | use procurement envelope until quotes arrive |
| Component access | TriLite engineering samples/evaluation kit publicly offered; custom HOE is the hard part | JBD projector/dev path and DigiLens InsightKit-class matched routes exist; pricing/lead time commonly RFQ | supplier confirmation required before buy |
| Fabrication difficulty | Very high if custom HOE is required | Medium-high with matched kit; very high if fabricating waveguide itself | v0.1 does not fabricate multilayer waveguide or custom dynamic holographic optics |
| v0.1 suitability | comparison / fallback if matched LBS+HOE system is available | **selected baseline** | choose the setup that removes the most unknown optical variables |

### Alignment tolerance normalization

Do not compare alignment tolerance using vague words such as "easy" or "tight" only.

For either A or B, after nominal optimization measure:

- lateral source-to-combiner tolerance: `Delta x80`, `Delta y80`;
- axial tolerance: `Delta z80`;
- angular tolerance: `pitch80`, `yaw80`, and roll if relevant;
- each `80` value is the positive/negative range within which center luminance remains >=80% of nominal, usable FOV remains >=90% of nominal, and no new Gate-6 artifact failure appears;
- perform three remove/reseat cycles using only mechanical fiducials/stops. After each reseat, center luminance must recover within +/-20%, usable FOV >=90% of baseline, and eyebox area >=80% of baseline without re-fabricating optics.

These are bench integration metrics, not claims about production tolerance stacks.

## 5. Optical efficiency definition

Preferred system metric:

`luminous efficiency = average in-eye luminance over the defined field/eyebox sample set / calibrated light-engine luminous flux`

Unit: `nits/lm`.

Rules:

- use input luminous flux measured at the light-engine output or supplied for the exact operating point;
- if only center luminance is available, report `center nits/lm` and do not compare it directly with an average IDMS-like value;
- if input lumens are unknown, optical efficiency remains `NOT MEASURED`, not estimated from electrical power;
- power efficiency may additionally be reported as `center nits/W` or `average nits/W`, but this is a different metric.

Reference context:

- DigiLens Crystal30 G4 public material lists average luminance efficiency >750 nits/lm for unpolarized engines and >1500 nits/lm for polarized engines, with higher peak values.
- Light: Science & Applications (2024) reports a 30-deg full-color SRG-waveguide example around 1300 nits/lm (~3%) and a 20-deg example around 4500 nits/lm (~10%), illustrating the FOV-efficiency tradeoff.
- A 2025 hybrid reflective-diffractive research prototype reported 2556.9 nits/lm average at 30 deg with a 12 x 10 mm eyebox.

PR3 v0.1 does not make optical efficiency a standalone hard gate when input flux cannot be calibrated. Brightness, thermal, and power remain mandatory gates.

## 6. Image-quality definitions and test patterns

### Required deterministic patterns

Use the same files for A and B:

1. full white;
2. 20% area centered white UI card on black;
3. white Korean/Latin text on black;
4. 1-pixel / 2-pixel geometric grid;
5. RGB edge-registration target;
6. grayscale ramp and 10-step gray bars;
7. RGB primaries + white patches;
8. isolated white point and isolated white line on black for ghost/stray detection;
9. checkerboard / line-pair target for effective sharpness;
10. corner markers for clipping/FOV mapping.

All comparison images must use fixed renderer scale, gamma/profile, source drive setting, and camera exposure unless a test explicitly varies them.

### Usable FOV

Report horizontal, vertical, and diagonal FOV. A region is not counted as usable if clipping, severe blur, severe color separation, or luminance loss prevents reference text/grid recognition.

### Eyebox

At the nominal eye-relief plane, map X/Y positions. A sample point is usable only if all are true:

- center text is visible;
- >=80% of the intended frame remains unclipped;
- local white-field luminance is >=50% of nominal-center luminance;
- severe color breakup does not prevent reading.

The reported eyebox is the largest contiguous usable region.

### Uniformity

Use a fixed 9-point FOV grid: center, four cardinal midpoints, four corners.

`white-field uniformity = minimum measured luminance / maximum measured luminance`

Retain all nine raw values.

### Artifact metrics

At nominal pupil plus the four main eyebox-edge positions, record:

- RGB registration RMS in pixels;
- strongest secondary ghost/stray peak divided by primary peak under identical exposure;
- whether a secondary readable copy of a glyph appears;
- qualitative rainbow/speckle image at fixed exposure, tagged with pupil coordinates;
- source-specific notes: speckle for LBS; rainbow/color walkoff for diffractive waveguides.

## 7. Eight Optical Gates

No CGH product path, retinal projection, dynamic holographic video, eye-tracked pupil steering, custom dynamic SLM/HOE architecture, or binocular product integration is admitted before **all eight gates** pass on one coherent Alpha configuration.

### Gate 1 — visible image

Measurement:

- nominal eye position and eye relief;
- full RGB, text, grid, and corner-marker patterns;
- 10-minute static run after alignment lock.

PASS when:

- Korean and Latin text are readable;
- grid/corner markers are recognizable over the usable region;
- diagonal usable FOV >=25 deg;
- no continuous manual optical adjustment is required during the 10-minute run;
- no complete image loss or unstable scan/frame behavior occurs.

Target: >=30 deg usable diagonal FOV.

### Gate 2 — eyebox

Measurement:

- camera/pupil proxy at nominal eye-relief plane;
- 3-4 mm pupil stop where practical;
- sweep X/Y in 1 mm steps; refine boundaries to 0.5 mm;
- retain full pass/fail map and local luminance.

PASS when contiguous usable eyebox is:

- width >=9 mm;
- height >=8 mm;
- eye relief >=15 mm.

Target: >=12 x 10 mm at 18-20 mm eye relief.

### Gate 3 — head/eye movement tolerance

Bench measurement:

- traverse the eye-camera proxy through at least +/-4.5 mm X and +/-4.0 mm Y around nominal pupil;
- repeat at least three horizontal and three vertical traversals;
- also check Z at nominal eye relief +/-2 mm where fixture travel allows;
- record video and timestamps.

PASS when:

- no complete image dropout >100 ms occurs inside the accepted eyebox;
- total complete-dropout time is <1% of traverse time inside the accepted region;
- reference text remains readable during normal traverse speed;
- ordinary small frame motion does not require one exact head pose.

### Gate 4 — brightness

Measurement:

- calibrated luminance meter or imaging photometer at nominal pupil;
- required scenes: full white, 20% white UI card, white text;
- record source drive, ambient lux, eye relief, and pupil aperture;
- use controlled lab condition for comparison and separately record practical readability at ~300-500 lux indoor, ~2,000-5,000 lux bright/window condition, and ~10,000-20,000 lux outdoor shade when available.

PASS when:

- representative 20% white UI center luminance >=1,500 cd/m2;
- after the 30-minute thermal run it remains >=80% of the stabilized early-run value **and** remains above 1,500 cd/m2;
- text remains practically readable indoors, bright/window condition, and outdoor shade.

Target: >=3,000 cd/m2 center. Direct-sun readability is a stretch test, not a v0.1 requirement.

### Gate 5 — image uniformity

Measurement:

- full-white pattern;
- fixed 9-point FOV grid;
- same drive and pupil position;
- repeat at center pupil and, if time allows, four eyebox-edge pupil locations.

PASS when:

- center-pupil white-field `Lmin/Lmax >=0.55`;
- no single sampled corner or edge is so dim that reference text becomes unreadable;
- the result is recorded with all nine raw luminance values.

Target: `Lmin/Lmax >=0.70`.

### Gate 6 — artifact / rainbow / ghost

Measurement:

- RGB edge target, white grid, isolated point, isolated line;
- fixed manual exposure;
- nominal pupil plus four principal eyebox-edge positions.

PASS when in the central 60% usable FOV:

- RGB edge-registration RMS <=3 px;
- strongest secondary ghost/stray peak <=10% of the primary peak under the same exposure;
- no second readable copy of a reference glyph is formed;
- no persistent rainbow/color breakup prevents reading reference text;
- for LBS, speckle must not erase thin reference strokes or create false readable detail.

Target: RGB registration <=1.5 px RMS and ghost/stray peak <=5%.

### Gate 7 — optical module size / mass feasibility

Measurement:

- measure the real engine + mandatory collimation/coupling hardware bounding box;
- measure combiner width, height, thickness, and mass;
- place optical components in a glasses-like geometric fixture, even if controller electronics remain off-head;
- record total worn Alpha mass and display-side local temple thickness.

PASS when:

- engine-side optical bbox <=30 x 20 x 10 mm and <=6 cm3;
- combiner footprint <=65 x 50 mm and thickness <=3.0 mm;
- display-side temple local max <=12 mm for the modeled/integrated optical position;
- whole worn Alpha mass <=85 g.

Target: engine-side bbox <=25 x 15 x 8 mm / <=3 cm3, combiner <=60 x 45 x 2 mm, temple <=10 mm, whole worn Alpha <=70 g.

This is a feasibility geometry gate, not an industrial-design freeze.

### Gate 8 — power / thermal feasibility

Measurement:

- ambient 25 +/-2 C;
- representative 20% white UI workload;
- 30-minute continuous run;
- log display-engine power, driver/bridge power where separable, whole worn power, center luminance, and temperatures at 0, 5, 10, 20, and 30 minutes;
- fixed temperature points: engine body, driver/bridge hotspot, display-side temple/contact surface, opposite-side reference surface.

PASS when:

- display stack average <=1.0 W;
- whole worn electronics average <=1.5 W, excluding phone compute;
- short peak <=2.0 W;
- no ordinary skin/contact surface reaches 45 C;
- no reboot, image dropout, optical drift, or thermal brightness collapse occurs;
- 30-minute reference luminance remains >=80% of stabilized early-run value and Gate 4 remains passed.

Targets: display stack <=0.6 W average, whole worn <=1.0 W average, peak <=1.5 W, contact surfaces <=42 C.

## 8. Alpha bench experiment

### 8.1 Physical optical layout

Common bench:

`light source/display -> collimation / beam conditioning -> input coupling -> HOE/waveguide propagation / pupil expansion -> outcoupling -> eye/camera pupil plane`

#### Bench A — LBS + HOE

`PC/phone pattern -> Trixel-class RGB LBS -> integrated beam shaping / optional relay -> matched HOE incoupling/combiner -> pupil plane camera`

Notes:

- if the selected LBS already provides the required beam conditioning, do not add relay optics merely to match the generic diagram;
- use supplier-recommended wavelengths/drive and certified evaluation conditions;
- human viewing beyond certified evaluation conditions is blocked until laser eye-safety review is complete;
- enclose or shield uncontrolled laser paths and use appropriate lab safety procedures.

#### Bench B — microdisplay + waveguide

`PC/phone pattern -> MicroLED/LCoS projector -> collimator -> matched input coupler -> waveguide + EPE/outcoupler -> pupil plane camera`

Notes:

- prefer a vendor-matched projector/collimator/waveguide path;
- do not buy an arbitrary loose waveguide before input-pupil, polarization, angular, spectral, and mechanical compatibility are confirmed.

### 8.2 Mechanical layout

Minimum stages:

- rigid optical breadboard/rail or equivalent stiff base;
- source/projector mount with XYZ and fine pitch/yaw where the kit does not key the pose;
- combiner mount with repeatable fiducials/stops;
- eye/camera XYZ translation stage with >= +/-10 mm X/Y travel and eye-relief Z adjustment;
- 3-4 mm pupil-stop holder;
- glasses-like surrogate fixture for Gate 7.

Motorized stages are not required for v0.1. Manual micrometer stages are acceptable if positions are recorded.

### 8.3 Bench sequence

1. **Incoming inspection** — record part revisions, dimensions, weight, optical interface, driver firmware, mechanical drawings, and supplier operating limits.
2. **Display-only bring-up** — verify deterministic test patterns before the combiner.
3. **Nominal coupling** — establish vendor/reference geometry, optical axis, polarization, and nominal eye relief.
4. **Gate 1** — visible image and usable FOV.
5. **Gate 2** — eyebox X/Y map.
6. **Gate 3** — movement traverse and Z robustness.
7. **Gate 4** — absolute luminance.
8. **Gate 5** — 9-point uniformity.
9. **Gate 6** — ghost/rainbow/RGB registration/speckle record.
10. **Alignment sensitivity** — determine Delta x80 / Delta y80 / Delta z80 / pitch80 / yaw80 and perform 3x reseat test.
11. **Optical efficiency** — measure nits/lm only if calibrated input flux is available.
12. **Gate 7** — glasses-like geometry, combiner/engine dimensions, worn mass.
13. **Gate 8** — 30-minute power/thermal/luminance-retention run.
14. Freeze the run as PASS/FAIL per gate. Do not average away a failed gate.

## 9. Parts for Alpha

### 9.1 Required before a meaningful optical Alpha

- one **matched optical path**:
  - preferred B: MicroLED projector + compatible/matched waveguide/dev kit;
  - A comparison: LBS engineering sample/eval kit + matched HOE/combiner path;
- vendor driver/control electronics and SDK or image-input method;
- deterministic test-pattern source (PC, phone, SBC);
- rigid optical base and repeatable mounts;
- projector/source fine alignment capability if not vendor-keyed;
- XYZ + Z eye/camera stage;
- manual-exposure camera with fixed-focus/fixed-exposure operation;
- 3-4 mm pupil stop/aperture;
- calibrated luminance meter or imaging photometer access;
- inline power analyzer or logged bench supply;
- thermocouples / calibrated contact temperature probes;
- digital scale and calipers;
- ambient lux meter;
- for A: supplier safety documentation plus appropriate laser-safe bench controls.

### 9.2 Substitutable without invalidating early optical feasibility

- MicroLED may be temporarily replaced by a matched LCoS/DLP projector to close Gates 1-6; Gates 7-8 must later be repeated with the intended engine class.
- imaging photometer may be replaced by a calibrated spot luminance meter plus repeated stage positions.
- motorized translation may be replaced by manual micrometer stages.
- optical breadboard may be replaced by a sufficiently rigid machined/jig fixture.
- PC may be replaced by phone/SBC if deterministic pixels and drive settings are preserved.
- IR camera may supplement but not replace contact probes for the thermal gate.

### 9.3 Later / explicitly not needed for v0.1

- custom CGH renderer;
- phase-only SLM;
- dynamic holographic video pipeline;
- retinal projection hardware;
- eye tracker / pupil steering;
- binocular second optical channel;
- custom prescription integration;
- custom consumer frame tooling;
- custom battery / production PCB;
- custom multilayer waveguide fabrication;
- custom dynamic HOE recording system.

## 10. Purchase-before-check specification list

Do not purchase a light engine or combiner until the following fields are known or explicitly marked unavailable.

### Light engine / projector

- exact model and revision;
- display technology: LBS / MicroLED / LCoS / DLP;
- resolution and refresh rate;
- supported FOV with the intended projection optics;
- luminous flux at specified pattern / drive point;
- projector brightness-uniformity definition;
- exit-pupil diameter / EPD where applicable;
- virtual-image distance / focus distance;
- output polarization state and requirements;
- output wavelength(s) / spectral bandwidth;
- required collimator / projection lens and whether included;
- optical-interface drawing: chief-ray angle, pupil location, pupil size, NA/F-number where available;
- engine dimensions, weight, connector/flex keep-out;
- electrical input, typical/peak power, and the exact workload used to quote power;
- operating temperature and thermal recommendations;
- MIPI/HDMI/USB/other interface;
- SDK, driver-board, firmware, and test-pattern access;
- engineering-sample/evaluation-kit availability;
- lead time, MOQ, sample price/quote, and return restrictions.

### Waveguide / HOE / combiner

- matched light-engine class and approved projector list;
- FOV H/V/D;
- eyebox definition and dimensions;
- eye relief;
- input pupil size/location and required chief-ray angle;
- polarization requirements;
- wavelength / spectral bandwidth;
- average and peak luminance efficiency, including whether reported as nits/lm and how averaged;
- brightness and color uniformity definition;
- MTF/sharpness metric;
- transparency;
- eye glow / stray-light metric if available;
- substrate dimensions, thickness, weight;
- in-coupler position/size and mechanical datum drawing;
- allowed source-coupler alignment tolerance or calibration procedure;
- required adhesive/mounting restrictions;
- environmental limits;
- sample availability and whether the combiner is supplied pre-aligned to a projector.

### Additional checks for A — LBS + HOE

- exact RGB wavelengths and wavelength tolerances;
- laser classification / eye-safety documentation for the supplied evaluation condition;
- scan trajectory calibration method;
- distortion correction / TCM equivalent;
- HOE recording wavelengths and Bragg incidence geometry;
- speckle mitigation method if any;
- supplier end-to-end calibration availability.

### Additional checks for B — microdisplay + waveguide

- microdisplay pixel pitch and projector MTF;
- X-cube / color-combiner alignment if full-color projector uses separate RGB panels;
- waveguide compatibility with unpolarized vs polarized source;
- input-coupler pupil match;
- whether quoted in-eye nits are center, average, or peak and at what source lumens;
- whether projector + waveguide can be purchased as one matched evaluation configuration.

## 11. Measurement record schema

Every run records at minimum:

- run ID, date, operator;
- path A/B;
- light-engine model/revision/sample ID;
- combiner model/revision/sample ID;
- collimator/coupler revision;
- renderer resolution, refresh, test-pattern file/version;
- drive/brightness setting;
- ambient temperature and illuminance;
- eye relief and pupil-stop diameter;
- H/V/D usable FOV;
- X/Y eyebox pass map;
- movement traverse videos/timestamps;
- nine luminance values and calculated uniformity;
- 20% UI center luminance;
- input lumens if measured and resulting nits/lm metric;
- RGB registration RMS;
- ghost/stray peak ratio;
- artifact photos keyed to pupil coordinates;
- Delta x80 / Delta y80 / Delta z80 / pitch80 / yaw80 where measured;
- three reseat results;
- engine/optics dimensions and volume;
- combiner W/H/T and mass;
- display-side temple thickness in surrogate geometry;
- total worn mass;
- display-stack average/peak power;
- whole-worn average/peak power;
- temperatures at 0, 5, 10, 20, 30 minutes;
- luminance at stabilized start and 30 minutes;
- Gate 1-8 PASS/FAIL with linked evidence file.

## 12. BOM / procurement envelope

Public pricing is insufficient for a production BOM because current AR projectors and combiners are commonly B2B/RFQ components.

### B — microdisplay + matched waveguide

Planning envelope for one monocular optical Alpha: **KRW 2M-8M**.

Assumption: calibrated photometry is borrowed, rented, or accessed through a lab rather than purchased as a new high-end imaging photometer.

Cost categories:

- matched projector/waveguide/dev hardware — primary unknown and likely largest line;
- driver/bridge/control electronics;
- optical/mechanical stages and fixtures;
- pupil proxy/camera hardware if not already available;
- power/thermal instrumentation;
- test targets, apertures, cables, adapters, ND/optical consumables;
- metrology rental/access.

### A — LBS + matched/custom HOE

Planning envelope: **KRW 3M-12M+**.

Uncertainty is higher because custom/matched HOE recording, exposure/fabrication NRE, laser-safe integration, and repeated calibration can dominate cost.

These are internal planning envelopes, not vendor quotations. A production BOM is blocked until:

1. one optical path passes all eight gates;
2. the integrated architecture is frozen;
3. at least three relevant supplier quotations or equivalent sourcing records are collected.

## 13. Evidence anchors reviewed 2026-09-27

### Official / vendor technical material

- JBD, **Roadrunner II** launch, 2026-06-17: 800 x 600, 30-deg class, 33.3 PPD, 6 lm, 98 mW typical projector power, up to 6,000-nit in-eye claim with a 30-deg diffractive waveguide; source-image brightness uniformity >95% in the published launch material. https://www.jb-display.com/newsdetails/88.html
- JBD, **Hummingbird II** product page: 500 x 380, 25 deg, 0.2 cm3, 0.5 g, 3 lm, 95 mW typical, up to 4,000-nit eye-level brightness with waveguide. https://www.jb-display.com/product_des/17.html
- TriLite, **Trixel 3 Cube**: approximately 1 cm3, 1.5 g, up to 15 lm, ~145 mW typical in the integration-facing Cube material, integrated MEMS driver electronics, engineering samples. https://www.trilite-tech.com/trilite-unveils-trixel-3-cube-projection-display-for-ar-glasses-and-automotive-applications/
- TriLite, **Trixel 3** current product/engineering material: <1 cm3, ~1.5 g, 15 lm, sub-320/320-mW-class stated operating points depending public document/revision; engineering sample and evaluation kit available. https://www.trilite-tech.com/product/ and https://www.trilite-tech.com/wp-content/uploads/product-flyer-trixel-3.pdf
- DigiLens, **Crystal30 G4**: 30-deg diagonal, 17 mm eye relief, ~59 x 45 mm reference footprint, 1.2 mm thickness, 6 g, >90% transparency, average luminance efficiency >750 nits/lm unpolarized and >1500 nits/lm polarized in the 4th-generation waveguide sheet. https://www.digilens.com/wp-content/uploads/2024/10/DL-Crystal30-4th-Gen-090324-08.pdf
- DigiLens, **Crystal30 G4 InsightKit**: matched waveguide + Avegant LCoS configuration, 30-deg diagonal, >3,000 nits to eye, 12 x 10 mm eyebox, 17 mm eye relief, 720 x 720, 60 fps, monocular option. https://www.digilens.com/wp-content/uploads/2025/07/DL-Crystal30-G4-v2.35.pdf
- RayNeo, **X3 Pro** commercial system reference: binocular full-color MicroLED + diffractive waveguide, 640 x 480, 30 deg, 3,500-nit average / 6,000-nit peak stated brightness, 76 +/-1 g total system weight. https://www.rayneo.com/products/x3-pro-ai-display-glasses

### Research evidence

- Ding et al., **Breaking the in-coupling efficiency limit in waveguide-based AR displays with polarization volume gratings**, Light: Science & Applications 13, 185 (2024), DOI: 10.1038/s41377-024-01537-8. Discusses the efficiency/uniformity/FOV tradeoff and cites 20-deg ~4500 nits/lm (~10%) vs 30-deg ~1300 nits/lm (~3%) full-color SRG-waveguide examples.
- Choi et al., **Synthetic aperture waveguide holography for compact mixed-reality displays with large etendue**, Nature Photonics (2025), DOI: 10.1038/s41566-025-01718-w. Demonstrated 38-deg diagonal FOV, 9 x 8 mm eyebox, and 23-33 mm eye-relief range. It is used only as physical evidence for gate ranges; it does not authorize CGH implementation in v0.1.
- Lyu et al., **2D Pupil Expansion Full-Color Volume Holographic Waveguide AR Display**, Laser & Photonics Reviews, DOI: 10.1002/lpor.202502085. Reported 28-deg diagonal, 14 x 16 mm eyebox, 15 mm eye relief, and 53.9% full-FOV white-light brightness uniformity.
- Qin et al., **Hybrid Reflective-Diffractive Waveguide Display with High Optical Efficiency**, Laser & Photonics Reviews, DOI: 10.1002/lpor.202500957. Reported 30-deg FOV, 12 x 10 mm eyebox, and 2556.9 nits/lm average optical efficiency in a research prototype.
- Li et al., **Eye-Box Measurement for Augmented-Reality Waveguides with Pupil Expansion**, SID Symposium Digest (2025), DOI: 10.1002/sdtp.18232. Supports treating eyebox as a measured 3D usable region rather than a single nominal vendor rectangle.
- **Prediction of assembly accuracy in multilayer diffractive optical waveguides based on virtual assembly**, Optics Communications 593 (2025), 132196, DOI: 10.1016/j.optcom.2025.132196. Highlights the severe assembly-accuracy burden in multilayer diffractive-waveguide fabrication; PR3 v0.1 therefore buys/uses a matched combiner rather than fabricating such a stack.
- Lin et al., **Bragg Condition Matching Technique between Volume Holographic Optical Elements in Light Field Near-Eye Displays**, Optica DH 2026, Th2C.2. Reinforces Bragg-condition sensitivity in multi-HOE systems and the need to treat angular/material variation explicitly in A.

## 14. Current blockers

1. **Matched hardware availability** — Roadrunner II-class projectors and suitable waveguides are not ordinary retail parts; actual availability, lead time, and price need supplier confirmation.
2. **Optical-interface data** — loose waveguide purchases are unsafe until pupil position/size, chief-ray angle, polarization, spectrum, and mechanical datums are known.
3. **Absolute photometry access** — Gate 4 cannot be credibly closed with an iPhone camera alone.
4. **Optical-efficiency instrumentation** — nits/lm comparison requires calibrated input luminous flux; without it the metric remains unmeasured.
5. **A-path laser safety** — human viewing beyond supplier-certified evaluation conditions requires laser eye-safety review and appropriate lab controls.
6. **BOM uncertainty** — current B2B/RFQ components do not support a credible production BOM yet.
7. **Mechanical integration evidence** — the 10-12 mm temple and <=70/85 g goals remain internal targets until real parts are mounted in a glasses-like surrogate.

## 15. Freeze rule and next engineering step

Until Gates 1-8 pass, PR3 work may include:

- deterministic raster renderer/test patterns;
- static calibration and color/uniformity compensation;
- bench alignment tooling;
- measurement logging/automation;
- supplier outreach and matched-evaluation-hardware sourcing;
- passive glasses-like surrogate mounting for Gate 7.

Do **not** start product implementation for:

- CGH rendering pipeline;
- dynamic holographic video;
- retinal projection;
- eye-tracked pupil steering;
- custom dynamic HOE/SLM product architecture;
- binocular product integration.

The next engineering decision after this spec is:

**obtain one matched B-path evaluation configuration (or a matched A-path system if supplier access is unexpectedly better) -> build the bench -> close Gate 1 first -> proceed sequentially through Gate 8 -> only then decide whether PR3 advances to a wearable product alpha.**