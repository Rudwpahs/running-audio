# PR1 Music-Oriented Packet Loss Concealment Research Ideas

> Status: **Research hypothesis / design notes only — not implemented**
>
> Date: 2026-09-13
>
> Scope: PR1 receiver-side audio concealment for short packet losses, especially music playback, plus interaction with PR1 link-layer recovery (FEC/ARQ/jitter/deadline information).

---

## 1. Why this document exists

PR1 originally treated packet loss mainly as a communications problem: avoid the loss, recover the exact packet if possible, and only then conceal the remaining audible gap.

During the current research discussion, a second direction emerged:

1. keep the existing exact-recovery path (original packet, FEC, deadline-aware ARQ, codec redundancy),
2. treat unrecoverable audio gaps as an **audio prediction / inpainting problem**,
3. exploit the fact that most intended PR1 listening is expected to be **music**, not only speech,
4. use the strong time-frequency structure and repetition of music,
5. make the concealment algorithm aware of network/link timing so that it spends computation only where exact recovery is no longer possible.

The purpose of this file is to preserve the ideas before implementation and to clearly separate:

- established prior art,
- current PR1 implementation,
- proposed PR1 research hypotheses,
- possible novelty areas that still require a patent/prior-art search.

---

## 2. Current PR1 recovery philosophy

The intended recovery order should remain approximately:

```text
Original packet
    -> exact packet available

XOR / packet-level FEC
    -> exact reconstruction if possible

Deadline-aware ARQ
    -> retransmit only if it can arrive before playout deadline

Codec redundancy / in-band FEC
    -> partial or codec-assisted exact/near-exact recovery

PLC / audio inpainting
    -> synthesize a perceptually plausible substitute
```

Important distinction:

- **Jitter buffer** hides timing variation and gives recovery mechanisms time.
- **FEC / ARQ** try to recover the actual transmitted information.
- **PLC** does not recover the original packet bits. It generates replacement audio that should be difficult to notice.

The final product objective is therefore not necessarily `raw PER = 0`, but closer to:

```text
raw packet loss
    -> residual loss after FEC
    -> residual loss after ARQ
    -> PLC-required events
    -> audible dropout rate
```

---

## 3. Why music should be treated differently from generic speech PLC

Music has useful priors that ordinary packet-level logic ignores:

- short-term waveform periodicity,
- stable pitch and harmonic series,
- correlated amplitude envelopes across harmonics,
- beat / rhythmic repetition,
- repeated motifs, accompaniment patterns, chorus/verse material,
- sparse and structured time-frequency representations,
- non-local self-similarity over hundreds of milliseconds to many seconds.

This suggests that a music-oriented PLC does not need to predict every time-frequency bin independently.

A more useful representation may be:

```text
received audio
      -> STFT / alternative TF transform
      -> time-frequency coefficients
      -> discover groups of bins that move together
      -> predict group evolution
      -> reconstruct missing TF region
      -> enforce phase / boundary consistency
      -> ISTFT
```

---

## 4. Core hypothesis A — dynamic frequency-group prediction

### 4.1 Do not use fixed frequency bands as the final representation

A simple system could split audio into fixed subbands and predict each independently. This already has strong prior art.

However, music contains relationships that do not align neatly with fixed adjacent bands.

Example harmonic family:

```text
f0 = 220 Hz
2f0 = 440 Hz
3f0 = 660 Hz
4f0 = 880 Hz
...
```

These frequencies may belong to the same musical source and have correlated temporal envelopes even though they are far apart in frequency.

### 4.2 Proposed dynamic grouping

Let the STFT be

```math
X(t,f)=\operatorname{STFT}(x(t)).
```

For frequency bins or compact frequency regions, estimate relationships using features such as:

- magnitude-envelope correlation,
- phase-velocity / instantaneous-frequency similarity,
- harmonic-ratio relationships,
- repeated co-activation,
- spectral cosine similarity,
- common modulation,
- source-like temporal persistence.

Build a graph:

```math
G=(V,E),
```

where a node is a frequency bin / compact band and

```math
w_{ij}
```

represents how strongly components `i` and `j` tend to evolve together.

Potential result:

```text
Group A -> bass fundamental + harmonics
Group B -> vocal harmonic family
Group C -> sustained pad / chord components
Group D -> high-frequency texture
Group E -> transient/noise-like content
```

This is a key candidate for separating a PR1-specific approach from older fixed-subband PLC.

---

## 5. Core hypothesis B — different predictors for different spectral groups

