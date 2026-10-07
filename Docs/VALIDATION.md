# Validation 0.3.0

Validation concerns the current source, not historical binaries stored in this repository.

## Local platform

GCC 13, Linux x64, JUCE 8.0.4, Release VST3. Windows x64 and macOS Universal VST3/AAX are built separately by GitHub Actions. Native Linux validation does not establish Pro Tools/PACE compatibility or successful native Mac/Windows builds.

- **PASS:** CMake build and CTest DSP/processor/editor checks (2/2).
- **PASS:** ASan + UBSan DSP runner. Local LeakSanitizer is disabled because sandbox `/proc` access prevents it from inspecting threads; this is not a leak certification.
- **PASS:** pluginval 1.0.4, strictness 5, native VST3.
- Clean regression against exact v0.2 header: six qualities, driven stereo tones, **maximum sample difference 0** with Low Protect at zero.
- All modes finite on overload, mode/quality/Low Protect/Delta automation performs no audio-thread allocation.
- Quiet Clean signal null, fixed 768-sample delay, delayed bypass and Delta, float/double, mono/stereo, state roundtrip and old-state migration to Clean.
- Nine parameters; original eight parameter indices retained and mode appended.
- Three themes at 600/800/1200 widths; mode selector included in overlap checks; bypass blur and 20 editor creation/destruction cycles.

## Controlled DSP measurements

48 kHz, default 16x, input sine amplitude 2.5 (~+7.96 dBFS), THD estimated from harmonics 2–10 over the stationary final second. These tests show harmonic reduction on simple signals, not universal mastering quality or guaranteed transient preservation.

| Frequency | Clean THD | Low Protect 100% THD |
|---|---:|---:|
| 40 Hz | 27.9813% | 0.159754% |
| 55 Hz | 27.9810% | 0.0522429% |
| 80 Hz | 27.9808% | 0.0257942% |
| 100 Hz | 27.9808% | 0.0313600% |

Low Protect reduces gain to prevent flat tops; these comparisons are **not loudness matched**. A 55 Hz decaying kick changes, while the controlled 220 Hz decaying snare-body stimulus has zero squared output difference. This does not imply real snares have no energy below 100 Hz or that coincident instruments remain unaffected.

Two tones at 11 kHz and 21 kHz, each amplitude 1.2: the 1 kHz third-order difference-product projection falls from 3596.11 (Clean) to 714.339 (Cancel), about **14 dB** lower. Projection units are arbitrary and common to both renders. This validates one cancellation mechanism, not all IM products or overall loudness/quality superiority.

The uploaded DnB comparisons motivated the redesign, but the stationary tests above do not demonstrate a win over StandardCLIP on those files. No new listening verdict is claimed. No user audio is committed.

## Limits

No certified output sample/true-peak ceiling. Cancel restores low-frequency correction and can overshoot. Multiband still contains a final nonlinear projection. Analog is a generic mathematical saturator. Fold and Orbit are intentionally coloured experimental modes. Fixed-size LF FIR transitions broaden at high sample rates; the fourth-order 100 Hz detector preserves trigger selectivity but loses some effective attack look-ahead there. Real-time CPU cost should be checked at 32x/64x and with Multiband/Low Protect on the target workstation.

Raw local checks are stored under `Docs/Checks/*v0.3*`. Older v0.2/v0.1 logs and preview/binary folders are historical. Current Windows/macOS artifact status must be read from Actions.
