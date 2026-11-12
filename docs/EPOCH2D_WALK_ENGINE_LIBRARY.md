# Epoch2DWalkEngine library and Runner example

## Product layout

`Epoch2DWalkEngine` is the reusable cross-platform C++23 locomotion library. `Runner` is the complete SDL3/Vulkan example application and trainer built on that library. The split is architectural, not a feature reduction: Runner retains Live Autopilot, Rig Lab, background PPO training, persistence, diagnostics, authored rigs, modular art, and the SandHybrid course.

The v0.7.51 package surface described here is a candidate contract. Version metadata and install rules are updated, but Raw retained-network inference, full platform matrices, installed/extracted audits, direct bitmap acceptance, exact archives, commit/tag identity, and publication remain release gates rather than completed evidence.

The library owns:

- species-aware rig topology, validation, serialization, and morphology;
- articulated particle/constraint physics, support contacts, water and terrain interaction;
- observations, actions, locomotion planning, authored gait reflexes, PPO, retention, and curriculum;
- checkpoint and autonomy persistence;
- deterministic acceptance/course/hybrid-brain/rig-training diagnostics;
- renderer-neutral modular-art transforms and species attachment metadata.

The diagnostics distinguish passive physical constraints from optional locomotion guidance. Raw audits disable teacher, scripted gait replacement, topology locomotion reflex, posture/swing/manipulator projection, root drive, and course-motion authority; library consumers can inspect the authority report. A Raw release result must additionally expose per-joint network command versus effective motor command, physical contact transitions, foot slip, root displacement, and injected-displacement/work evidence. The current v0.7.51 retained-network Raw gate remains open, so Assisted results cannot be presented as independent policy inference.

Runner owns SDL events, Vulkan resources, shaders, canvas batching, window layout, UI hit testing, fonts, panels, and packaging of executable assets. It links the library once through `Epoch2DWalkEngine::Epoch2DWalkEngine`; core sources are not compiled again into the example.

## Installed CMake usage

```cmake
cmake_minimum_required(VERSION 3.28)
project(MyWalker LANGUAGES CXX)

find_package(Epoch2DWalkEngine 0.7.51 EXACT CONFIG REQUIRED)
add_executable(my_walker main.cpp)
target_link_libraries(my_walker PRIVATE
    Epoch2DWalkEngine::Epoch2DWalkEngine)
target_compile_features(my_walker PRIVATE cxx_std_23)
```

```cpp
#include <Epoch2DWalkEngine/Engine.hpp>

int main()
{
    auto rig = epoch2dwalk::sim::CreatureBlueprint::humanoid();
    epoch2dwalk::sim::Environment world{rig, 42u};
    world.set_course(epoch2dwalk::sim::CourseStage::uneven, 0.25f);
    epoch2dwalk::integration::ControlRequest request{};
    request.mode = epoch2dwalk::integration::ControlMode::training;
    request.joint_control =
        epoch2dwalk::integration::JointControlSource::expert_joint_actions;
    request.guidance =
        epoch2dwalk::integration::GuidanceRequest::raw_policy;
    const auto result = epoch2dwalk::integration::fixed_step(
        world, request, 1u, 1u);
    return result.control_applied ? 0 : 1;
}
```

The package also requires `Threads` and the exact branded `EpochSimEngine 2.5.29` package; its generated config resolves both before loading the exported target. v2.5.29 is the publicly consumable pin for v0.7.51. The package does not substitute the legacy compatibility package name.

The exported target advertises the install root's `include/` directory, while every Epoch2DWalkEngine header remains beneath `include/Epoch2DWalkEngine/`. Release CI installs Epoch2DWalkEngine and EpochSimEngine into two independent prefixes and compiles/runs this consumer with both prefixes in `CMAKE_PREFIX_PATH`. That split-prefix test prevents the dependency's include root from concealing a broken Epoch2DWalkEngine export.

## External host and authoring API

The umbrella header exports `director::TaskGraph`, `director::State`, `integration::TaskCommand`, `integration::ControlRequest`, `integration::CanonicalSnapshot`, `integration::FixedStepResult`, and `integration::DiagnosticEvents`. `integration::fixed_step()` applies an authenticated task only at an explicit host boundary and otherwise advances the caller-owned physical environment exactly once. A command is accepted only when its task index, stable ID, task/challenge pair, prerequisites, evidence, and profile reproduce `director::select()` for the supplied graph and `ControlRequest::mode`. Curriculum, autonomous-NPC, and player-guided hosts therefore share the same contacts, materials, equipment state, projectiles, terminal causes, and fixed-step clock without trusting a caller-provided support flag.

