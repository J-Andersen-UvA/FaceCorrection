# FaceCorrection todo

## 1. Repository and plugin foundation

- [x] Create the Unreal Engine 5.7 `FaceCorrection` plugin structure.
- [x] Add runtime and editor modules.
- [x] Add an Unreal-focused `.gitignore` and repository documentation.
- [x] Initialize the local Git repository.
- [x] Create a directory link at `Reallusion/Plugins/FaceCorrection`.
- [x] Verify that the plugin compiles with the Reallusion editor target.

## 2. Face correction data model

- [x] Add `UFaceCorrectionProfile`, creatable as a Data Asset in Unreal.
- [x] Store the curve allowlist as editable curve rules.
- [x] Support source-to-target curve names, multiplier, offset and output clamping.
- [x] Add replace and additive curve blend modes.
- [x] Add default strength and transition timing to the profile.
- [x] Add `FFaceCorrectionBlock` with correction type, text and timeline frames.
- [x] Store generated named curve samples without depending on CSV files.
- [x] Keep machine-specific paths out of profile assets.

## 3. Audio2Face bridge (deferred)

The direct Piper-to-Audio2Face path did not produce NGT mouthings that were clearer than the existing ARKit animation. Keep this as a comparison path, not the current implementation direction.

- [ ] Define a small C ABI between Unreal and an Audio2Face bridge DLL.
- [ ] Adapt the existing local Audio2Face executor code to accept PCM audio in memory.
- [ ] Return timestamps, curve names and weights in memory.
- [ ] Load Audio2Face, CUDA and TensorRT dependencies without blocking the editor UI.
- [ ] Add cancellation, progress and actionable error reporting.
- [ ] Keep the existing CSV exporter available as a diagnostic fallback.
- [ ] Check SDK and model redistribution requirements before committing binaries or models.

## 4. Phoneme input

- [x] Use Piper's Dutch eSpeak phonemizer through the Piper voice profile.
- [x] Store the Piper voice profile in a Face Correction config Data Asset.
- [x] Convert entered Dutch text to a preliminary phoneme sequence.
- [x] Let the user review the original phoneme text before creating timeline blocks.
- [ ] Let the user correct exceptional words through the existing phoneme, viseme and timing review instead of storing lexical overrides.

## 5. Dutch IPA to NGT mouthing conversion

- [ ] Use the IPA-style phoneme sequence returned by Piper's eSpeak phonemizer as input.
- [ ] Preserve syllable boundaries, vowel length and stress information during parsing.
- [x] Define an editable `UFaceCorrectionRuleSet` Data Asset with ordered rules, stages, conditions and actions.
- [x] Let a Face Correction config reference a mouthing ruleset.
- [ ] Preserve the original mouthing duration and initially preserve relative phoneme durations inside it.
- [ ] Research or design a method for detecting the original mouthing start and end; until then, use the word-block duration set by the user.
- [ ] Map Dutch phonemes to visible NGT mouthing targets instead of treating the output as another phoneme sequence.
- [ ] Generate an explicit `null` block for every phoneme without a dedicated visible target, such as some realizations of `/r/`.
- [ ] Store the source phoneme and its timing on each `null` block.
- [ ] Let a `null` block use its natural transition, the previous viseme, the next viseme, `rest`, or a manually selected viseme.
- [ ] Make the stressed syllable prominent, with configurable strength and duration on its visible targets.
- [ ] Test when schwa `/ə/` should use the relaxed open-mouth `schwa` pose and when it should become `NULL`; do not add a general schwa rule yet.
- [ ] Keep the original phoneme sequence alongside the reduced viseme sequence for inspection and regeneration.
- [ ] Represent each generated viseme with its type, start, end, strength, stress and source phoneme range.
- [ ] Decide how to represent the visible plosive quality of `b` and `p`: a reusable `bp` gesture with closure, pressure and release phases, or separate viseme blocks.
- [ ] Keep `m` separate from `bp`; `m` should hold the lip closure without the plosive release.
- [ ] Test whether `b` and `p` can share the same gesture with different release intensity and timing.
- [ ] Let the rules generate the chosen `bp` gesture so it only needs to be hand-authored once and remains editable on the timeline.
- [ ] Generate a plosive ending for `/t/` and `/d/` while allowing `/n/` to use the shared `tdn` pose without the plosive ending.
- [ ] Generate tongue motion programmatically as a context-dependent layer instead of including it in one fixed viseme pose.
- [ ] For `/l/`, use the preceding and following phonemes to determine the tongue target, timing and strength.
- [ ] Validate the generated `/l/` tongue motion against recordings in different phoneme contexts.
- [ ] Define initial test cases, including `taart`: `/taːrt/` to visible `T-AA-T`, with no dedicated `/r/` target and emphasis on the long `/aː/` target.
- [ ] Validate general rules with deaf NGT users rather than assuming spoken-Dutch visibility rules are NGT rules.

