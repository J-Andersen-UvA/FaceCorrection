# Dutch phoneme to NGT viseme rules

This is a working ruleset. The current recorded viseme assets are in:

`/Game/Animations/VisemeRecording/PostProcessed`

The input is a sequence of spoken-Dutch phonemes. We alter the sequence and its visemes so the result looks like NGT word mouthing and remains readable on an avatar. The resulting sequence is called `avatarMouthing`. It consists of viseme blocks and `NULL` blocks.

## Processing order

1. Convert the Dutch text to phonemes while preserving sequence, stress and vowel length.
2. Create an initial block for each phoneme and fit the blocks to the original mouthing duration when that duration becomes available.
3. Replace invisible phonemes and confirmed context-dependent omissions with `NULL`.
4. Map the other phonemes to viseme poses.
5. Apply vowel length and tentative stress weighting.
6. Generate plosive release and tongue motion.
7. Produce the editable `avatarMouthing`.

## Initial viseme inventory

| Viseme | Initial phoneme group | Status |
| --- | --- | --- |
| `rest` | No visible articulation | Recorded or planned |
| `NULL` | A source phoneme without a generated visible target | Keeps its own timing and can borrow a neighbouring viseme |
| `m` | `/m/` | Separate from `bp` |
| `bp` | `/b, p/` | Requires a temporal plosive gesture |
| `fv` | `/f, v/` | Provisional mapping |
| `tdn` | `/t, d, n/` | Provisional mapping |
| `sz` | `/s, z/` | Provisional mapping |
| `l` | `/l/` | Base mouth pose; tongue is generated separately |
| `shCh` | Dutch sh/ch-like visible articulations | Exact phoneme membership still needs validation |
| `aa` | Dutch aa-like vowels | Exact phoneme membership still needs validation |
| `eh` | Dutch e/è-like vowels | Exact phoneme membership still needs validation |
| `ih` | Dutch i-like vowels | Exact phoneme membership still needs validation |
| `oh` | Dutch o-like vowels | Exact phoneme membership still needs validation |
| `oe` | Dutch oe-like vowels | Exact phoneme membership still needs validation |
| `uu` | Dutch uu-like vowels | Exact phoneme membership still needs validation |
| `eu` | Dutch eu-like vowels | Exact phoneme membership still needs validation |
| `schwa` | `/ə/` | Recorded relaxed open-mouth pose; whether it should become `NULL` depends on further observations |

## General rules

### Rule 1: Replace invisible phonemes with `NULL`

When a phoneme is invisible, we replace it with `NULL`. The initial invisible phoneme list is `/r/` and `/k/`. We can add phonemes when new examples show that they are consistently invisible. In other words:

```csharp
/taːrt/ → T-AA-NULL-T
```

### Rule 2: Preserve the original mouthing duration and sequence

When `avatarMouthing` differs from the original phoneme sequence, we preserve the duration of the original mouthing and initially preserve the relative duration of each part. A replaced phoneme becomes a `NULL` block with the same relative position and duration.

We do not yet have a programmatic method for detecting the start and end of the original mouthing. Until that method exists, the user sets the word-block duration on the timeline and the generated child blocks are fitted proportionally inside it.

### Rule 3: Stress may increase block weighting

This rule is not confirmed yet. When a phoneme or syllable is stressed, we may give its corresponding block a higher weighting.

### Rule 4: Vowel length changes timing

When we receive `:` or `ː`, we lengthen the duration of the relevant block. In other words:

```csharp
/a/  → aa with short duration
/aː/ → aa with longer duration
```

### Rule 5: Schwa mapping is unconfirmed

Do not automatically replace schwa `/ə/` with `NULL`. A relaxed open-mouth schwa pose was correct for some observed words. Keep the recorded `schwa` viseme available and decide later whether context-dependent schwa reduction is needed. Do not add a schwa rule to the current ruleset yet. The initial output is therefore:

```csharp
/'lo:pə(n)/ → L-OO-P-SCHWA-N
```

### Rule 6: Keep optional phonemes

This rule is not confirmed yet. Optional phonemes are not optional in `avatarMouthing`. We always include them to keep the output deterministic. This means we do not apply n-deletion. In other words:

```csharp
/'lo:pə(n)/ → L-OO-P-SCHWA-N
```

### Rule 7: `bp` has a plosive release

The single-frame `bp` viseme pose already has closed lips. At the end of the `bp` block, we add visible pressure during the release. In other words:

```csharp
/b, p/ → closed bp pose → visible pressure on release
```

### Rule 8: Map phonemes to viseme poses

We define a mapping from every phoneme to a viseme pose. In other words:

