# DSP 0.6.0

The host plugin always selects Clean, hard Knee. Style/Knee are removed from the parameter layout; old state children are pruned. Internal legacy DSP branches remain private and are not exposed by the plugin. The eight parameters are in, ceiling, output, bass, quality, renderHQ, bypass, link. State version 6 inserts Link off for older sessions.

## Signal and sample boundary

129-tap Kaiser halfband stages process nonlinear error with a delayed dry branch, inspired by US6337999B1. Quality is 2x–64x, with aligned bank transitions. Prepared latency is `640+ceil(0.030*sampleRate)`, 2080 samples at 48 kHz. All controls use the same latency.

The oversampled Clean curve is `clamp(x,-T,T)`. After reconstruction, samples are clipped to ±T before final Output Gain. Output +1 dB therefore permits +1 dBFS at Ceiling 0. Active bypass transitions are bounded to `T*outputGain`; fully bypassed audio remains delayed raw input. There is no ISP/true-peak guard. Native final clipping can add in-band distortion and aliasing.

## Low Protect

A nominal 100 Hz linear-phase split feeds a retrospective half-wave scaler. Each completed sign interval receives constant gain `min(1,0.90*T/lowPeak,0.95*T/fullPeak)`, preserving its internal sample ratios. Corresponding full-band samples are aligned with the FIR's 512-sample centre. A 30 ms buffer waits for half-wave completion; longer incomplete waves use the prior smooth-envelope fallback.

Protected LF plus a headroom-clipped residual blends toward a full-band half-wave-scaled result during LF dominance. A fourth-order 100 Hz detector and held peak envelopes govern engagement. Knob 0 bypasses protection; quiet signals remain unchanged. Adjacent half-waves can have different gains; waveform slope changes/modulation remain possible. This cannot isolate kick from an arbitrary mix and may attenuate coincident sounds.

This adapts predictive zero-crossing level control from US20040002313A1, not its complete radio system.

## Gain reduction

The old instantaneous `abs(input)/abs(shaped)` division could diverge around output zeros even when useful peak attenuation was modest. Each oversampled phase now contributes only its excess over Ceiling, `max(1,abs(x)/T)`. Low Protect also contributes the attenuation of its actual blended half-wave gain, `1-protect*engaged*(1-gain)`, while input is active; silent filter startup is excluded. Final native-bound attenuation is measured separately. The maximum of these estimates is reported as GR. Output Gain does not affect GR.

This is a peak attenuation estimate, not the exact ratio of mixed-band waveform samples or an RMS/LUFS measurement. Split-band reduction is conservatively represented by its low-protection control; it does not quantify an individual source's attenuation.

## Link and UI

Link is an APVTS boolean stored per instance, off by default. Manual gain changes in the editor notify the host of both parameters, setting the counterpart to the negative value. A recursion guard prevents attachment feedback. Input spans −24…+36 dB, Output is expanded to −36…+24 dB to provide its exact inverse across the full range. Enabling Link aligns Output with Input. Standalone host automation is not rewritten from the audio callback; record both gains for inverse automation.

OUT/GR segmented meters use left-to-right fill. OUT range −30…+6 dBFS, GR 0…36 dB; peak numeric holds remain unrestricted and reset independently by clicking the right-hand label/value. Ballistics do not affect audio. Layout is checked at three sizes/themes.
