# PR3 Optical Alpha v0.1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Turn the approved PR3 Optical Alpha v0.1 spec into a procurement, bench-measurement, data-analysis, and gate-report workflow that can determine whether a compact monocular MicroLED + matched-waveguide path is viable before any CGH, retinal-projection, or dynamic-holographic product work.

**Architecture:** Use a matched monocular MicroLED projector + waveguide evaluation path for v0.1. Separate vendor/component evidence, renderer/test-pattern assets, bench measurements, quantitative analysis, and gate decisions so each can be reviewed independently. LBS + HOE remains a later v0.2 comparison and is not implemented in this plan.

**Tech Stack:** Vendor evaluation hardware, monocular optical bench/fixture, camera/pupil proxy on X/Y translation, photometric measurement access, power meter, thermal measurement, fixed test patterns, CSV/JSON measurement records, Python 3 for repeatable analysis and plots.

**Spec:** `docs/superpowers/specs/2026-09-25-pr3-optical-alpha-v0.1-design.md`

## Global Constraints

- Monocular see-through only for v0.1.
- Selected v0.1 path: MicroLED microdisplay/projector + matched waveguide.
- Do not implement CGH, retinal projection, dynamic holographic video, pupil steering, or a custom product enclosure.
- Target diagonal FOV 30°; minimum gate 25°.
- Target eyebox 12 x 10 mm; minimum gate 9 x 8 mm.
- Target eye relief 18-20 mm; minimum 15 mm.
- Target resolution 800 x 600 class; minimum 640 x 480 class.
- Target angular resolution >=28 PPD; minimum >=22 PPD.
- Target center eye-level brightness >=3,000 nit; minimum gate >=1,500 nit.
- White-field luminance uniformity target >=0.70; minimum gate >=0.55 using `Lmin / Lmax` on the fixed 9-point grid.
- Target optical module <=25 x 15 x 8 mm and <=3 cm^3; minimum <=30 x 20 x 10 mm and <=6 cm^3.
- Target display-side temple <=10 mm; minimum <=12 mm.
- Target worn mass <=70 g; minimum <=85 g.
- Target display-stack average power <=0.6 W; minimum <=1.0 W.
- Target whole worn electronics average power <=1.0 W; minimum <=1.5 W.
- Target skin-contact temperature <=42°C; hard gate <45°C after 30 minutes at ~25°C ambient.
- Vendor claims and literature values are inputs, not PR3 measured results.

## Repository / File Structure

This is a documentation/analysis execution plan. No PR3 product-code repository is assumed to exist.

```text
docs/pr3/alpha-v0.1/
  VENDOR_SHORTLIST.md
  RFQ_TEMPLATE.md
  PROCUREMENT_LOG.md
  TEST_PATTERN_SPEC.md
  BENCH_SETUP.md
  MEASUREMENT_SCHEMA.md
  GATE_REPORT_TEMPLATE.md
  measurements/
    README.md
  reports/
    README.md
tools/pr3_optical/
  analyze_measurements.py
  validate_schema.py
  generate_test_patterns.py
tests/pr3_optical/
  test_analyze_measurements.py
  test_validate_schema.py
```

## Review Focus

- A vendor advertises projector brightness but not eye-level brightness through the selected waveguide: never substitute engine output for the v0.1 brightness gate.
- A measured eyebox is non-contiguous: report the largest contiguous usable region, not the total area of disconnected visible spots.
- One very bright 9-point sample masks dark corners: use `Lmin / Lmax`; do not report only average luminance.
- Thermal test shows brightness collapse before 30 minutes: fail the brightness/thermal gate even if minute-0 brightness passed.
- Bench controller is off-head: exclude it only from worn-mass/power figures where the spec explicitly allows, and record the actual configuration.

---

### Task 1: Vendor shortlist and RFQ evidence package

**Files:**
- Create: `docs/pr3/alpha-v0.1/VENDOR_SHORTLIST.md`
- Create: `docs/pr3/alpha-v0.1/RFQ_TEMPLATE.md`
- Create: `docs/pr3/alpha-v0.1/PROCUREMENT_LOG.md`

**Interfaces:**
- Produces: comparable vendor records for projector, matched waveguide, driver/eval electronics, published geometry, quoted availability, quote status, and evidence links.
- Consumes: approved spec values only.

- [ ] **Step 1: Build the shortlist schema**

Each candidate row must record: vendor/product, display type, full-color support, native resolution, stated FOV, projector dimensions/volume/mass, engine power, matched-waveguide availability, nominal eye relief, nominal eyebox, eye-level brightness claim if any, sample/dev-kit status, lead time, quote status, and source date.

- [ ] **Step 2: Prepare RFQ questions**

Ask specifically for matched monocular optical stack availability, interface/driver requirements, eye-relief/eyebox definition, measured eye-level luminance, waveguide efficiency, color/uniformity limits, engineering-sample pricing, lead time, NDA restrictions, thermal/power figures and whether evaluation optics can be photographed/measured.

- [ ] **Step 3: Record sourcing results**