Not all music components should use the same predictor.

Possible routing:

```text
stable tonal / harmonic group
    -> phase + instantaneous-frequency extrapolation

smooth sustained group
    -> low-order AR / Burg predictor

noise-like group
    -> shaped-noise / spectral-envelope synthesis

transient group
    -> conservative interpolation / nearest context / neural residual

strong repeated section found elsewhere
    -> self-similarity / exemplar prior
```

This follows an important lesson from prior work: hybrid PLC often beats a single universal reconstruction rule.

---

## 6. Core hypothesis C — cross-frequency coupled optimization

### 6.1 Independent optimization is probably too weak

If every frequency group `i` is reconstructed independently:

```math
X_i^{(k+1)}=X_i^{(k)}-\eta_i\nabla_{X_i}L_i,
```

then a strong correction in a fundamental frequency cannot influence its harmonics.

That is inconsistent with real music structure.

### 6.2 Couple the groups through the loss

A safer first formulation is to include inter-group consistency directly in the objective:

```math
L = \sum_i L_i
+ \lambda \sum_{i,j} w_{ij}L_{ij}.
```

Possible terms include:

```math
L_{ij}^{amp}
=\left|\Delta A_i-c_{ij}\Delta A_j\right|^2,
```

and a phase/evolution relation such as:

```math
L_{ij}^{phase}
=\left|\Delta\phi_i-r_{ij}\Delta\phi_j\right|^2.
```

This allows related bins to influence one another through the gradient itself.

---

## 7. Core hypothesis D — learned cross-frequency update matrix

A more general version of the previous idea is to replace a scalar learning rate with an interaction / preconditioning matrix.

Let

```math
g=\nabla_X L.
```

Instead of

```math
X_{k+1}=X_k-\eta g,
```

use

```math
X_{k+1}=X_k-Mg.
```

`M` can encode cross-frequency relationships.

Example interpretation:

- a gradient at 220 Hz may also update 440/660/880 Hz,
- strongly correlated harmonic groups obtain coordinated corrections,
- unrelated spectral regions remain weakly coupled.

This is a more general mathematical form of the idea that "the learning rate of one frequency should be influenced by other frequencies."

### 7.1 Adaptive / learned `M`

A research extension is to adapt `M`:

```math
M_{k+1}=M_k-\mu\nabla_M L.
```

This turns the system into a type of learned optimizer / meta-gradient system.

**Important:** this is a research hypothesis, not a claim of novelty.

---

## 8. Core hypothesis E — meta-learning the update rule / learning rate

A simpler meta-gradient version is to treat each group learning rate as a variable:

```math
X_i^{(k+1)}=X_i^{(k)}-\eta_i\nabla_{X_i}L,
```

then update

```math
\eta_i^{(k+1)}
=\eta_i^{(k)}-\mu\frac{\partial L}{\partial \eta_i}.
```

Possible confidence-controlled rate:

```math
\eta_i=\eta_0 C_i,
```

where `C_i` may depend on:

- periodicity strength,
- harmonic correlation,
- spectral stability,
- phase stability,
- agreement with neighboring groups,
- similarity to prior musical material,
- loss duration,
- availability of a future frame.

This should not automatically be assumed superior to Adam/SGD/proximal methods. It must be experimentally justified.

---

## 9. Core hypothesis F — use the music's own repetition

Music often contains non-local repetition that short-context speech PLC does not exploit.

Example:

```text
0:32 -> A B C D
0:33 -> E F G H

1:04 -> A B C ?
```

If the system identifies that the material at 1:04 resembles 0:32, then the earlier section can become a strong prior for the missing spectrum.

Potential hierarchy:

```text
local predictor
    -> 10–50 ms temporal evolution

harmonic predictor
    -> correlated frequency-family evolution

non-local self-similarity
    -> repeated musical patterns / motifs / accompaniment
```

The system should never blindly copy a previous section. The retrieved segment should be used as a prior or initialization and then reconciled with the immediate boundaries.

---

## 10. Phase must be treated as a first-class problem

Magnitude-only reconstruction is insufficient for high-quality real-time audio.

Missing audio reconstruction should consider:

- complex STFT coefficients,
- instantaneous frequency,
- phase progression,
- boundary continuity,
- overlap-add consistency.

For stable tonal components, phase may evolve approximately predictably over a short gap.

A candidate short-loss procedure:

```text
past TF frames
    -> estimate magnitude trend
    -> estimate instantaneous frequency / phase velocity
    -> initialize missing complex spectrum
    -> cross-group correction
    -> enforce overlap/boundary consistency
    -> ISTFT
```