Every request must explicitly name a `JointControlSource` and `GuidanceRequest`; default/unknown, non-finite, wrong-rig, stale-semantics, provenance-mismatched, and mixed retained-plus-expert requests fail before the fixed-step clock or physics advances. `expert_joint_actions` is the deliberately named low-level host path. `player_guided` instead requires a versioned `PlayerIntent`, an authenticated task, Raw guidance, and a `RetainedPolicyController` loaded from the compatible checkpoint's retained-best parameters. The player can request signed travel direction, momentarily stop a selected moving gait without changing its Director node, select a task/gait, and authorize fire while the retained network supplies every joint command. Signed direction and stop feed planner, observation, and reward intent only; they never translate a root or apply a body force. Switching back to autonomous or training control explicitly restores forward, non-stopped planner intent so player stop state cannot leak across authority modes. Interact and tool intents reject until their real physical implementations exist, and fire authorization never grants a shot or hit.

The result publishes both requested network/expert actions and the physically conditioned motor actions. Its complete `ControlAuthorityReport` includes retained checkpoint semantics/rig/guidance provenance plus teacher, topology, safety, posture, swing, contact, and mandatory joint-cluster authority. Player-guided requests reject Assisted checkpoints or Assisted execution, so another project cannot mistake a projected replay for independent retained-policy control.

The Director never owns a renderer or grants motion. Its stable task/challenge identity and selection reason can be sent to another project beside the canonical snapshot and action request. Each task node now binds both its stable task ID and the stable ID of the physical challenge it expects. `ChallengeResolution` identifies the real material interval, terrain boundary, generated course feature, or active equipment target with that stable challenge ID and course coordinate. Unsupported recovery, construction, and tool nodes are disabled and rejected rather than simulated with placeholder authority; the current static range is not advertised as a moving target. `compatible_task_graph()` also disables equipment-only tasks and their dependent chains for bodies without manipulators.

Graphs reject duplicate or inconsistent task/challenge IDs, invalid enum/range/finite values, duplicate or cyclic prerequisites, and inconsistent persisted current selections. `reduce_evidence()` is a pure trial-result reducer, but scalar completion is no longer sufficient for physical water or stop/plant/aim/fire-and-correct nodes. Water mastery requires one trial-bound and tick-ordered approach, entry, wading (and swimming for Swim), measured depth and body submersion, shore-exit phase, and later far-shore dry observation. Equipment mastery requires a timed two-foot plant, settled physical aim, a real missed shot with finite error, an observed opposite-signed correction bounded to 0.22 radians, and a later physical shot with lower error and a hit. The receipt is bound to schema, trial ID, stable task ID, stable challenge ID, and tick interval; replay, wrong challenge, missing phase, out-of-order, non-finite, hit-only, and scalar-only evidence fail without mutating mastery.

Accepted physical receipts are persisted per node under Director state schema 2 and autonomous-state format 29. Director schema 1 and autonomous format 28 cannot be silently upgraded because they carried scalar mastery without these receipts. Older pre-Director autonomous formats remain ordinary settings/checkpoint migrations and cannot manufacture physical Director mastery. State replacement remains atomic and never deletes the accepted destination first. The current PPO `TrainingMetrics` aggregate does not retain an ordered water trace or physical plant/aim/miss/correction/next-shot sequence, so automatic curriculum mastery for those nodes deliberately remains ungrantable until that evaluator publishes the same evidence; distance, stance, transition, shot, and hit totals are not substituted.

Persistence APIs accept explicit caller-owned paths; no default save directory, process working directory, UI singleton, or environment variable is consulted by the external fixed-step boundary. The public atomic-replacement header is declaration-only. Its Windows/POSIX implementation and operating-system headers are compiled inside the library, so including `<Epoch2DWalkEngine/Engine.hpp>` does not expose platform headers to a consumer.

`CanonicalSnapshot` schema 5 records API/training semantics, rig identity, full particle pose/velocity/inverse mass, semantic root/torso/contact indices and poses, mass-weighted center-of-mass position and velocity, every active motor diagnostic with its action slot and three canonical node identities, root/left/right canonical 16-byte material cells with global coordinates, contact phase/traction/water/stall/equipment state, locomotion intent, requested and applied actions, the full guidance report, and the opt-in per-source Raw authority ledger. Its action-layout schema 2 identifies all 15 public action roles and the topology/stage-active mask: 12 anatomy motors followed by 3 separate equipment actions. `canonical_snapshot_valid()` fails closed on stale schemas, malformed indices or action roles, nonzero inactive commands, hidden array-tail data, non-finite physical or authority values, and inconsistent center-of-mass evidence. The payload lets a host audit physical authority; it does not prove that an arbitrary retained network learned a viable gait. It contains no UI pointer, renderer state, filesystem path, or hidden trainer singleton. `RuntimeSnapshot` remains a source-compatible alias.

`art::Layout` is renderer-neutral, species-owned authoring data. `art::adjust_transform()` applies bounded module anchor, pivot, length, thickness, rotation, flip, and depth settings to the same physical transforms Runner renders in Live, PIP, and Rig Lab. Layout save/load validates schema and species identity; undo/redo is deterministic and bounded.