Do not convert planning envelopes into vendor quotes. Mark unknowns as unknown.

- [ ] **Step 4: Review against v0.1 minimum gate**

Reject any candidate that cannot plausibly support the measurement campaign because a matched waveguide or evaluation interface is unavailable.

- [ ] **Step 5: Commit**

```bash
git add docs/pr3/alpha-v0.1/VENDOR_SHORTLIST.md docs/pr3/alpha-v0.1/RFQ_TEMPLATE.md docs/pr3/alpha-v0.1/PROCUREMENT_LOG.md
git commit -m "docs: define PR3 optical alpha procurement"
```

### Task 2: Deterministic test patterns and measurement schema

**Files:**
- Create: `docs/pr3/alpha-v0.1/TEST_PATTERN_SPEC.md`
- Create: `docs/pr3/alpha-v0.1/MEASUREMENT_SCHEMA.md`
- Create: `tools/pr3_optical/generate_test_patterns.py`
- Create: `tools/pr3_optical/validate_schema.py`
- Test: `tests/pr3_optical/test_validate_schema.py`

**Interfaces:**
- Produces: fixed pattern IDs and a machine-readable measurement-row contract.
- Consumes: target/minimum values from the approved spec.

- [ ] **Step 1: Define fixed pattern IDs**

Required patterns: `white_full`, `white_card_20pct`, `white_text`, `rgb_primaries`, `grayscale_steps`, `alignment_grid`, `korean_latin_readability`, `ghost_check`.

- [ ] **Step 2: Write failing schema-validation tests**

Require every measurement row to contain at minimum: `run_id`, `timestamp`, `hardware_id`, `pattern_id`, `eye_relief_mm`, `x_mm`, `y_mm`, `luminance_nit` when applicable, `visible_fraction`, `readable`, `color_breakup`, `dropout`, `ambient_condition`, `power_w`, `temperature_c`, and notes/source fields.

- [ ] **Step 3: Implement schema validator and pattern generator**

Generate test PNGs/SVGs deterministically from source, including resolution metadata.

- [ ] **Step 4: Run tests**

Run: `python -m pytest tests/pr3_optical/test_validate_schema.py -q`

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add docs/pr3/alpha-v0.1/TEST_PATTERN_SPEC.md docs/pr3/alpha-v0.1/MEASUREMENT_SCHEMA.md tools/pr3_optical tests/pr3_optical
git commit -m "test: define PR3 optical measurement contract"
```

### Task 3: Bench geometry and repeatable pupil-position procedure

**Files:**
- Create: `docs/pr3/alpha-v0.1/BENCH_SETUP.md`

**Interfaces:**
- Produces: repeatable eye-relief plane, X/Y origin, fixture photographs/diagrams, alignment procedure and run metadata requirements.
- Consumes: measurement schema from Task 2.

- [ ] **Step 1: Define nominal coordinate system**

Set nominal pupil position to `(0,0)` at the measured eye-relief plane. Define positive X/Y and the reference optical axis.

- [ ] **Step 2: Define translation sweep**

At minimum sample enough positions to prove or disprove a contiguous 9 x 8 mm gate region; include center and boundaries at +/-4.5 mm X and +/-4.0 mm Y. Use finer increments near clipping/dropout transitions.

- [ ] **Step 3: Define camera/pupil proxy controls**

Lock focal settings, exposure method, reference eye-relief distance, and alignment-grid procedure. Record any auto-exposure state when photographs are used as evidence.

- [ ] **Step 4: Define run metadata**

Every run records hardware serial/sample ID, waveguide orientation, firmware/driver version, brightness setting, ambient condition and fixture revision.

- [ ] **Step 5: Commit**

```bash
git add docs/pr3/alpha-v0.1/BENCH_SETUP.md
git commit -m "docs: define PR3 optical bench procedure"
```

### Task 4: Gate 1 visibility/readability and Gate 2 eyebox analysis

**Files:**
- Create: `tools/pr3_optical/analyze_measurements.py`
- Test: `tests/pr3_optical/test_analyze_measurements.py`
- Create: `docs/pr3/alpha-v0.1/measurements/README.md`

**Interfaces:**
- Produces: `visibility_pass`, largest contiguous usable eyebox width/height, eye-relief check and evidence summary.
- Consumes: validated measurement CSV/JSON rows.

- [ ] **Step 1: Write failing synthetic-data tests**

Create fixtures where the visible points form: a passing 9 x 8 mm contiguous region; disconnected islands with equivalent total area but failing contiguous dimensions; insufficient eye relief; unreadable center text.

- [ ] **Step 2: Implement contiguous-region analysis**

Do not infer pass from vendor eyebox claims. Pass only from measured grid samples satisfying the approved visibility definition.

- [ ] **Step 3: Run tests**

Run: `python -m pytest tests/pr3_optical/test_analyze_measurements.py -q`

Expected: PASS.

- [ ] **Step 4: Record real run when hardware exists**

Until hardware exists, keep result status `not_measured`; do not synthesize a pass.

- [ ] **Step 5: Commit**

```bash
git add tools/pr3_optical/analyze_measurements.py tests/pr3_optical/test_analyze_measurements.py docs/pr3/alpha-v0.1/measurements/README.md
git commit -m "feat: analyze PR3 visibility and eyebox gates"
```

### Task 5: Gate 3 movement persistence and Gate 4 brightness/uniformity

**Files:**
- Modify: `tools/pr3_optical/analyze_measurements.py`
- Modify: `tests/pr3_optical/test_analyze_measurements.py`

**Interfaces:**
- Produces: movement/dropout map, center brightness, 9-point `Lmin/Lmax`, ambient-condition readability status.
- Consumes: real or synthetic measurement rows.

- [ ] **Step 1: Add failing brightness/uniformity tests**

Pin minimum center brightness 1,500 nit, target 3,000 nit, minimum uniformity 0.55, target 0.70. Include a case where average is high but one corner drives `Lmin/Lmax` below 0.55 and must fail.

- [ ] **Step 2: Add failing movement-persistence tests**

A sample inside the minimum eyebox with complete dropout must fail the movement gate; occasional edge clipping outside the gate region is recorded but does not invalidate the minimum gate.

- [ ] **Step 3: Implement calculations**

Keep gate, target and raw measurements separate in output.

- [ ] **Step 4: Run tests**

Run: `python -m pytest tests/pr3_optical/test_analyze_measurements.py -q`

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add tools/pr3_optical/analyze_measurements.py tests/pr3_optical/test_analyze_measurements.py
git commit -m "feat: analyze PR3 movement and brightness gates"
```