## 6. HandposeTimeline extension seam

- [x] Add a public extension interface to HandposeTimeline.
- [x] Allow an extension to register a toolbar toggle.
- [x] Allow an extension to add one shared lane below both hand tracks.
- [x] Share timeline geometry, zoom, trim and playhead state with extension lanes.
- [x] Add a preview hook without making HandposeTimeline depend on FaceCorrection.
- [ ] Add a bake hook without making HandposeTimeline depend on FaceCorrection.
- [ ] Move FaceTouchCorrection toward the same extension seam if practical.

## 7. Face correction UI

- [x] Add `Face Correction` to the HandposeTimeline toolbar.
- [x] Open Face Correction settings from the toolbar and select the config there.
- [x] Add one shared `Face Correction` lane.
- [x] Add a frame ruler that scrubs the shared HandposeTimeline preview.
- [x] Add `UFaceCorrectionConfig` for the Piper profile, viseme assets, mappings, colors and default timing.
- [x] Add a config button that populates viseme definitions recursively from a Content Browser folder.
- [x] Assign stable random-looking colors when populating visemes from a folder.
- [x] Add a config button that creates Dutch phoneme mappings and fills confident viseme matches.
- [x] Leave uncertain phoneme mappings empty for manual viseme or `NULL` selection.
- [x] Create a parent mouthing block by clicking the timeline and entering text.
- [x] Convert the text to phonemes and open a human review step before creating blocks.
- [ ] Run the NGT ruleset before presenting the future preliminary viseme list.
- [ ] Store an ordered rule trace on every generated word and viseme block.
- [ ] Add a right-click rule window showing which rules affected the selected word or viseme and what each rule changed.
- [x] Add a block-specific right-click rule window with ordered rule assignments and a picker sourced from the configured ruleset.
- [ ] Let the rule window enable, disable, edit, reorder and add rules, then regenerate the selected word.
- [ ] Keep ruleset edits separate from manual block edits so exceptional words can be corrected without creating lexical overrides.
- [x] Display the reviewed viseme sequence as child blocks inside the parent word block.
- [x] Allow each child viseme block timing to be adjusted through shared borders.
- [x] Keep adjacent child visemes contiguous through one shared timing border.
- [x] Add per-boundary transition ranges and selectable interpolation types.
- [x] Allow visemes to be appended, removed, replaced and rearranged in the review list.
- [ ] Add right-click actions for changing the behaviour of a `null` child block.
- [ ] Allow stress and strength to be edited per child viseme.
- [ ] Preserve manual child-block edits until the user explicitly regenerates the parent block.
- [ ] Show queued, generating, ready and failed states on blocks.
- [x] Support moving and scaling parent blocks while scaling their child blocks proportionally.
- [x] Delete a selected word with the shared HandposeTimeline Delete shortcut.
- [x] Display the shared HandposeTimeline trim range in the Face Correction lane.
- [x] Support blend-in and blend-out handles.
- [ ] Add correction strength and profile selection.
- [ ] Preserve generated curves when only timeline timing changes.
- [ ] Regenerate only when text or generation settings change.

## 8. Curve composition

- [x] Touch only blendshape curves listed by the selected Face Correction config.
- [ ] Time-warp generated curves to the correction content range.
- [x] Crossfade between the original and generated values at block boundaries.
- [ ] Apply source-to-target mapping, multiplier and offset before blending.
- [x] Preserve eyes, eyebrows, emotion and semantic curves unless explicitly allowlisted.
- [ ] Make cheek, mouth-corner and tongue ownership explicit in profiles.
- [x] Apply the configured jaw-bone pose and blend it with the word handles.
- [ ] Define deterministic overlap behavior for multiple correction blocks.
- [x] Preview corrections without modifying the source animation.
- [ ] Bake to a new animation asset.

## 9. Default profiles and validation

- [ ] Create a Character Creator mouthing profile.
- [ ] Create a Character Creator mouthing-with-tongue profile.
- [ ] Compare original ARKit, direct Audio2Face and masked/crossfaded Audio2Face results.
- [ ] Test representative Dutch words and short phrases with lab workers.
- [ ] Compare generated viseme sequences with attested NGT word mouthings.
- [ ] Tune curve multipliers and transition defaults from those tests.
- [ ] Warn when timing compression or expansion is likely to look unnatural.
- [ ] Verify save, reload, undo, redo and bake behavior.
- [ ] Verify missing model, missing dependency and generation failure behavior.

## 10. Future correction sources

- [ ] Add reusable mouth gestures.
- [ ] Add broader facial profiles.
- [ ] Add optional per-block curve-rule overrides if profile-level control is too broad.