---

## 11. PR1 should not run a heavy optimizer after every loss

This is a critical hardware constraint.

Previous PR1 RF stress experiments suggested that receiver processing / re-arm timing can become a bottleneck before RF signal strength itself becomes the limiting factor.

Therefore a PLC that performs dozens or hundreds of STFT-gradient iterations on every missing 10 ms packet would be dangerous.

Preferred directions:

### Option A — learn/adapt while packets are healthy

```text
normal received music
    -> continuously estimate spectral groups
    -> update correlations / predictor state / optional small optimizer state

packet loss occurs
    -> use already-learned state
    -> 1–3 correction steps maximum
```

### Option B — offline pretrained model + lightweight inference

```text
PC training
    -> masked packet loss
    -> waveform + STFT/phase/perceptual loss
    -> train compact predictor

PR1 receiver
    -> inference only on actual loss events
```

### Option C — hybrid

- pretrained predictor,
- cheap online adaptation of only a small state / subset of parameters,
- no full model backpropagation on the receiver.

---

## 12. Cross-layer PR1 policy layer

The potentially more distinctive PR1 direction is not PLC alone, but coordination between exact recovery and audio reconstruction.

A future policy layer may observe:

```text
sequence number
loss / CRC state
RSSI / link quality
channel / AFH state
estimated burst length
FEC availability
ARQ retransmission ETA
playout deadline
jitter-buffer depth / slack
receiver processing-limited state
audio signal class / prediction confidence
future-frame availability
```

and choose among:

```text
1. Wait for original packet
2. Recover using packet FEC
3. Request/use deadline-safe ARQ
4. Use codec/in-band redundancy
5. Use cheap AR / phase predictor
6. Use music-specific spectral-group reconstruction
7. Use neural residual correction
8. Use conservative fallback (repeat/fade/noise/mute)
```

Key proposed decision principle:

```math
\text{choose action } a
=\arg\min_a
\Big(
D_{perceptual}(a)
+\lambda_E E_{RX}(a)
+\lambda_L L_{added}(a)
+\lambda_C C_{compute}(a)
\Big)
```

subject to the hard playout deadline.

This is deliberately different from a PLC that always runs one reconstruction model whenever a packet is missing.

---

## 13. Interaction with PR1 `ProcessingLimited`

PR1 already has the important concept that a processing-limited receiver must not be given more optional work.

The music PLC design should preserve that rule.

Example:

```text
if ProcessingLimited:
    disable expensive spectral optimizer
    disable optional online adaptation
    use cheap exact recovery if deadline-safe
    otherwise use low-cost fallback PLC
```

A reconstruction algorithm that improves audio quality but causes the next RF packet to be missed is a net failure.

---

## 14. Closest prior art found so far

### Fixed/subband online prediction

Cocchi & Uncini, **Subband neural networks prediction for on-line audio signal recovery**, IEEE Transactions on Neural Networks, 2002.

- multirate subband architecture,
- small neural predictor per narrow subband,
- online/continuous learning,
- music restoration,
- reported useful reconstruction beyond 100 ms.

DOI: https://doi.org/10.1109/TNN.2002.1021887

This is important prior art against any broad claim of "split audio into frequency bands and predict each band online."

### Music self-similarity / spectral graph

Perraudin et al., **Inpainting of Long Audio Segments With Similarity Graphs**.

- uses time-frequency structure,
- constructs similarity graph,
- exploits repeated musical content for long missing segments.

DOI: https://doi.org/10.1109/TASLP.2018.2809864

### Time-frequency dictionary learning

Tauböck et al., **Dictionary Learning for Sparse Audio Inpainting**, 2021.

- learns a TF dictionary from reliable audio around the gap,
- adapts representation to the signal itself,
- improves SDR/ODG over several baselines.

DOI: https://doi.org/10.1109/JSTSP.2020.3046422

### Music-specific DNN inpainting

Marafioti et al., **A Context Encoder For Audio Inpainting**.

- TF context around missing audio,
- tens-of-ms gaps,
- music and musical instruments,
- magnitude-domain neural reconstruction was competitive and particularly useful on complex music.

DOI: https://doi.org/10.1109/TASLP.2019.2947232

### Structured TF sparsity

Lieb & Stark, **Audio inpainting: Evaluation of time-frequency representations and structured sparsity approaches**, 2018.

- STFT/Gabor sparse structure,
- optimization-based reconstruction,
- notes value of more flexible TF representations such as wavelets/ERBlets.

