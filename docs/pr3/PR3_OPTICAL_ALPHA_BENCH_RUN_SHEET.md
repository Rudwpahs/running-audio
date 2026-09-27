# PR3 Optical Alpha v0.1 — Bench Run Sheet

Use with `PR3_OPTICAL_ALPHA_V0_1_SPEC.md`.

This file is a blank execution record. Do not pre-fill vendor claims as measured PR3 results.

## Run identity

- Run ID:
- Date/time:
- Operator:
- Path: [ ] A LBS + HOE  [ ] B microdisplay + waveguide
- Light engine model / revision / sample ID:
- Collimator / relay / coupler revision:
- HOE / waveguide model / revision / sample ID:
- Driver / firmware / SDK version:
- Renderer resolution / refresh:
- Test-pattern version / commit:

## Ambient and pupil setup

- Ambient temperature, C:
- Ambient illuminance, lux:
- Nominal eye relief, mm:
- Pupil stop diameter, mm:
- Source drive / brightness setting:
- Camera model / lens:
- Camera exposure / ISO / aperture:
- Luminance instrument / calibration date:
- Power instrument / calibration date:

## Incoming inspection

- [ ] exact part revisions recorded
- [ ] mechanical drawing / datum available
- [ ] optical interface / pupil information recorded
- [ ] polarization requirement recorded
- [ ] wavelength / spectrum recorded
- [ ] source luminous-flux condition recorded if available
- [ ] electrical input and power condition recorded
- [ ] operating-temperature limits recorded
- [ ] supplier alignment/calibration notes recorded
- [ ] A path only: laser-safety documentation reviewed before human viewing

## Gate 1 — visible image

- Horizontal usable FOV, deg:
- Vertical usable FOV, deg:
- Diagonal usable FOV, deg:
- 10-minute static-run image loss count:
- Manual optical readjustments during run:
- Korean reference text readable: [ ] yes [ ] no
- Latin reference text readable: [ ] yes [ ] no
- Grid/corner markers recognizable: [ ] yes [ ] no

Result: [ ] PASS [ ] FAIL

Evidence files:

## Gate 2 — eyebox

- Eye relief, mm:
- X step, mm:
- Y step, mm:
- Measured contiguous eyebox width, mm:
- Measured contiguous eyebox height, mm:

Attach X/Y pass map. Mark each sample with at least visible / clipped / luminance / color-breakup status.

Result: [ ] PASS [ ] FAIL

Evidence files:

## Gate 3 — head/eye movement tolerance

- Horizontal traversals completed:
- Vertical traversals completed:
- Z test range, mm:
- Longest complete dropout, ms:
- Total traverse time inside accepted eyebox, s:
- Total complete-dropout time, s:
- Dropout fraction, %:
- Reference text readable while moving: [ ] yes [ ] no

Result: [ ] PASS [ ] FAIL

Evidence files:

## Gate 4 — brightness

### Controlled measurement

- Full-white center luminance, cd/m2:
- 20% white UI center luminance, cd/m2:
- White-text center luminance, cd/m2:

### Practical readability

- Indoor ~300-500 lux: [ ] readable [ ] not readable  actual lux:
- Bright/window ~2,000-5,000 lux: [ ] readable [ ] not readable  actual lux:
- Outdoor shade ~10,000-20,000 lux: [ ] readable [ ] not readable  actual lux:

### Luminance retention

- Stabilized early-run 20% UI luminance, cd/m2:
- 30-minute 20% UI luminance, cd/m2:
- Retention, %:

Result: [ ] PASS [ ] FAIL

Evidence files:

## Gate 5 — image uniformity

Record full-white luminance using the same drive and pupil position.

| FOV point | Luminance cd/m2 |
| --- | ---: |
| center | |
| top | |
| bottom | |
| left | |
| right | |
| top-left | |
| top-right | |
| bottom-left | |
| bottom-right | |

- Lmin, cd/m2:
- Lmax, cd/m2:
- Lmin/Lmax:

Result: [ ] PASS [ ] FAIL

Evidence files:

## Gate 6 — artifact / rainbow / ghost

| Pupil position | RGB registration RMS, px | strongest ghost/primary, % | second readable glyph? | rainbow/color breakup | LBS speckle note |
| --- | ---: | ---: | --- | --- | --- |
| center | | | | | |
| left edge | | | | | |
| right edge | | | | | |
| top edge | | | | | |
| bottom edge | | | | | |

Result: [ ] PASS [ ] FAIL

Evidence files:

## Alignment sensitivity / reseat characterization

- Delta x80 negative / positive, mm:
- Delta y80 negative / positive, mm:
- Delta z80 negative / positive, mm:
- pitch80 negative / positive, deg:
- yaw80 negative / positive, deg:
- roll80 negative / positive, deg if relevant:

| Reseat | center luminance vs baseline, % | usable FOV vs baseline, % | eyebox area vs baseline, % | manual optical retune needed? |
| --- | ---: | ---: | ---: | --- |
| 1 | | | | |
| 2 | | | | |
| 3 | | | | |

## Optical efficiency — only when calibrated input flux is available

- Input luminous flux at tested operating point, lm:
- Average in-eye luminance definition / sample set:
- Average in-eye luminance, cd/m2:
- Average luminous efficiency, nits/lm:
- Center luminance, cd/m2:
- Center-only efficiency, nits/lm:

If input luminous flux is not calibrated, record: `NOT MEASURED`.

## Gate 7 — optical module size / mass feasibility

### Engine-side optics

- Bounding box W x H x T, mm:
- Calculated/measured volume, cm3:
- Engine + mandatory collimation/coupling mass, g:

### Combiner

- Width, mm:
- Height, mm:
- Thickness, mm:
- Mass, g:

### Glasses-like surrogate

- Display-side temple local maximum thickness, mm:
- Whole worn Alpha mass, g:
- Notes on cable/controller excluded from worn mass:

Result: [ ] PASS [ ] FAIL

Evidence files:

## Gate 8 — power / thermal feasibility

### Power

- Display engine average, W:
- Driver / bridge average, W:
- Display stack average, W:
- Whole worn electronics average excluding phone, W:
- Short peak, W:
- Peak duration / workload:

### Thermal and luminance log

| Time min | engine C | driver/bridge C | display-side contact C | opposite-side reference C | 20% UI luminance cd/m2 | notes |
| ---: | ---: | ---: | ---: | ---: | ---: | --- |
| 0 | | | | | | |
| 5 | | | | | | |
| 10 | | | | | | |
| 20 | | | | | | |
| 30 | | | | | | |

Result: [ ] PASS [ ] FAIL

Evidence files:

## Final gate table

| Gate | Result | Evidence reference | Blocking issue / next correction |
| --- | --- | --- | --- |
| 1 Visible image | | | |
| 2 Eyebox | | | |
| 3 Movement tolerance | | | |
| 4 Brightness | | | |
| 5 Image uniformity | | | |
| 6 Artifact | | | |
| 7 Optical module size/mass | | | |
| 8 Power/thermal | | | |

Overall Alpha result: [ ] PASS ALL 8  [ ] FAIL / ITERATE

Next action:

Do not advance to CGH, retinal projection, dynamic holographic video, pupil steering, or binocular product integration unless all eight gates pass on the same coherent Alpha configuration.