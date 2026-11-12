# FaceCorrection todo

## 1. Repository and plugin foundation

- [x] Create the Unreal Engine 5.7 `FaceCorrection` plugin structure.
- [x] Add runtime and editor modules.
- [x] Add an Unreal-focused `.gitignore` and repository documentation.
- [x] Initialize the local Git repository.
- [x] Create a directory link at `Reallusion/Plugins/FaceCorrection`.
- [ ] Verify that the plugin compiles with the Reallusion editor target.

## 2. Face correction data model

- [x] Add `UFaceCorrectionProfile`, creatable as a Data Asset in Unreal.
- [x] Store the curve allowlist as editable curve rules.
- [x] Support source-to-target curve names, multiplier, offset and output clamping.
- [x] Add replace and additive curve blend modes.
- [x] Add default strength and transition timing to the profile.
- [x] Add `FFaceCorrectionBlock` with correction type, text and timeline frames.
- [x] Store generated named curve samples without depending on CSV files.
- [x] Keep machine-specific paths out of profile assets.

## 3. Audio2Face bridge

- [ ] Define a small C ABI between Unreal and an Audio2Face bridge DLL.
- [ ] Adapt the existing local Audio2Face executor code to accept PCM audio in memory.
- [ ] Return timestamps, curve names and weights in memory.
- [ ] Load Audio2Face, CUDA and TensorRT dependencies without blocking the editor UI.
- [ ] Add cancellation, progress and actionable error reporting.
- [ ] Keep the existing CSV exporter available as a diagnostic fallback.
- [ ] Check SDK and model redistribution requirements before committing binaries or models.

## 4. Text-to-speech

- [ ] Define a text-to-speech provider interface.
- [ ] Select a Dutch text-to-speech provider.
- [ ] Generate mono PCM audio suitable for Audio2Face.
- [ ] Keep display text separate from a future pronunciation override.
- [ ] Store provider and voice settings outside correction profiles.
- [ ] Cache generated audio by text, voice and settings.

## 5. HandposeTimeline extension seam

- [ ] Add a public extension interface to HandposeTimeline.
- [ ] Allow an extension to register a toolbar toggle.
- [ ] Allow an extension to add one shared lane below both hand tracks.
- [ ] Share timeline geometry, zoom, trim and playhead state with extension lanes.
- [ ] Add preview and bake hooks without making HandposeTimeline depend on FaceCorrection.
- [ ] Move FaceTouchCorrection toward the same extension seam if practical.

## 6. Face correction UI

- [ ] Add `FaceCorrectionModus` to the HandposeTimeline toolbar.
- [ ] Add one shared `Face Corrections` lane.
- [ ] Create mouthing blocks from selected timeline ranges.
- [ ] Add Dutch text entry and a Generate action.
- [ ] Show queued, generating, ready and failed states on blocks.
- [ ] Support moving and scaling blocks.
- [ ] Support blend-in and blend-out handles.
- [ ] Add correction strength and profile selection.
- [ ] Preserve generated curves when only timeline timing changes.
- [ ] Regenerate only when text or generation settings change.

## 7. Curve composition

- [ ] Touch only curves listed by the selected profile.
- [ ] Time-warp generated curves to the correction content range.
- [ ] Crossfade between the original and generated values at block boundaries.
- [ ] Apply source-to-target mapping, multiplier and offset before blending.
- [ ] Preserve eyes, eyebrows, emotion and semantic curves by default.
- [ ] Make cheek, mouth-corner and tongue ownership explicit in profiles.
- [ ] Define deterministic overlap behavior for multiple correction blocks.
- [ ] Preview corrections without modifying the source animation.
- [ ] Bake to a new animation asset.

## 8. Default profiles and validation

- [ ] Create a Character Creator mouthing profile.
- [ ] Create a Character Creator mouthing-with-tongue profile.
- [ ] Compare original ARKit, direct Audio2Face and masked/crossfaded Audio2Face results.
- [ ] Test representative Dutch words and short phrases with lab workers.
- [ ] Tune curve multipliers and transition defaults from those tests.
- [ ] Warn when timing compression or expansion is likely to look unnatural.
- [ ] Verify save, reload, undo, redo and bake behavior.
- [ ] Verify missing model, missing dependency and generation failure behavior.

## 9. Future correction sources

- [ ] Add reusable mouth gestures.
- [ ] Add broader facial profiles.
- [ ] Add pronunciation or phoneme overrides where normal Dutch spelling is insufficient.
- [ ] Add optional per-block curve-rule overrides if profile-level control is too broad.