DOI: https://doi.org/10.1016/J.SIGPRO.2018.07.012

### Weighted optimization

Mokrý & Rajmic, **Audio Inpainting: Revisited and Reweighted**, 2020.

- weighted `l1` optimization,
- addresses amplitude/energy loss inside reconstructed gaps.

DOI: https://doi.org/10.1109/TASLP.2020.3030486

### Phase-aware TF optimization

Balušík & Rajmic, **Audio Inpainting in Time-Frequency Domain with Phase-Aware Prior**, 2026 preprint.

- STFT-domain optimization,
- instantaneous-frequency/phase-aware prior,
- generalized Chambolle-Pock solver,
- high quality but current full method is far too heavy for PR1 10 ms embedded real-time use.

DOI/preprint: https://doi.org/10.48550/arXiv.2601.18535

### Gradient-optimized STFT parameters

Zhao, Subramani & Smaragdis, **Optimizing Short-Time Fourier Transform Parameters via Gradient Descent**.

- demonstrates differentiable optimization of STFT window/hop parameters,
- relevant evidence that even transform parameters themselves can be learned/adapted.

DOI: https://doi.org/10.1109/ICASSP39728.2021.9413704

### Hybrid AR + neural residual baseline

Mezza et al., **Hybrid Packet Loss Concealment for Real-Time Networked Music Applications (PARCnet)**.

- AR predictor + neural residual,
- multi-resolution STFT training loss,
- real-time CPU target,
- strong baseline for any PR1-specific music PLC.

Public implementation: https://github.com/polimi-ispl/PARCnet

The implementation uses an AR model based on autocorrelation / Levinson-Durbin and a neural generator trained with waveform MSE plus multi-resolution STFT spectral-convergence and log-magnitude losses.

### Low-latency AR implementation

Sacchetto et al., **Implementation and optimization of Burg’s method for real-time packet loss concealment in networked music performance applications**, 2024.

- 44.1 kHz music,
- 128-sample (~2.9 ms) packets,
- demonstrates that carefully optimized AR can meet strict real-time constraints on Raspberry Pi 4B.

DOI: https://doi.org/10.1007/s00779-024-01806-8

---

## 15. Cross-layer prior art that limits broad novelty claims

Broad ideas such as "use network state to choose retransmission/PLC" are not new.

Examples include:

- perceived-quality-driven retransmission for wireless VoIP,
- adaptive playout + PLC,
- cross-layer VoIP systems using runtime network conditions,
- EVS channel-aware FEC modes,
- channel-state-aware neural joint source-channel coding.

Representative references:

- Li et al., **Perceived speech quality driven retransmission mechanism for wireless VoIP**, DOI: https://doi.org/10.1049/CP:20030403
- Liang, Färber & Girod, **Adaptive playout scheduling and loss concealment for voice communication over IP networks**, DOI: https://doi.org/10.1109/TMM.2003.819095
- Wah & Sat, **The Design of VoIP Systems With High Perceptual Conversational Quality**, DOI: https://doi.org/10.4304/jmm.4.2.49-62
- Chung et al., **A Cross Layer Perceptual Speech Quality Based Wireless VoIP Service**, DOI: https://doi.org/10.1587/TRANSFUN.E93.A.2153
- Rämö et al., **EVS Channel Aware Mode Robustness to Frame Erasures**, DOI: https://doi.org/10.21437/Interspeech.2016-917

Therefore PR1 should not claim novelty merely for combining a jitter buffer, network state, FEC, ARQ, and PLC.

---

## 16. Current candidate research contribution

The strongest research candidate identified so far is the combination of:

1. **music-specific dynamic spectral grouping** rather than fixed bands,
2. **harmonic/correlation graph** learned continuously from the current song,
3. **group-specific predictors** (phase, AR, noise, neural residual, self-similarity),
4. **cross-frequency coupled correction** rather than independent bins,
5. optional **learned preconditioner / meta-gradient update state**,
6. non-local musical repetition as a prior,
7. strict 10 ms-class low-latency constraint,
8. event-driven execution only on unrecoverable losses,
9. PR1 link-layer inputs including FEC/ARQ ETA, playout slack, RF state and processing-limited state,
10. explicit choice between **exact recovery** and **perceptual reconstruction** under deadline/energy/compute constraints.

No single paper found in the targeted search contained this complete combination.

**This is NOT a novelty or patentability conclusion.** A dedicated patent search and broader prior-art analysis are still required.

