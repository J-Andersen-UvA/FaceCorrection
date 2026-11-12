# FaceCorrection

`FaceCorrection` is an Unreal Engine 5.7 plugin for non-destructive facial animation post-processing. Its first workflow will generate Dutch mouthing corrections with text-to-speech and NVIDIA Audio2Face. The plugin is intentionally named more broadly so mouth gestures and facial profiles can use the same timeline and curve-composition system later.

## Current state

The repository currently provides the plugin foundation:

- A runtime module and editor module.
- `UFaceCorrectionProfile`, an Unreal Data Asset containing the curve allowlist and per-curve mapping settings.
- `FFaceCorrectionBlock`, a timeline correction with transition frames, text and generated curve samples.
- Project settings for machine-specific Audio2Face and text-to-speech configuration.
- Evaluation helpers for profile mapping and correction envelopes.

The HandposeTimeline lane, text-to-speech provider and in-process Audio2Face bridge are tracked in [TODO.md](TODO.md).

## Repository layout

This repository is the plugin root. Link the entire directory into an Unreal project's `Plugins` directory:

```text
Reallusion/Plugins/FaceCorrection -> this repository
```

Generated Unreal files, Audio2Face binaries and models are excluded from Git.

## Face correction profiles

Create a profile in Unreal through:

```text
Content Browser -> Add -> Miscellaneous -> Data Asset -> FaceCorrectionProfile
```

Each enabled curve rule is part of the allowlist. A rule maps one generated source curve to an avatar target curve and can apply a multiplier, offset, blend mode and clamp range. Curves not listed in the profile are never changed.

Profiles contain artistic and rig-specific settings. Machine-specific paths belong in:

```text
Project Settings -> Plugins -> Face Correction
```

## Dependencies

The planned Audio2Face integration uses NVIDIA's Audio2Face C++ SDK through a small bridge DLL. Do not commit CUDA, TensorRT, NVIDIA runtime binaries or model files until their redistribution requirements have been checked.

## Build

The plugin targets the Reallusion Unreal Engine 5.7 project. Once linked into its `Plugins` directory, build the `ReallusionEditor` target normally.