### Task 6: Gate 5 wearability and Gate 6 power/thermal

**Files:**
- Modify: `docs/pr3/alpha-v0.1/MEASUREMENT_SCHEMA.md`
- Modify: `tools/pr3_optical/analyze_measurements.py`
- Modify: `tests/pr3_optical/test_analyze_measurements.py`

**Interfaces:**
- Produces: worn-mass, temple-thickness, optical-volume, average/peak-power and 30-minute thermal gate results.
- Consumes: mechanical measurements and time-series power/temperature records.

- [ ] **Step 1: Add failing mechanical gate tests**

Pin pass values: worn mass <=85 g; display-side temple <=12 mm; optical module <=30 x 20 x 10 mm and <=6 cm^3. Keep targets separately reported.

- [ ] **Step 2: Add failing thermal/power tests**

Pin minimum gate: display stack average <=1.0 W; whole worn electronics average <=1.5 W; peak <=2.0 W; no skin-contact hotspot >=45°C after 30 minutes; brightness must not collapse below Gate 4 during thermal run.

- [ ] **Step 3: Implement calculations**

If any required sensor sample is missing, result is `insufficient_evidence`, not pass.

- [ ] **Step 4: Run tests**

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add docs/pr3/alpha-v0.1/MEASUREMENT_SCHEMA.md tools/pr3_optical/analyze_measurements.py tests/pr3_optical/test_analyze_measurements.py
git commit -m "feat: analyze PR3 wearability and thermal gates"
```

### Task 7: Gate report and v0.2 decision record

**Files:**
- Create: `docs/pr3/alpha-v0.1/GATE_REPORT_TEMPLATE.md`
- Create: `docs/pr3/alpha-v0.1/reports/README.md`

**Interfaces:**
- Produces: one evidence-bound report with six gate states and explicit next action.
- Consumes: analysis outputs and raw measurement references.

- [ ] **Step 1: Define gate state vocabulary**

Use only: `pass`, `fail`, `not_measured`, `insufficient_evidence`.

- [ ] **Step 2: Build report template**

For each of six gates include: hardware configuration, measurement date, raw-data path, method, measured value(s), minimum gate, target, result and known caveats.

- [ ] **Step 3: Define v0.2 decision rule**

Do not start LBS + HOE comparison because of curiosity alone. Open v0.2 only if v0.1 identifies a concrete limitation that LBS+HOE plausibly addresses, or if B passes basic feasibility and a form-factor/power comparison is now worth the added variables.

- [ ] **Step 4: Commit**

```bash
git add docs/pr3/alpha-v0.1/GATE_REPORT_TEMPLATE.md docs/pr3/alpha-v0.1/reports/README.md
git commit -m "docs: define PR3 optical alpha gate report"
```

## Plan Self-Review Result

- Spec coverage: monocular decision, FOV, eyebox, eye relief, resolution, brightness, uniformity, artifact evidence, module size, temple thickness, mass, power, thermal, BOM/procurement and all six optical gates are mapped.
- Scope: no CGH, retinal projection, dynamic holographic video or custom product enclosure enters v0.1.
- Evidence discipline: unknown vendor values remain unknown and unmeasured hardware remains `not_measured`.
- Testability: all quantitative gate calculations have synthetic-data unit tests before real measurements are interpreted.
- Dependency boundary: actual Gate 1-6 measurements are physically blocked until an evaluation optical stack and bench access exist; the procurement/schema/analysis work can proceed before hardware arrival.
