# Validation 0.4.0

Current source validation uses GCC 13/Linux x64, JUCE 8.0.4, Release VST3. Native Windows/macOS VST3+AAX builds are performed separately in GitHub Actions. Linux checks do not establish PACE/Pro Tools release compatibility.

## Checks — local PASS

CMake build, CTest 2/2 (13.97 s), ASan/UBSan and native pluginval strictness 5 passed. UI captures were visually inspected after the new layout build.

- DSP: quiet hard-knee null at six sample rates and six qualities; 768-sample latency; stereo anti-phase behaviour; finite inputs; delayed bypass; no audio-thread allocations; mode/quality/Low Protect automation.
- **Native-sample bound:** all three styles at 44.1/48/192 kHz, changing all six qualities, Input +36 dB, Output +12 dB, Low Protect 100%, Knee changing 0/100%. Every finite returned active sample stays at or below absolute unity (0 dBFS).
- Negative bound: Ceiling −6 dB + Output −3 dB keeps returned samples at or below −9 dBFS.
- Knee: zero is exact hard clipping; soft knee changes the shoulder below threshold.
- Processor/editor: nine parameters; removed Delta absent; state roundtrip and old-state migration; float/double, mono/stereo, offline 64x equivalence.
- UI: six dials and two vertical meters, no overlap at widths 600/800/1200 in three themes; Style has three discrete stops and displays Clean/Punchy/Analog; independent held peak reset; bypass blur; 20 editor creation/destruction cycles.
- Native VST3 pluginval 1.0.4, strictness 5.
- ASan/UBSan DSP runner. Local LeakSanitizer disabled because sandbox `/proc` access prevents inspecting threads; no leak certification claimed.

The supplied DnB dry recording was also rendered at +11.6 dB Input Gain, 16x, hard Knee, all three styles and Low Protect 0/100%. All six renders have a maximum returned sample magnitude of exactly 1 (0 dBFS). This is a peak-bound check, not a new listening verdict.

## Controlled DSP measurements

48 kHz, 16x, hard knee, sine input amplitude 2.5 (~+7.96 dBFS). Harmonics 2–10 measured over the stationary final second:

| Frequency | Clean THD | Low Protect 100% THD |
|---|---:|---:|
| 40 Hz | 27.9822% | 0.159754% |
| 55 Hz | 27.9821% | 0.0522429% |
| 80 Hz | 27.9810% | 0.0257942% |
| 100 Hz | 27.9799% | 0.0313600% |

These are not loudness-matched comparisons: smooth protection reduces gain rather than flat-topping bass. Controlled 55 Hz decaying kick changes; controlled 220 Hz decaying snare-body tone remains unchanged. Real snares can contain LF energy, and simultaneous instruments can be affected.

Two tones at 11 and 21 kHz, amplitude 1.2 each: 1 kHz third-order difference-product projection is 4248.12 in Clean vs 2112.54 in Punchy, approximately 6 dB lower. Units are arbitrary and common to both. The new final sample boundary intentionally reduces some of the prior Cancel's cancellation benefit. This does not prove universally better sound or superiority over StandardCLIP.

## Limits

Bypass passes delayed original audio and can exceed unity if the original does. No true-peak/ISP bound. The final native-sample clip can produce additional harmonics and aliasing when it catches reconstruction overshoot or positive Output Gain. Soft Knee changes dynamics and tonal balance. Clean and Analog coincide at exact hard Knee; their soft transfer shapes differ. Fixed-size LF filters/look-ahead are less precise in time/frequency at high sample rates. 64x can be CPU intensive.

Current check logs are under `Docs/Checks/*v0.4*`. Older binaries, captures and v0.3/v0.2/v0.1 logs are historical; use the latest GitHub Actions artifacts.
