# DSP 0.3.0

## Modes

All six modes share the differential, linear-phase 2x–64x oversampling engine and constant **768-sample** latency. Mode automation uses a 10 ms mixture of model outputs; quality changes retain the warm-up/crossfade from 0.2. The inactive bank is reset without allocation. No ISP guard, output limiter, presets, automatic make-up gain or Mix control.

- **Clean:** exactly the 0.2 Perceptual curve with Low Protect at zero. C1 quadratic knee, width 0.25–1.5% of threshold, selected by spectral tonality/onset analysis. Quiet samples pass unchanged. Independent channel clipping; the shared knee is not a stereo gain envelope.
- **Cancel:** clips, estimates the removed waveform through an 8x probe, extracts its low-frequency portion with a 1025-tap symmetric Kaiser FIR at 2 kHz, and adds 80% back in the oversampled domain. Reinjection is capped at ±0.35 × Ceiling to avoid unlimited bass restoration. This adapts the distortion-cancellation branch of [US4208548A](https://patents.google.com/patent/US4208548A/en), not its entire masking/feedback controller. It reduces low difference-frequency products from high-frequency clipping, but allows peaks above Ceiling and is not a strict limiter. This is a different topology from simply increasing the soft knee.
- **Analog:** unity slope below 0.5 × Ceiling, then `sign(x) * T * (0.5 + 0.5*tanh(2*(abs(x)/T-0.5)))`. Symmetric smooth saturation; no claimed model of a particular circuit, noise or DC bias.
- **Multiband:** complementary linear-phase FIR bands at nominal 100 Hz and 2.5 kHz. Low, mid and high signals sum to the delayed input before nonlinearity. They have clipping budgets 0.5T, 0.65T and 0.35T; the low band first receives smooth envelope limiting. Their sum is projected through the narrow Clean knee before decimation. Adapted from the concept of embedded band clippers in [US4412100A](https://patents.google.com/patent/US4412100A/en); this is a three-band digital design, not the patent's six-band distributed analog crossover. The final projection can still create intermodulation; this does not promise isolation of every band.
- **Fold:** odd-symmetric triangular wavefolding, period 4T, unity below T. Deliberate harmonic colour, not a clean mastering mode.
- **Orbit:** radial stereo clipping: vectors longer than T are scaled to length T. It preserves stereo-vector direction while coupling channel gain. In mono the two equal channels still share a radius, so its effective onset is T/√2; this is deliberate and differs from Clean. Experimental, not a patent reconstruction.

The exact topology of [StandardCLIP](https://www.siraudiotools.com/manual.php?id=standardclip) is not public. We use its documentation as a reference for oversampling/reconstruction behaviour, not as source code.

## Low Protect

The 0.2 onset-gated clipped-error reinjection is removed. Its detector remains only as an internal diagnostic; it no longer gates the protection. Steady sub-bass also deserves protection.

A 1025-tap symmetric Kaiser FIR estimates the band at nominal **100 Hz**. The finite FIR has a transition region, not a brick-wall cutoff; separation gets less precise at high host sample rates. The low band is delayed by 128 samples after its FIR centre, matching the 640-sample analysis delay of the main input. The FIR output leads aligned audio by 128 samples. Gain detection uses an additional fourth-order Butterworth 100 Hz low-pass on the undelayed input to avoid broad FIR transitions falsely triggering protection on snare at high rates. The detector has frequency-dependent delay, reducing effective look-ahead at high host rates. The oversampling stage adds another 128 samples.

A peak envelope spans LF cycles with a 120 ms decay; gain reduction uses 0.3 ms attack / 90 ms recovery and targets of 0.72T for the low-only branch / 0.95T for bass-dominant full-band limiting. A slow LF/full-band peak ratio selects between:

1. a protected low-band signal plus a clipped residual with reserved headroom;
2. smoothly limited full-band audio when LF dominates, as on a kick/sub-only passage.

Protection engages only when the held full-band peak exceeds T and the low-band peak exceeds about 0.55T. The knob blends the selected model with this protected result. At zero, Clean is unchanged; quiet bass is untouched. At 100%, overloaded low-dominant audio is primarily envelope-limited rather than flat-topped.

This adapts the **low-frequency VCA + upper-band clipper** concept of [US5168526A](https://patents.google.com/patent/US5168526A/en), replacing its original 2.2 kHz filters and control network. It is not exact source separation: a simultaneous snare or hat can be attenuated during a bass-dominant event. Cleaner bass at a fixed threshold necessarily trades some level/crest factor for fewer harmonics. FIR pre-ringing and envelope modulation also remain possible.

## Shared engine

129-tap sparse Kaiser halfband stages filter only the nonlinear error, based on the differential topology of [US6337999B1](https://patents.google.com/patent/US6337999B1/en). The dry branch receives matching delay. No allocations/locks in process; double internal arithmetic. Float/double and mono/stereo hosts supported.

**Ceiling is a nonlinear threshold, not a certified sample/true-peak ceiling.** Reconstruction can overshoot in every mode; Cancel explicitly restores part of the removed signal. Output trim is applied last. Delta compares the delayed driven original with the complete processed result, including Low Protect. Bypass is delayed raw audio.

Existing eight parameter indices/IDs remain in place; `mode` is appended. States before v3 select Clean explicitly. The old `bass` ID is retained with the new name/behaviour. Solid White preferences fall back to Solid Dark.
