# Validation 0.5.0

## DSP comparison

48 kHz, 16x, hard Knee, Punchy, Low Protect 0, two equal-amplitude (1.2 each) sinusoidal carriers. Analyse the final 1 s of 2 s. IM figures are amplitude of the specified difference product relative to the retained upper carrier; this removes a simple global-level advantage. Compare the same 0.5.0 topology with the internal IMD switch off/on.

| Carriers | Measured product | IMD Clean change |
|---|---|---|
| 11 + 21 kHz | 1 kHz | −4.29 dB |
| 7 + 13 kHz | 1 kHz | −13.93 dB |
| 5 + 9 kHz | 1 kHz | −6.86 dB |
| 60 Hz + 7 kHz | 6880 Hz | +0.022 dB |
| 55 Hz + 1 kHz | 890 Hz | +0.001 dB |

The initial positive correction worsened the high-carrier probes; it was rejected. The negative limited correction with bass-dominance suppression is retained. These selected-product measurements do not establish lower total distortion or universal perceptual superiority. Tests permit at most 0.1 dB regression for the two bass combinations. They do not prove zero regression on arbitrary audio.

On the user-provided 10.97 s DnB dry file, input +11.6 dB, same settings: both off/on hit exactly 1.0 sample peak. ffmpeg ebur128 rounds loudness to −3.6 / −3.5 LUFS; LRA 0.5 LU both. After global RMS matching, 20–100 Hz energy changes +0.00163 dB, 100–2000 Hz +0.00307 dB, 2–20 kHz −0.01398 dB. Matched difference RMS is −49.23 dB relative to the output. No listening test or superiority over StandardCLIP is claimed; the supplied track changes only subtly. User audio is not included in the repository.

## Low Shape Protect

Constant 2.5-amplitude sine, Clean, same sample rate/quality: THD of harmonics 2–10 relative to fundamental changes from approximately 28% to numerical-floor levels at 40/55/80/100 Hz with Low Protect 100. This is steady-state behaviour, not a loudness-matched improvement claim. A direct half-wave ratio test checks uniform scaling after the lookahead; kick onset 55 Hz is affected and isolated 220 Hz snare-body tone is unchanged. Completed half-waves are protected; long incomplete waves use fallback gain. Mixed music can have modulation, attenuation of coincident instruments and residual final clipping.

## Boundary and latency

Tests cover all three styles, quality/mode/Knee/Low Protect automation and positive Output Gain. Ceiling is applied before Output, so +1 dB Output on a saturated 0 dB Ceiling reaches exactly +1 dBFS. Ceiling −6 plus Output −3 gives −9 dBFS. No ISP bound is asserted. The prepared latency is `640+ceil(0.030*sampleRate)`, 2080 samples at 48 kHz, independent of controls. Quiet delayed null, bypass, mono/stereo, float/double, state migration and no audio-thread allocation are also checked.

Local build/CTest, ASan/UBSan and pluginval results are recorded in Docs/Checks. Windows/macOS VST3+AAX are checked by GitHub Actions; local Linux checks do not substitute for remote platform builds or Pro Tools/PACE distribution validation.
