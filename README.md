# FaceCorrection

`FaceCorrection` is an Unreal Engine 5.7 plugin for non-destructive facial animation post-processing. Its first workflow is a human-in-the-loop Dutch word-mouthing timeline. Text is converted to phonemes, the user reviews the phoneme-to-viseme list, and editable viseme blocks are placed inside a movable and scalable word block.

## Current state

The repository currently provides the plugin foundation:

- A runtime module and editor module.
- `UFaceCorrectionProfile`, an Unreal Data Asset containing the curve allowlist and per-curve mapping settings.
- `FFaceCorrectionBlock`, a timeline correction with transition frames, text and generated curve samples.
- Project settings for machine-specific Audio2Face and text-to-speech configuration.
- Evaluation helpers for profile mapping and correction envelopes.
- `UFaceCorrectionConfig`, an Unreal Data Asset containing the Piper profile, phoneme mappings, viseme animations, colors and default timing.
- A soft HandposeTimeline extension that adds Face Correction settings and a shared timeline lane.
- Text entry, Piper phoneme conversion and a draggable phoneme/viseme review dialog.
- Editable parent word blocks, child viseme blocks and non-destructive avatar preview.

Viseme reconstruction refinements, baking and the future automatic NGT ruleset are tracked in [TODO.md](TODO.md).

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

## Face correction timeline config

Create the timeline config through:

```text
Content Browser -> Add -> Miscellaneous -> Data Asset -> FaceCorrectionConfig
```

Set its Piper voice profile. In the config Details panel, click `Populate Visemes from Folder`, choose the Content Browser folder containing the viseme animation sequences, and the plugin fills the viseme array recursively with distinct colors. Click `Populate Dutch Phoneme Mappings` to add the Dutch phoneme inventory and fill confident mappings from the available visemes. Unmatched phonemes remain empty so they can be assigned manually or set to `NULL`.

Create a second Data Asset using `FaceCorrectionRuleSet` and assign it to `Mouthing Rule Set` in the Face Correction config. The ruleset stores the available rule definitions. Each rule has a stage, conditions and actions. Edit those definitions on the ruleset asset itself. Right-click a word or viseme block and choose `Block Mouthing Rules` to view that block's ordered rule assignments. `Add Rule` opens a picker containing the enabled rules from the configured ruleset. Rule execution is intentionally not connected yet, so assigning a rule records it on the block without changing the animation.

Add the mouth curves that Face Correction is allowed to replace to `Apply To Blendshapes`. Curves outside this allowlist remain unchanged. Set `Jaw Bone` when the recorded viseme animations also contain the jaw pose. The jaw local transform is blended with the same timing as the blendshapes.

Open HandposeTimeline, click `Face Correction`, choose the config in the settings menu, enable the timeline and click an empty part of the lane to insert a word. Drag the handle beside a review row to rearrange it. Selecting a word expands its influence graph. The first viseme curve shows the word blend-in, the last curve shows the word blend-out, and adjacent child visemes crossfade at their shared border. Drag a white timing triangle to move a shared border. Drag the two colored triangles beneath a crossfade to control its transition range. Right-click a transition handle to choose its interpolation type. The shared ruler above the scrollable lanes scrubs the preview animation and displays the trim range. Loading another animation clears the current Face Correction words. Select a word and press `Delete` to remove it.

## Dependencies

HandposeTimeline and PiperTTS are optional plugin dependencies. The editor integration is compiled when those plugins are available. The deferred Audio2Face comparison path still requires separate redistribution review for NVIDIA, CUDA and TensorRT files.

## Build

The plugin targets the Reallusion Unreal Engine 5.7 project. Once linked into its `Plugins` directory, build the `ReallusionEditor` target normally.
