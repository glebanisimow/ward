# Ward

A layered instrument that treats medical-device sounds as raw material — stretch,
process and stack them into drones and noise (not an ICU simulation). Standalone
app + VST3 (Windows).

Each device is a Faust engine under `engines/devices/`; the app runs 12 layer
slots, each with granular processing and an FX chain, plus WAV recording.

| Devices | File |
|---|---|
| monitor, alarm, ventilator, pump, concentrator, doppler, defib, mri, room | `engines/devices/*.dsp` |

`lib/devices.lib` and `lib/contract.lib` hold the shared Faust contract.

## Build (Windows)

Needs VS 2022 C++ Build Tools, CMake, and Faust + JUCE under
`%LOCALAPPDATA%\drone-tools`. Device DSP is generated at build time from
`engines/devices/*.dsp`.

```powershell
.\app\build.ps1              # Release build + dist\Ward-Setup-<ver>.exe
.\app\build.ps1 -NoInstaller
```

Output: `Ward.exe` (Standalone) and `Ward.vst3`.

---
Split out of the `drone-lattice` monorepo; the Drone Lattice synth and the Drone
Studio host live in their own repositories.
