# Validation 0.6.0

GR regression: unit-amplitude 997 Hz sine at 48 kHz, Input +25 dB, hard Clean, Ceiling 0. Maximum GR is 25 dB with Output 0 or +12 dB. A decaying 55 Hz + 3 kHz drum stimulus with Low Protect enabled/disabled during playback checks that startup, waveform zeros and protection changes do not generate inflated GR (maximum 24.17 dB). On the supplied DnB dry file, Input +25 gives maximum GR 21.77 dB with protection off / 22.19 dB with protection 100; both outputs peak at 1.0. User audio is not committed.

DSP checks cover six sample rates/qualities, delayed quiet null, bypass, finite sanitization, quality automation, LF protection, no audio-thread allocations and sample boundaries before final Output. Output +1 dB on saturated Ceiling 0 reaches +1 dBFS; Ceiling −6 plus Output −3 stays within −9 dBFS. Low Protect's steady-state THD and direct completed-half-wave ratio tests remain.

Integration checks cover eight parameter IDs, Link off by default, Input +10 ↔ Output −10, reverse editing, linked range boundaries, state migration removing old Style/Knee, mono/stereo, float/double, offline quality, three themes/window sizes, independent peak reset, blur and repeated editor lifetimes. No listening test or superiority over other clippers is claimed. Historical 0.5 IMD measurements remain in Docs/Checks; Punchy is no longer exposed in 0.6.

Native build, CTest and pluginval output are recorded in Docs/Checks. Windows/macOS VST3+AAX checks run in GitHub Actions and do not substitute for PACE distribution signing.
