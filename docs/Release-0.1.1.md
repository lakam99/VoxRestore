# VoxRestore 0.1.1

Fixes a Windows project-close hang when the last VoxRestore instance is removed
after an editor has been opened. GPU-backed skin images could trigger Direct3D
resource creation while the plugin DLL was unloading.

- Retain CPU-backed skin/VFD images and use software editor rendering.
- Release temporary native image-cache entries before DLL static teardown.
- Add native teardown checks covering eight instances, editor open/close, repeated
  project closure, complete module unloading and worker-initiated close requests
  marshalled to the UI thread.
- Ship the teardown reports and stage logs with the Windows package.

Restoration DSP, parameter IDs and saved reference profiles are unchanged.
Existing projects retain their controls and reference data.

Local mixed-project validation closed eight NoiseBase and eight VoxRestore instances
over five cycles without the reproduced deadlock. The longest processor-removal phase
was approximately 178 ms; native Studio Pro confirmation requires retesting the affected song.

Windows x64 and Apple Silicon packages follow the existing verification pipeline.
Mac packages are ad-hoc signed, not Developer ID signed or notarized.
