# VoxRestore

Reference-guided, algorithmic voice restoration for watery or robotic speech damaged by heavy denoising, compression or codecs. A separate VST3 and standalone application with generated Ivory, Graphite and Walnut hardware skins, moving analog-style meters and a cyan vacuum fluorescent display.

VoxRestore uses the damaged voice's timing and pitch. A cleaner sample of the same speaker guides broad spectral balance and dynamic contour. DEWATER repairs short missing harmonics from observed phase and softens isolated spectral artifacts. REBUILD adds bounded upper harmonics derived from the actual input. It does not generate new dialogue, replay a reference waveform, or reconstruct words that have been completely removed. Strong settings can soften consonants or add buzz; audition with MIX and BYPASS.

## Use

1. Insert **VoxRestore** on the damaged voice track.
2. **LOAD REF**, or drop a cleaner 5–20 second sample of the same speaker onto the faceplate. The entire reference is analyzed automatically. Different words are fine; similar vocal delivery and the desired recording tone help.
3. Keep **MATCH** selected. Start with RESTORE 65%, DEWATER 45%, REBUILD 25% and MIX 100%, then compare with BYPASS and reduce excessive processing.

**LEARN** manually re-analyzes the whole reference file. **CLEAR** removes it; **FREE** retains artifact repair and harmonic rebuilding without reference matching. The compact profile is saved with the DAW session and restores without the source file. It contains speech statistics, not recorded audio. Loading/saving a profile does not require keeping the editor open. Use separate instances/references for different speakers.

Live processing adapts to incoming speech as the DAW delivers it; it cannot inspect unplayed host audio. Standalone **LOAD VOICE** automatically scans the whole damaged file. **PRINT WAV** performs whole-file source analysis, then renders to a new WAV with latency compensation and the original length, rate, channel layout and supported WAV depth/metadata. Recordings cannot be overwritten. The play/pause, stop, beginning and loop controls audition the damaged recording through the restoration engine. The app starts with microphone input disabled.

## Installation

Windows x64: extract the package and run `Install-VoxRestore.ps1`. The standalone is installed in `%LOCALAPPDATA%/Programs/VoxRestore`; the complete VST3 bundle goes to `%LOCALAPPDATA%/Programs/Common/VST3/VoxRestore.vst3`. It is a separate product and plug-in identifier from NoiseBase. Restart/rescan the DAW after installation. The installer verifies per-file hashes, protects unowned folders, refuses downgrades and loaded files, preserves settings, retains backups outside the VST scan folder, and rolls back failed swaps.

Apple Silicon source/build target: macOS 14 or later, native arm64 DAW. A Mac release, when published, contains `VoxRestore.app` and `VoxRestore.vst3`; copy them to Applications and `~/Library/Audio/Plug-Ins/VST3`. Packages are ad-hoc signed, without Developer ID notarization. Native Mac verification must pass before publication; a local Windows build does not verify Mac runtime behavior. No Intel or AU target is included.

## Updates, privacy and host safety

The standalone checks the product's GitHub Releases and provides a manual UPDATES menu. Windows downloads show actual byte progress, followed by verification/extraction activity. A package must match GitHub's SHA-256 digest, the checksum file and every payload hash. Close the DAW before replacing a loaded plug-in. Mac updates open the release page for manual installation. Automatic checks can be disabled. The VST3 itself does not check the network or install updates.

Audio and reference analysis run locally, without an account, cloud inference or API key. File access, neural speech detection and model construction run on an owned background job. The audio callback performs bounded, preallocated DSP and uses a nonblocking profile handoff. Jobs own their data independently of editors/processors, cancellation cannot restore an obsolete profile, and slow cleanup is deferred safely when a plug-in is removed. The module may stay mapped until the DAW exits. This addresses known lifetime hazards; it is not certification of a particular DAW.

## Verification and limits

Version 0.1.1 fixes a reproduced Direct3D deadlock when unloading the last Windows
plugin instance after editor use. Skin and VFD images use CPU-backed storage, with
software editor rendering. Windows release checks now include repeated project closures
with eight instances and complete module unloading. See [release notes](docs/Release-0.1.1.md).

The native suite covers 44.1/48/96 kHz timing, neutral/bypass alignment, stereo phase, finite audio, no generated sound on silence, compact-profile validation, loaded-state restoration, reference completion with the editor closed, manual whole-file re-analysis, canceled-job retirement, source-safe sample-aligned export and 16 real VST3 add/restore/remove cycles. Actual rendered UI captures are reviewed across all three raster skins. Pipeline checks cover Windows and Apple Silicon, installer rollback, update policies, progress reporting and source/archive secret scans.

The supplied demonstration uses locally synthesized speech: a cleaner reference and a different held-out utterance, deliberately damaged with changing spectral holes, filtering and compression. Its numeric comparison measures that controlled example, not perceptual quality on every recording. Real heavily damaged dialogue still requires auditioning. Completely missing phonemes, clipping, overlapping voices and strong reverberation are outside reliable reconstruction.

Latency is 2048 samples at 44.1/48 kHz and 4096 at 96 kHz. Host dry/wet and bypass remain aligned; file export removes this delay. Physical audio devices and your particular DAW are not certified by the native stress host.

## Source and license

`Build-source.ps1` builds Windows VST3 and standalone using Visual Studio 2022 C++ Build Tools and CMake 3.22+. All JUCE source, ONNX Runtime CPU headers/runtimes, Silero model and generated artwork are vendored. On Mac, configure arm64 with `-DCMAKE_OSX_DEPLOYMENT_TARGET=14.0`, then build `VoxRestore_VST3 VoxRestoreApp`.

Source is **AGPL-3.0-or-later**, using JUCE's AGPL option. Preserve the [license](LICENSE) and [third-party notices](Licenses). The generated raster hardware artwork and VFD implementation are shared with NoiseBase. Asset prompts are included in `resources/generated`; no SVG interface artwork is used. No user film/audio is included in this source or the synthetic fixtures. JUCE 9.0.6 and ONNX Runtime CPU 1.32.0 are included.
