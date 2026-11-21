# NoiseBase dependency packaging

JUCE is vendored from version 9.0.6. The Windows NoiseBase build uses the framework modules and CMake support; it does not build the Android demo applications.

The following unused Android Firebase configuration files are intentionally omitted because they contain JUCE's upstream demo Google API key:

- `JUCE/examples/Assets/google-services.json`
- `JUCE/examples/DemoRunner/Builds/Android/app/src/main/assets/google-services.json`
- `JUCE/extras/AudioPluginHost/Builds/Android/app/src/main/assets/google-services.json`

NoiseBase does not use Firebase, Google APIs, or these demo credentials. Omitting these files does not change the Windows application or VST3 audio processing. Upstream licenses remain intact. Building JUCE's Android push-notification demos would require your own Firebase configuration.

Publication checks must include all vendored dependencies and nested source archives, not only first-party code. Use Gitleaks with full redaction and archive scanning before publishing.

Apple Silicon runtime: `runtimes/osx-arm64/native/libonnxruntime.dylib` is extracted from the same Microsoft.ML.OnnxRuntime 1.32.0 NuGet package as the existing Windows DLL (Windows DLL hashes matched before extraction). It is native arm64 and declares macOS 14.0 as its minimum OS. The existing ONNX Runtime license and third-party notices apply.

- Official package: https://api.nuget.org/v3-flatcontainer/microsoft.ml.onnxruntime/1.32.0/microsoft.ml.onnxruntime.1.32.0.nupkg
- Package SHA-256: `a9150bc2992cbbb6722070da9910e1ffa989a8bfc67bdd5bb8e745ca755641db`
- Original Mac dylib SHA-256: `21ff304871400e846a30c8a1165d15173add56598afb45f98227e2dfc716a497`

Mac bundle assembly applies a local ad-hoc code signature to bundled binaries; the final bundle hashes therefore differ from the original vendor signature. No Developer ID identity or Apple notarization is supplied.