The public integration API version is `7`. Consumers should compare `integration::api_version`, `canonical_snapshot_schema_version`, `action_layout_schema_version`, `player_intent_schema_version`, and `rl::training_semantics_version` when they persist or transmit these contracts. API 7 binds canonical snapshot schema 5 and action-layout schema 2 to the expanded Human motor/observation ABI while retaining retained-policy provenance schema 3: Raw control is valid only when an opaque artifact issued by the real trainer or exact-EOF loader supplies a verified post-handoff, all-Raw, nonzero-advantage optimizer receipt bound to the exact retained parameters and current stage, gait, equipment, and action mask. Ordinary consumers cannot construct or mark that artifact verified.

The v0.7.51 physical two-hand equipment ABI and historical 76-observation/13-action policy layout first entered under training semantics `0x0007'5109`; `0x0007'5110` is the historical foundational-input/mixed-origin boundary, `0x0007'5111` is the historical whole-composite authority/plant boundary, `0x0007'5112` is the support-feedback observation boundary, and `0x0007'5113` is the CrouchWalk authority boundary. Current semantics are `0x0007'5115`: they bind the canonical RUNRIG7 Human to 84 observations and 15 actions, with explicit distal-toe motors and policy-visible support-phase-coordinator state. The 5110 boundary made Assisted and Raw network inputs both scrub private gait phase, used a 300-update per-environment ramp to replace Assisted origins with Raw origins, admitted only Raw origins to PPO actor gradients during mixed updates, and admitted only Assisted origins to the separate decaying imitation pass. The 5111 boundary additionally faded the entire Human Uneven Assisted composite—including history filtering, startup/body clamps, bilateral synergy, the manipulator envelope, and optional guidance—to exact sampled-action identity and gave moving Human Assisted and Raw the same actuator-slew/damping plant. The 5112 boundary preserved the then-current 76-value shape while replacing only the two canonical Human Uneven Walk private-clock slots with finite previous-step facing-local capture error and anatomical right-minus-left support-load asymmetry. The 5113 boundary made one finite CrouchWalk physical-authority value fade the entire Assisted composition to exact Raw action/plant identity. The Environment maintains the mass-weighted center-of-mass motion and aggregate physical center of pressure independently of optional telemetry; invalid or unloaded feedback encodes exact zero. Every policy-input call supplies its explicit guidance: non-target Raw scopes retain exact phase scrubbing, non-target Assisted scopes retain phase, and retained integration uses the artifact's recorded guidance rather than ambient Environment state. Self-imitation and retained-best anchoring remain disabled for every update containing Raw actor samples, and a Raw provenance receipt is available only after the whole cohort is Raw. None of `0x0007'5109`, `0x0007'5110`, `0x0007'5111`, `0x0007'5112`, or `0x0007'5113` can resume under `5115`; all may enter only the explicit unverified-transfer path. `EPPO51` validates parameter/provenance/content digests, every logical metric and counter, persisted execution context, canonical flags, and exact EOF. These unkeyed digests detect accidental corruption and internal mismatch, but do not authenticate a malicious producer able to edit the binary or source; that stronger claim requires protected signing keys or external attestation. The Human owns 19 canonical nodes, 12 powered anatomy actions (explicit left/right ankles and distal toes), and 84 observations. The appended eight-value tail exposes two toe angles, two toe rates, and four finite coordinator values; the three equipment slots remain a separate stage- and capability-masked action tail.

## Release packages

A complete release contains:

- `Epoch2DWalkEngine-v0.7.51-runner-windows-x64.zip`;
- `Epoch2DWalkEngine-v0.7.51-runner-linux-x86_64.tar.gz`;
- `Epoch2DWalkEngine-v0.7.51-dev-windows-x64.zip` and `Epoch2DWalkEngine-v0.7.51-dev-linux-x86_64.tar.gz`, with the static library, public headers, exported CMake config, and exact EpochSimEngine dependency package;
- `Epoch2DWalkEngine-v0.7.51-source.zip`, produced from the exact release commit;
- SHA-256 sidecars and complete per-file manifests for every archive.

Each archive checksum is UTF-8 without a byte-order mark and contains exactly one LF-terminated `lowercase-sha256  exact-archive-filename` record. Carriage returns, a missing filename, or trailing text fail the release audit. The five archive payloads and their five checksum and five per-file-manifest siblings form the 15-object release set.

Both runtime examples must be independently extracted and launched outside the build tree. Both development packages must configure, compile, link, and run a downstream `find_package` consumer. A source archive is never presented as a runnable release.

The v0.7.51 workflow must run `--diagnose-raw-inference` separately from `--diagnose-walk-eye`. The former proves that the immutable post-handoff retained network supplies the powered joint commands without scripted gait replacement, root drive, projection, or course motion and reports joint/contact/slip evidence; the latter remains the physical replay and packaged visual gate. Neither archive nor publication is accepted until both pass with the complete release matrix.