---

## 17. Candidate algorithm sketch

```text
NORMAL AUDIO ARRIVES
        |
        +--> update jitter/link state
        +--> STFT
        +--> update spectral correlation graph
        +--> update harmonic groups
        +--> update AR / phase / repetition predictor state
        +--> optional low-rate adaptation of optimizer state

PACKET MISSING
        |
        +--> exact FEC available? -------- yes --> recover
        |
        +--> ARQ can meet deadline? ------ yes --> retransmit/recover
        |
        +--> codec redundancy available? - yes --> recover
        |
        v
  estimate:
    loss duration
    buffer slack
    RF/link state
    future-frame availability
    processing budget
    audio class
    predictor confidence
        |
        v
  choose PLC route
        |
        +--> tonal: phase/harmonic predictor
        +--> sustained: AR predictor
        +--> repeated: self-similarity prior
        +--> complex: neural residual
        +--> noise-like: shaped spectrum/noise
        |
        v
  initialize missing TF region
        |
        v
  cross-frequency correction
      X <- X - M grad(L)
  (target: very few iterations)
        |
        v
  phase/boundary consistency
        |
        v
      ISTFT
        |
        v
  output concealed audio
```

---

## 18. What must be tested before considering this useful

### Baselines

At minimum compare against:

- zero fill / silence,
- previous packet repeat,
- cross-fade / WSOLA-style simple concealment,
- Opus PLC,
- low-order AR / Burg,
- phase/STFT predictor,
- PARCnet-type AR + neural residual,
- PR1 proposed music-specific method.

### Loss patterns

Test separately:

- isolated 10 ms loss,
- 20 ms consecutive loss,
- 30–50 ms burst,
- random losses,
- bursty wireless-like losses,
- losses during tonal/sustained material,
- losses during transients/drums,
- losses at section boundaries.

### Audio classes

Use at least:

- vocal-heavy pop,
- acoustic instruments,
- piano,
- guitar,
- bass-heavy tracks,
- drums/percussion,
- electronic music,
- dense polyphonic music.

### Metrics

Do not rely on waveform MSE alone.

Candidate metrics:

- SI-SDR / SDR,
- log spectral distance,
- multi-resolution STFT error,
- PESQ where applicable,
- STOI for speech/vocal intelligibility,
- PLC-MOS / subjective AB listening,
- discontinuity/click score,
- compute time per lost frame,
- receiver CPU load,
- receiver energy,
- whether PLC computation causes the next RF packet to be missed.

---

## 19. Critical falsification tests

The proposed method should be rejected or simplified if any of the following occurs:

1. dynamic spectral grouping costs more than the quality gain,
2. cross-frequency coupling introduces musical hallucination or smearing,
3. phase optimization creates metallic artifacts,
4. non-local repetition chooses musically wrong material,
5. neural residual gives little improvement over optimized AR,
6. online adaptation destabilizes between tracks,
7. processing cost increases RF packet loss,
8. Opus/PARCnet already provides equal quality with far lower complexity.

The objective is not to force a novel algorithm into the product. The objective is to discover whether a PR1-specific method is measurably better.

---

## 20. Immediate next research steps

Before firmware implementation:

1. build a PC-side simulator that injects known 10/20/30 ms losses into clean music,
2. preserve ground truth for objective evaluation,
3. implement cheap baselines first,
4. implement STFT complex-spectrum analysis,
5. test fixed subband prediction,
6. test dynamic correlation/harmonic grouping,
7. compare independent vs cross-group prediction,
8. test one/few-step coupled correction,
9. test self-similarity retrieval as an initialization prior,
10. only then evaluate whether a meta-gradient / learned update matrix is justified,
11. after PC validation, profile a stripped-down version against ESP32-S3 timing constraints,
12. integrate with PR1 FEC/ARQ/jitter only after the audio-only simulator proves value.

---

## 21. Working names

Temporary research names only:

- **PR1 Music-PLC**
- **Spectral Group PLC**
- **Harmonic Graph PLC**
- **Cross-Frequency Predictive Inpainting**
- **PR1 Adaptive Music Concealment (AMC)**

No naming choice implies patent or publication novelty.

---

## 22. Research rule going forward

Every new idea should be tagged as one of:

- `KNOWN PRIOR ART`
- `PR1 EXISTING`
- `RESEARCH HYPOTHESIS`
- `SIMULATED`
- `HARDWARE VALIDATED`

Do not allow conceptual ideas to be mistaken for implemented or measured functionality.