```csharp
/b/, /p/     → bp
/t/, /d/, /n/ → tdn
```

### Rule 9: Generate tongue motion from `avatarMouthing`

We generate tongue motion after constructing `avatarMouthing`. The `/l/` block relies on its neighbours. We select the relevant neighbouring block and include its name in the name of the `l` block. In other words:

```csharp
/lamp/ → LA-A-M-P
```

Here `LA` uses the `A` mouthing with an exaggerated `/l/` tongue motion towards the front teeth.

### Rule 10: `/t/` and `/d/` have a plosive release

When the source phoneme is `/t/` or `/d/`, we add a visible plosive ending. They may use the shared `tdn` pose, but `/n/` does not receive the plosive ending. In other words:

```csharp
/t/, /d/ → tdn pose → visible pressure on release
/n/      → tdn pose without the plosive release
```

### Rule 11: Replace `/t/` after `/s/` with `NULL`

When `/s/` is followed by `/t/`, we replace `/t/` with `NULL`. In other words:

```csharp
/ˈstikəm/ → SZ-NULL(t)-IH-NULL(k)-SCHWA-M
```

## Proposed system rules

These rules do not change the linguistic conversion. They keep the generated result traceable and editable.

### Store the source on every block

Every block stores its source phoneme, original start and end time, selected viseme pose and the rule that created it. For example, the block displayed as `NULL` still stores that it came from `/r/`.

### Make `NULL` behaviour editable

The user can right-click a `NULL` block and use the default transition, hold the previous viseme, start the next viseme early, use `rest`, or select a viseme manually.

### Start with one block per source phoneme

We initially create one block per source phoneme. This preserves Rule 2 and prevents adjacent invisible phonemes from becoming ambiguous. A later rule may explicitly combine blocks.

### Separate the source phoneme from the viseme pose

A block can keep the source name while using a shared viseme pose. For example, a block can store source phoneme `/p/`, display `P` and use the `bp` pose. This keeps examples such as `LA-A-M-P` readable without losing the mapping from Rule 8.

### Store a rule trace

Every generated word and viseme block stores the ordered rules that matched it, the changes made by each rule and the output before and after each change. Rules that were evaluated but did not match are not part of the normal trace, but can be shown in a detailed diagnostic view.

### Inspect and edit rules from timeline blocks

Right-clicking a word or viseme block opens a rule window. The window shows the rules that affected the selected block, their ordering and the result of each rule. The user can enable, disable, edit, reorder or add rules, then regenerate the selected word to inspect the result.

Editing a ruleset changes its generated results wherever that ruleset is used. Manual timeline edits remain block edits and are not converted into lexical overrides. The window must clearly separate `Edit Ruleset and Regenerate` from `Edit This Block` so an exceptional word can be corrected by hand without creating a new word-specific rule.

## Current word cases

### `taart`

| Field | Value |
| --- | --- |
| Source phonemes | `/taːrt/` with stress on its only syllable |
| `avatarMouthing` | `T-AA-NULL(r)-T` |
| Applied rules | Replace `/r/` with `NULL`; retain its duration; express vowel length through the duration of `AA` |
| Status | Initial test case; validate against recorded NGT mouthing |

### `stiekem`

| Field | Value |
| --- | --- |
| Expected source phonemes | Approximately `/ˈstikəm/`; verify against the phonemizer output |
| Observed NGT mouthing | `/s/`, `/i/`, `/m/` |
| Preliminary ruleset output | `SZ-NULL(t)-IH-NULL(k)-SCHWA-M` |
| Observed manual result | `/s/`, `/i/`, `/m/`; the schwa may be manually changed to `NULL` for this word |
| Material mapped to `NULL` by current rules | `/t/` and `/k/` |
| Applied rules | Replace `/t/` after `/s/` with `NULL`; replace invisible `/k/` with `NULL`; keep schwa unresolved |
| Status | Keep a separate block for every source phoneme and do not generalize schwa reduction yet |

## Candidate rules

### C001: A reduced mouthing may simplify internal consonants

`stiekem` suggests that an NGT mouthing may retain the visible onset, stressed vowel and final consonant while mapping internal consonants and schwa to `NULL`. Do not apply this generally yet. Compare it with new words containing internal `/t/` or `/k/` before promoting it to a general rule.

## New word observation template

### `word`

| Field | Value |
| --- | --- |
| Source phonemes |  |
| What is visible |  |
| Preliminary block sequence |  |
| Visible result |  |
| Surrounding context |  |
| Timing or strength observation |  |
| Proposed change | Example-specific observation, candidate general rule, or confirmed general rule |
| Validation status |  |

