# DSP 0.4.0

## Three styles and Knee

The differential linear-phase 2x–64x engine remains. Multiband, Fold, Orbit and Delta are removed. Style is a three-choice parameter exposed as a discrete dial, not a continuous algorithm blend control. Automation transitions use a 10 ms internal mixture to prevent abrupt changes.

`Knee` maps 0…100% to `k=0…0.95`. At zero, the transfer is exactly `clamp(x,-T,T)`. Clean uses a C1 quadratic shoulder between `T*(1-k)` and `T*(1+k)`; below the lower boundary the input passes unchanged, above the upper boundary the output is ±T.

- **Clean:** this knee applied directly in the oversampled domain. The old spectral analysis no longer automatically widens the knee; its diagnostic onset output is retained only for tests.
- **Punchy:** formerly Cancel. An 8x probe measures removed signal, a symmetric 1025-tap 2 kHz FIR extracts its LF portion, and 80% is restored before main decimation, capped at ±0.35T. The probe uses the same user knee. This adapts the distortion-cancellation branch of [US4208548A](https://patents.google.com/patent/US4208548A/en), not the entire patent. Final native-sample clipping now prevents its restoration from escaping the output boundary, trading some of the previous cancellation benefit for a strict sample bound.
- **Analog:** at k>0, unity below `lo=T*(1-k)`, then `sign(x)*(lo+T*k*tanh((abs(x)-lo)/(T*k)))`. At k=0, a hard clip. It is a mathematical saturator, not a measured model of a particular device.

## Sample boundary

After oversampled reconstruction and Output Gain:

```
B = min(1, T * outputGain)
y = clamp(reconstructed * outputGain, -B, B)
```

During a transition back from bypass, the active result is bounded again after the dry/wet transition. Fully bypassed audio remains delayed raw input. All mode, quality, Knee and Low Protect combinations are subject to the same boundary; positive Output Gain never disables the 0 dBFS maximum. Parameter smoothing changes the lower derived bound smoothly, while the absolute unity bound always holds.

**This is native-sample clipping, not ISP/true-peak protection.** No future reconstructed-wave maxima are searched. Final clipping can add harmonics and aliasing; it is an explicit tradeoff required for strict source-rate sample peaks. The oversampling topology alone cannot promise an identical source-rate ceiling after its FIR filters. [StandardCLIP's manual](https://www.siraudiotools.com/manual.php?id=standardclip) also distinguishes its computed clip level from post-oversampling sample ceiling.

## Low Protect

Same hybrid protection as 0.3: nominal 100 Hz linear-phase FIR estimates bass; fourth-order Butterworth 100 Hz detector maintains trigger selectivity as FIR transitions broaden at high rates. Peak hold decay 120 ms; gain attack 0.3 ms, recovery 90 ms. Low-only gain target 0.72T, bass-dominant full-band target 0.95T. LF/full-band envelope ratio selects between protected low-band plus a clipped residual, and full-band smooth limiting during bass dominance. The knob blends the selected style with this protected result.

Protection engages only when the held full-band peak exceeds T and LF peak exceeds about 0.55T. Quiet bass remains unchanged at hard knee. Steady bass is also protected; onset diagnostics do not gate protection. This adapts the LF VCA/upper-band clipper concept of [US5168526A](https://patents.google.com/patent/US5168526A/en), not its full circuit or original 2.2 kHz split.

This cannot isolate a kick from a mixed recording. Coincident instruments can be attenuated, and cleaner overloaded bass necessarily trades some level/crest factor for fewer harmonics. FIR ringing and envelope modulation remain possible. At high host rates fixed sample look-ahead is shorter in milliseconds.

## Engine and meters

129-tap Kaiser halfband stages filter the nonlinear error, with a delayed dry branch, adapting [US6337999B1](https://patents.google.com/patent/US6337999B1/en). Main roundtrip =128 samples, aligned analysis delay=640: **768 samples** fixed. Double arithmetic, no allocation/locks in audio processing, finite-input sanitization. Quality switching warms/crossfades two aligned banks.

OUT meters final returned sample magnitude. GR shows a peak estimate of nonlinear reduction plus final-bound attenuation; it is not a separate compressor envelope or a LUFS difference. Meter ballistics do not affect audio. Held OUT and GR maxima reset independently by clicking their top values; resetting also clears queued processor meter maxima. Values received after reset can immediately establish a new maximum.

State v4 has nine active parameters: in, ceiling, output, bass, quality, renderHQ, bypass, mode, knee. State migration preserves common controls, maps old Cancel to Punchy, drops Delta, maps removed styles to Clean and inserts hard Knee when absent. Host index automation after removing Delta may need reassignment.
