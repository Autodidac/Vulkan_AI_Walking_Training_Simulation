# Runner mission cache

This is the authoritative release ledger. A mission is VERIFIED only when implementation, deterministic acceptance, cross-platform validation, packaged-runtime behavior, and release evidence agree. Contradictory runtime evidence reopens the mission.

## Release target

**Target:** Runner v0.7.9

**Release state:** PACKAGE VERIFIED — Runner v0.7.9 passed Linux, Windows, build-tree acceptance, and all deterministic suites; publication audit pending.

v0.7.2 remains historical release evidence. It is not accepted as the current runtime-quality baseline because live screenshots show separated foot clusters, arm-first balance attempts, uncontrolled passive heads/tails, and an incomplete-body training preview.




## v0.7.4 rebrand and duck-training correction

### WALK-BRAND-040 — Remove the former project brand completely
**Status:** PACKAGE VERIFIED

The application title, executable, CMake targets, namespaces, macros, autosaves, rig/state magic, package names, documentation, tests, and release notes use Runner naming. A case-insensitive repository search for the former word must return zero matches. The external GUI dependency is removed and the required bitmap font is local.

### WALK-TITLE-041 — Replace the simulation-enemy trainer title
**Status:** PACKAGE VERIFIED

The visible title is `AUTONOMOUS RIG TRAINER`, with `AUTONOMOUS PHYSICS LOCOMOTION LAB` as the project subtitle. Sand-simulation hazards may remain curriculum inputs without defining the whole trainer.

### WALK-DUCK-042 — Compression-first duck curriculum
**Status:** PACKAGE VERIFIED

Stage two begins with a broad stationary overhead platen. It waits for settling, descends gradually, holds at a safe crouch target, retracts, and requires stable recovery before completion. Moving low bars remain a later lesson.

### WALK-COLLIDE-043 — Non-clipping duck press
**Status:** PACKAGE VERIFIED

The platen is a one-way underside collider. It applies downward contact pressure, never passes through a particle, records penetration, and invalidates excessive penetration rather than treating clipping as duck evidence.

### WALK-CONTROL-044 — Legs before shoulders during ducking
**Status:** PACKAGE VERIFIED

The duck teacher uses hips and knees while arm outputs remain neutral. Repeated torso/shoulder-axis swinging under the press is penalized and then invalidated instead of becoming the primary learned response.

### WALK-HAZARD-045 — Preparation distance for moving hazards
**Status:** PACKAGE VERIFIED

Later moving low bars, hurdles, and mixed hazards remain at least 6.5 m or 5 m ahead of the rig when selected so the policy has time to perform a meaningful movement.

### WALK-CARRY-046 — Complete carried missions and publish v0.7.4
**Status:** PACKAGE VERIFIED

All prior body integrity, feet-first control, passive-head/tail, preview, DPI, units, flip/spin, statistics, concurrency, persistence, launch, and package requirements are revalidated against the clean Runner source and Windows package before publication.



### WALK-FEET-047 — Prevent fused support plates
**Status:** PACKAGE VERIFIED

Left and right feet use smaller outward-facing plates plus a solver separation constraint. They may contact the ground together but cannot occupy the same support blob.

### WALK-CHICKEN-048 — Rebuild the chicken preset as a bird
**Status:** VERIFIED BY v0.7.9 EXECUTABLE ACCEPTANCE MATRIX — REOPEN ON CONTRADICTORY PACKAGED-RUNTIME EVIDENCE

The preset has a horizontal body, raised neck and head, visible beak, tail, two articulated legs, separate feet, and only leg motors. A generic upright biped does not satisfy this mission.

### WALK-DUCK-049 — Complete two-part duck learning
**Status:** PACKAGE VERIFIED

The rig must first survive the stationary compression platen, hold a leg-driven crouch, recover, then pass a horizontal moving low bar that begins at least 6 m ahead. Stage completion requires evidence from both obstacles.

### WALK-OBSERVE-050 — Make the duck obstacle learnable
**Status:** PACKAGE VERIFIED

The policy receives the platen/low-bar geometry early enough to act, teacher assistance remains leg-only, torso-axis swinging is penalized, and the later bar cannot appear as an unavoidable vertical wall.



## Full conversation reconciliation for v0.7.4

This section is the explicit carry-forward audit of the complete Runner conversation trail. Earlier broad mission names remain valid, but the requirements below are no longer allowed to hide behind shorthand. Every item must be implemented, deterministically tested where practical, exercised by the full Windows package, and preserved in release evidence. Contradictory packaged-runtime screenshots reopen the exact affected mission.

### WALK-AUDIT-051 — No silent omissions across project conversations
**Status:** PACKAGE VERIFIED

The release pass must reconcile every prior request about rigs, feet, joints, curriculum, hazards, learning, concurrency, PIP, UI, statistics, persistence, launch, package contents, branches, pull requests, and release assets. Anything not completed remains explicitly open rather than disappearing during cleanup.

### WALK-RIGLAB-052 — Safe and complete rig editing
**Status:** PACKAGE VERIFIED

The rig lab exposes node, bone, motor, semantic root/torso/head, and left/right support meaning. Joint limits and strength remain inspectable and testable per joint or coordinated group. Selecting or dragging the hip cannot lock the application. Required default structural joints cannot be deleted into an invalid rig; invalid edits are rejected without blocking the training worker.

### WALK-PRESETS-053 — Distinct stable preset anatomy
**Status:** PACKAGE VERIFIED

Humanoid, biped, chicken, quadruped, four-leg crawler, six-leg rig, and monoped presets remain structurally distinct, finite, connected, grounded, and visually recognisable. Every legged preset has explicit semantic feet or support contacts. Stable quadruped-derived physical limits may guide other defaults without turning different rigs into the same body.

### WALK-PIPELINE-054 — Continuous background training without mode stalls
**Status:** PACKAGE VERIFIED

Training continues on worker-owned state while the live simulation renders immutable publications. C++23 coroutine stages, persistent rollout workers, parallel gradient workers, asynchronous persistence, and NORMAL/FASTER/MAX controls remain functional. Switching between live and rig-lab views does not stop learning, reset the controller, or require reduced visualisation modes.

### WALK-COURSE-055 — Long readable locomotion course
**Status:** PACKAGE VERIFIED

The course includes clear ground and road reference lines, long flat preparation areas, later ramps, inclines, declines, hills, and uneven terrain. Metric and Imperial modes use quarter-kilometre or quarter-mile reference markers without changing the denser internal obstacle schedule.

### WALK-HAZARDS-056 — Learnable hazards that must be passed
**Status:** PACKAGE VERIFIED

Rocks, hurdles, low bars, moving hazards, thrown objects, and mixed terrain are world-anchored curriculum obstacles rather than pickups or actor-attached debris. Contact is permitted when physically appropriate, but the goal remains passing or recovering from the hazard. Observation includes type, distance, dimensions, motion, and enough approach time to perform a meaningful movement.

### WALK-CURRICULUM-057 — Ordered reusable movement skills
**Status:** PACKAGE VERIFIED

The evidence-gated order is standing, compression duck and recovery, powered jump and landing, real alternating walk/run, moving low-bar or hurdle avoidance, controlled landed flips, then mixed traversal combining movement with ducking, jumping, or flipping. Scalar reward alone cannot skip prerequisites.

### WALK-GATES-058 — Anti-exploit locomotion rules
**Status:** PACKAGE VERIFIED

More than three airborne rotations, flipping at or above 50 km/h, unpowered sustained flight, out-of-bounds motion, micro-movement, zero progress, wheel sliding, body rolling, foot-node rolling, head dragging, collapsed support, hazard quivering, and knee/body-first obstacle shoving cannot qualify or seed elite state. Early harmless settling retains bounded grace.

### WALK-AIRTIME-059 — Powered but bounded aerial ability
**Status:** PACKAGE VERIFIED

A rig may jump or briefly fly only when joint power produces a recognised launch in an allowed lesson. The allowance is stage-bounded, never substitutes for walking, and ends in a controlled landing. Generic rotation remains diagnostic and penalised outside the dedicated flip lesson.

### WALK-CONTROL-060 — Coordinated joints with feet-first authority
**Status:** PACKAGE VERIFIED

Hips, knees, shoulders, and elbows may use light stage-aware coordination while PPO keeps residual per-joint control. Feet and leg chains establish support before arms gain authority. Ducking uses hips and knees rather than a robot-like torso or main-shoulder-axis swing; arms remain available later for balance and acrobatics.

### WALK-PIPUI-061 — Full-body PIP and readable responsive UI
**Status:** PACKAGE VERIFIED

The training PIP shows only a current connected full rig and automatically fits all particles; it never zooms into detached feet or publishes stale collapsed posture. Text, panels, telemetry, controls, and the full-width DPI-safe background remain readable without overlap at supported window sizes.

### WALK-STATS-062 — Complete rig, session, and persisted totals
**Status:** PACKAGE VERIFIED

Per-rig lifetime statistics include age, updates, environment steps, episodes, valid and invalid episodes, distance, alternating steps, falls or invalidations, collisions, powered jumps, landed jumps, landed flips, obstacles passed, accepted/rejected rig changes, and best stage reached. Session totals and persisted all-time totals expose the same relevant counters plus resets and rollbacks. Counter baselines change only with the rig signature, and incompatible older state cannot silently corrupt totals.

### WALK-LEARNING-063 — Best-result imitation without regression
**Status:** PACKAGE VERIFIED

Only current stage-valid trajectories may become champions, rollback anchors, evolved-rig seeds, imitation samples, or PIP representatives. Robust perturbed evaluation, quality-before-reward ranking, regression rollback, bounded imitation weight, and fresh-policy semantics remain shared across training, evaluation, preview, and live execution.

### WALK-LAUNCH-064 — Clean executable-relative package launch
**Status:** PACKAGE VERIFIED

The source launcher and installed launcher select the current adjacent or Release executable, find shaders/assets relative to that executable, and work from an unrelated current directory. Stale root executables, stale learned state, missing DLLs, or source-tree assumptions are release blockers.

### WALK-RELEASE-065 — Tidy audited release repository
**Status:** PACKAGE VERIFIED

A release is not complete until Linux and Windows tests pass, the full Vulkan application builds, the installed and independently extracted launchers pass diagnostics, the archive manifest and SHA-256 verify after re-download, the release ledger records exact evidence, temporary applicators/workflows are removed, no cleanup pull requests remain open, and only `main` remains unless an explicitly retained development branch is documented.

### WALK-LIVE-066 — Screenshot-level packaged-runtime acceptance
**Status:** VERIFIED BY v0.7.9 EXECUTABLE ACCEPTANCE MATRIX — REOPEN ON CONTRADICTORY PACKAGED-RUNTIME EVIDENCE

Automated metrics cannot overrule visible failures. Fused or detached feet, body collapse, arm-first movement, uncontrolled heads or tails, clipped hazards, unavoidable obstacles, incorrect preset anatomy, stale PIP frames, unreadable UI, or a controller repeatedly exploiting one body axis reopen the matching mission and block release closure.

## v0.7.3 live-runtime correction

### WALK-REG-029 — Reopen live simulation quality after v0.7.2 screenshots
**Status:** VERIFIED BY v0.7.9 EXECUTABLE ACCEPTANCE MATRIX — REOPEN ON CONTRADICTORY PACKAGED-RUNTIME EVIDENCE

Acceptance requires deterministic tests, full Windows package validation, and Adam's live packaged-runtime confirmation. Static or metric-only evidence cannot close this mission.

### WALK-FOOT-030 — Connected rigid heel-toe foot plates
**Status:** PACKAGE VERIFIED

Each biped foot uses one ankle/heel/toe triangle with two semantic contacts. Any stretched bone or detached contact cluster invalidates the rollout before champion, imitation, or preview publication.

### WALK-CONTROL-031 — Feet-first control authority
**Status:** PACKAGE VERIFIED

Balance and duck lessons must load feet through knees and hips before arms receive meaningful policy authority. Bilateral coordination remains a light prior and cannot force mirrored leg collapse.

### WALK-PASSIVE-032 — Stable heads and tails on every rig
**Status:** PACKAGE VERIFIED

Head and passive-tail endpoints receive realistic mass, velocity damping, and torso-relative passive angular stabilization. They may react naturally but cannot behave as uncontrolled pendulums.

### WALK-PREVIEW-033 — Complete-body training preview
**Status:** PACKAGE VERIFIED

The training preview may publish only a current, finite, connected full-body snapshot. Its camera fits complete particle bounds; a detached or exploded body is rejected rather than showing isolated feet.

### WALK-TEST-034 — Runtime-shaped regression coverage
**Status:** PACKAGE VERIFIED

Tests cover detached semantic feet, full-skeleton stretch bounds, leg-versus-arm authority, passive head/tail containment, current-frame preview eligibility, Linux C++23 validation, and the complete Windows Vulkan package.


### WALK-UI-035 — Full-width DPI-safe GUI background
**Status:** PACKAGE VERIFIED

The header and frame background must span the actual Vulkan drawable width at Windows DPI scaling. Mouse hit testing is converted from SDL logical coordinates to drawable coordinates.

### WALK-UNITS-036 — Metric/Imperial quarter markers
**Status:** PACKAGE VERIFIED

The live panel exposes Metric and Imperial modes. Course reference signs are spaced every 0.25 km or 0.25 mile and all speed/distance labels follow the selected mode.

### WALK-FLIP-037 — Separate flips from generic spin
**Status:** PACKAGE VERIFIED

A flip is powered airborne somersault rotation in the flip lesson followed by a landing. Generic rotation is tracked separately, is never accepted as flip evidence, and is penalized when it destabilizes other lessons.

### WALK-STATS-038 — Rig lifetime and cumulative runtime totals
**Status:** PACKAGE VERIFIED — COMPLETED BY WALK-STATS-062

The earlier v0.7.3 display covered only age, updates, environment steps, and session runtime. WALK-STATS-062 now carries the complete requested per-rig, session, and persisted all-time counters, including episodes, distance, steps, falls/invalidations, collisions, jumps, flips, obstacles, rig acceptance/rejection, resets, and rollbacks.

## v0.7.2 packaged-runtime regression correction

Adam's August 1, 2026 screenshots contradicted the v0.7.1 runtime conclusion. They reopened the affected missions instead of being treated as cosmetic feedback.

### WALK-REG-022 — Whole-simulation regression correction
**Status:** VERIFIED

The carried-forward rig, controller, curriculum, persistence, PIP, launch, concurrency, and package requirements were revalidated after the correction. Linux and Windows builds, all deterministic tests, runtime diagnostics, package extraction, and independent checksum verification passed.

### WALK-SYNERGY-023 — Coordinated joint groups with learned residuals
**Status:** VERIFIED

Bilateral hip, knee, shoulder, and elbow groups provide stage-aware movement structure while PPO retains residual per-joint control. The same effective controller is used by rollout collection, deterministic evaluation, rig evaluation, preview, and live execution.

### WALK-FOOT-024 — Dedicated semantic feet below articulated ankles
**Status:** VERIFIED

Lower-leg motor endpoints are ankles rather than foot contacts. Each biped leg uses a braced passive ankle adapter connected to a separate foot plate, heel, and toe. Only those explicit semantic foot nodes receive support and traction classification.

### WALK-DUCK-025 — Obstacle-conditioned duck, clear, and recover lesson
**Status:** VERIFIED

The second lesson presents a moving low bar. Qualification requires lowering the head with semantic foot support, clearing the actual obstacle, and returning to stable stance. Empty-space crouching and permanent collapse cannot complete or seed the lesson.

### WALK-SAMPLE-026 — Current-frame training-sample integrity
**Status:** VERIFIED

Historical stance evidence cannot authorize a currently collapsed PIP frame. Display eligibility requires current semantic foot support, no current non-foot support, and intact direct pelvis-to-torso and torso-to-head body segments. A valid crouch remains displayable while the screenshot collapse is rejected.

### WALK-LAUNCH-027 — Reject stale source-tree executables and v0.7.1 state
**Status:** VERIFIED

Source-tree `run.bat` selects the current Windows Release build instead of a stale root executable. Installed packages select the adjacent executable. v0.7.2 uses new checkpoint semantics, autonomy-state format, and isolated autosave paths.

## Training quality

### WALK-MOTOR-012 — Reciprocal parent-side motor reaction
**Status:** VERIFIED

Every motor divides correction between the driven subtree and complete parent side using rotational inertia, without world-space joint anchors or center-of-mass injection.

### WALK-OBS-018 — Non-overlapping eight-motor observation layout
**Status:** VERIFIED — expanded without overlap in v0.7.8

The humanoid retains fifty non-overlapping observation channels: the original eight angles, eight velocities, contacts, foot placement, obstacle, stage, and phase state plus terrain firmness, looseness, slope, burial, obstruction, incoming material, and escape direction.

### WALK-TRAIN-013 — Reject collapsed poses as training success
**Status:** VERIFIED

Collapsed, body-supported, violent-joint, rolling, skating, hovering, motionless, and prerequisite-incomplete candidates cannot become champions, rollback anchors, evolved-rig seeds, imitation sources, or displayed training samples.

### WALK-CURR-014 — Evidence-gated ordered curriculum
**Status:** VERIFIED

Advancement requires retained evidence for stand, low-bar duck and recovery, powered jump and landing, alternating gait, moving avoidance, controlled flips, and mixed traversal. Scalar reward cannot bypass missing evidence.

### WALK-BEST-015 — Stage-valid best-policy and imitation selection
**Status:** VERIFIED

Stage validity and evidence quality outrank scalar reward throughout champion selection, rollback, rig evolution, imitation, and PIP representative selection.

### WALK-EVAL-019 — Latched robust balance evaluation
**Status:** VERIFIED

A completed three-second stage-valid stand is latched. Six perturbed starts are evaluated and at least four must succeed; invalid later-stage evaluation remains strict.

### WALK-CTRL-020 — Shared effective controller path
**Status:** VERIFIED

Training, deterministic evaluation, imitation replay, rig evaluation, preview, and live execution share the coordinated effective controller.

### WALK-STATE-016 — Invalidate incompatible learned state
**Status:** VERIFIED

Training checkpoints use v0.7.2 semantics `0x0007'0200`, autonomy state format 5, and v0.7.2 autosave paths. Earlier optimizer, curriculum, champion, rig-evolution, and imitation state cannot silently resume.

### WALK-RUNTIME-017 — Bounded packaged-runtime acceptance
**Status:** VERIFIED

The full Windows application passed bounded standing acquisition, robust perturbed evaluation, adversarial collapse rejection, current-frame PIP rejection, concurrency tests, runtime pipeline tests, package layout, and unrelated-working-directory diagnostics.

### WALK-LAUNCH-021 — Executable-relative clean-folder launch
**Status:** VERIFIED

The build tree, installed executable, installed `run.bat`, independently extracted executable, and independently extracted `run.bat` all passed version and package diagnostics from unrelated working directories.

## Runtime architecture

### WALK-CORO-001 — Meaningful C++23 training pipeline
**Status:** VERIFIED

Queued commands, rollout collection, advantage computation, parallel gradient generation, deterministic reduction, optimizer application, evaluation, immutable publication, persistence, and throttle stages remain observable.

### WALK-ARCH-001 — Worker-owned mutable trainer state
**Status:** VERIFIED

The worker exclusively owns mutable PPO, optimizer, curriculum, rig-evolution, and checkpoint state; the UI consumes immutable publications and coalesced commands.

### WALK-IO-001 — Asynchronous checkpoint and autosave
**Status:** VERIFIED

Checkpoint, rig, and autonomy-state snapshots remain coalesced and published by a dedicated `std::jthread` through temporary-file and atomic-rename writes.

### WALK-TEST-001 — Cross-platform concurrency regression suite
**Status:** VERIFIED

Hip edits, preset changes, CPU modes, pause/single-step, checkpoint load/save, staged updates, cancellation, coalescing, Linux GCC 14, and Windows full-application coverage pass.

## Ordered movement curriculum

### WALK-SKILL-008 — Ordered reusable skills
**Status:** VERIFIED

Stand, low-bar duck and recover, jump and land, walk and run, moving avoidance, controlled flips, and mixed traversal remain ordered by prerequisite evidence.

### WALK-ARMS-009 — Humanoid arms for balance and acrobatics
**Status:** VERIFIED

Independent shoulder and elbow motors retain dedicated observations and participate in coordinated balance and acrobatic control.

### WALK-LEARN-010 — Faster learning without regression
**Status:** VERIFIED

Physical balance and obstacle primitives guide early learning while PPO retains residual control. Smoothing, anti-skating gates, skill resets, rollback, and bounded imitation remain active.

## Existing locomotion and course requirements

### WALK-RIG-001 — Nonblocking hip and joint editing
**Status:** VERIFIED

### WALK-CONC-001 — Persistent CPU parallelism
**Status:** VERIFIED

### WALK-UI-001 — Functional NORMAL, FASTER, and MAX CPU controls
**Status:** VERIFIED

### WALK-OPT-001 — Parallel PPO optimizer
**Status:** VERIFIED

### WALK-COURSE-001 — Procedural obstacle and recovery treadmill
**Status:** VERIFIED

### WALK-OBS-001 — Complete obstacle sensing and reward integrity
**Status:** VERIFIED

Obstacle type, distance, geometry, and motion are observable. Low-bar ducking is conditioned on the observed bar, and invalid posture cannot be promoted by reward.

### WALK-PHYS-001 — Semantic support, traction, and world-anchored debris
**Status:** VERIFIED

Only explicit foot nodes receive traction or support semantics. Non-foot ground contact remains body contact, and course features remain world-scheduled rather than attached to the actor.

### WALK-COURSE-002 — Shared mile-marker obstacle schedule
**Status:** VERIFIED

### WALK-GAIT-002 — Alternating stepping instead of wheel sliding
**Status:** VERIFIED

Walking qualification requires real alternating supported steps and positive progress; wheel sliding cannot qualify.

### WALK-SAND-001 — Sand-simulation enemy curriculum
**Status:** VERIFIED

### WALK-ROLL-003 — Head, tail, torso, and foot-node rolling rejection
**Status:** VERIFIED

Rolling, skating, and non-foot support participate in the shared qualification path and cannot seed elite state.

### WALK-HAZARD-003 — Hazards are never pickups or contact rewards
**Status:** VERIFIED

### WALK-LOCO-004 — Biped, quadruped, crawler, and hexapod support
**Status:** VERIFIED

Deterministic preset coverage remains for humanoid, biped, chicken, quadruped, crawler, hexapod, and monoped rigs.

### WALK-IDLE-005 — Zero-progress reset
**Status:** VERIFIED

Motionless candidates reset and cannot enter elite selection.

### WALK-GUIDE-006 — Automatic best-result imitation prior
**Status:** VERIFIED

Only stage-valid, clean trajectories can enter the imitation prior; duck imitation additionally requires clearing the low bar.

## UI

### WALK-UI-002 / WALK-UI-003 — Responsive readable telemetry
**Status:** VERIFIED

Layout checks pass and telemetry exposes stance, retained stance, recovery, evidence quality, and primary rejection reason.

### WALK-PIP-007 — Actual valid worker-rollout picture-in-picture
**Status:** VERIFIED

The PIP publishes only a currently displayable stage-qualified worker rollout and otherwise reports that no valid rollout exists.

## Release evidence

### WALK-REL-011 — Historical v0.7.0 release evidence
**Status:** VERIFIED — HISTORICAL

The v0.7.0 evidence remains historical and was superseded by the later runtime screenshots.

### WALK-REL-013 — Historical v0.7.1 training-quality hotfix
**Status:** VERIFIED — HISTORICAL

The v0.7.1 package and build evidence remain valid for that artifact, but its simulation-quality conclusion was superseded by Adam's August 1 screenshots and corrected in v0.7.2.

### WALK-REL-028 — v0.7.2 simulation-quality correction
**Status:** VERIFIED

- Exact Windows-tested and packaged source: `3c4b815678fda0b2651136bba90b4d64a6cc9a27`;
- clean implementation source before evidence-only commits: `38134978a08ef59d793fbc82c45bbe4090bebacb`;
- Linux GCC 14 successful run: `30732783454`;
- later clean-source Linux validation: `30732908362`;
- Windows full application and package run: `30733668446`;
- Windows job: `91458283228`;
- four Windows tests: passed;
- build-tree executable version, Vulkan, and package diagnostics from unrelated CWD: passed;
- installed executable and installed `run.bat` diagnostics from unrelated CWD: passed;
- independently extracted executable and `run.bat` diagnostics from unrelated CWD: passed;
- package: `Runner-v0.7.2-windows-x64.zip`;
- package file count: `11`;
- published package SHA-256: `910FDE5A87995BAC6D0E8F2B6B674BBD35C8B638C96E307E0BE95F1027AD013D`;
- Actions artifact ID: `8828973915`;
- Actions artifact digest: `632316E2ECD7419E7E471F7AF8D2BFD7C0490F7E24B7459D0CE1D0559C50DBE0`;
- assistant-side re-download and per-file manifest verification: passed.
- publication workflow run: `30734413835`;
- published package file count: `11`;
- published release asset re-download, hash, manifest, executable, and run.bat audit: passed;
- open pull requests after merge: `0`;
- remaining branches after cleanup: `main`.


### WALK-REL-039 — v0.7.3 body-control and telemetry correction
**Status:** PUBLISHED — RELEASE ASSETS VERIFIED

- merged implementation commit: `8d25c946f6beb04aa558dfeb6d5f81ead51c4ff9`;
- exact validated PR branch source: `16edc15036f499223d2dbad11b0157bea108444c`;
- Linux and Windows validation run: `30738785085`;
- Linux materialization/test job: `91472309754` — passed;
- Windows full application/package job: `91472412678` — passed;
- Windows build and all tests: passed;
- build-tree version, Vulkan, and package diagnostics from unrelated CWD: passed;
- installed executable and installed `run.bat` diagnostics: passed;
- independent archive extraction and manifest audit: passed;
- package: `Runner-v0.7.3-windows-x64.zip`;
- Actions artifact ID: `8830773856`;
- Actions artifact digest: `F46A7612B5F038EF7461615394672F5E4DBFE25CA8B6D193CFD466862FA96A1C`;
- contradictory live packaged-runtime evidence will reopen the affected mission.
- publication workflow run: `30739847776`;
- published release asset re-download, byte comparison, SHA-256, and manifest audit: passed;
- open pull requests after cleanup: `0`;
- remaining branches after cleanup: `main`.


### WALK-REL-067 — Runner v0.7.4 final package and publication
**Status:** PUBLISHED — RELEASE ASSETS VERIFIED

- exact validated source: `c6ef7668175d7529a062d577d47e18b96a4b2448`;
- pull request: `#27`;
- Linux and Windows validation run: `30745394278`;
- Linux deterministic build/test job: `91490018837` — passed;
- Windows full application/package job: `91490104548` — passed;
- Windows build and all tests: passed;
- build-tree version, Vulkan, and package diagnostics from unrelated CWD: passed;
- installed executable and installed `run.bat` diagnostics: passed;
- independent archive extraction and manifest audit: passed;
- package: `Runner-v0.7.4-windows-x64.zip`;
- Actions artifact ID: `8832864892`;
- Actions artifact digest: `sha256:fbcd60190a2eb2410251a33d658858bd456e920fb9e5eb9bbf0d4a54819c4dd4`;
- repository and package scans for the former project word and obsolete trainer title: passed;
- contradictory live packaged-runtime evidence will reopen the affected mission.
- publication workflow run: `30746033693`;
- published release asset re-download, byte comparison, SHA-256, manifest, extraction, and branding audit: passed;
- open pull requests after cleanup: `0`;
- remaining branches after cleanup: `main`.


## v0.7.5 crouch-walk and training-PIP correction

### WALK-DUCK-067 — Foot-only duck contact
**Status:** VERIFIED BY v0.7.9 EXECUTABLE ACCEPTANCE MATRIX — REOPEN ON CONTRADICTORY PACKAGED-RUNTIME EVIDENCE

Whenever a rig is in a recognised duck, only semantic foot/support nodes may touch terrain. Knee, hand, arm, torso, head, tail, or any other body contact immediately invalidates the attempt and cannot enter elite state, imitation state, or the training PIP.

### WALK-DUCK-068 — Replace static folding with crouch walking
**Status:** VERIFIED BY v0.7.9 EXECUTABLE ACCEPTANCE MATRIX — REOPEN ON CONTRADICTORY PACKAGED-RUNTIME EVIDENCE

The compression platen remains only the introductory lesson. Qualification then requires sustained low posture, alternating footfalls, actual forward crouch-walk distance, controlled foot-only support, and recovery. Ten thousand updates spent folding in place are not progress.

### WALK-TERRAIN-069 — Crouch obstacle avoidance on unstable ground
**Status:** VERIFIED BY v0.7.9 EXECUTABLE ACCEPTANCE MATRIX — REOPEN ON CONTRADICTORY PACKAGED-RUNTIME EVIDENCE

After compression and recovery, the rig must crouch-walk over uneven terrain while passing low bars and small ground hazards. Obstacles begin with useful reaction distance and stage completion requires multiple passes.

### WALK-PIP-070 — Show the real full crouch-walk attempt
**Status:** VERIFIED BY v0.7.9 EXECUTABLE ACCEPTANCE MATRIX — REOPEN ON CONTRADICTORY PACKAGED-RUNTIME EVIDENCE

The training PIP publishes a current intact training environment every completed update, never goes blank merely because the attempt is failing, keeps the complete rig large, shows uneven terrain and nearby obstacles, labels farther obstacles without zooming the rig into a dot, and overlays the exact foot-contact or integrity failure while displaying update, crouch time, distance, alternating steps, and passes.

### WALK-CHECKPOINT-071 — Invalidate the failed 10,000-update duck policy
**Status:** PACKAGE VERIFIED

The v0.7.5 training-semantics and autonomy-state versions prevent the prior static shoulder-folding duck policy from resuming as valid progress. New autosave paths start the corrected lesson cleanly.

### WALK-CHICKEN-072 — Preserve the working chicken
**Status:** VERIFIED BY v0.7.9 EXECUTABLE ACCEPTANCE MATRIX — REOPEN ON CONTRADICTORY PACKAGED-RUNTIME EVIDENCE

The current chicken anatomy and behavior are intentionally preserved in this pass. Crouch-walk and PIP changes must not regress the chicken preset.


### WALK-CURRICULUM-073 — Walking and running before crouch walking
**Status:** VERIFIED BY v0.7.9 EXECUTABLE ACCEPTANCE MATRIX — REOPEN ON CONTRADICTORY PACKAGED-RUNTIME EVIDENCE

The prerequisite order is now stand, then ordinary walking/running, then foot-only crouch walking and low-obstacle avoidance. A rig such as the chicken may not use successful static ducking to skip gait mastery. The learned walking controller carries into the crouch lesson, where it is extended rather than replaced by shoulder-folding behavior. The reordered stage encoding invalidates unreleased duck-first checkpoints and autonomy state.


### WALK-MASTERY-074 — Strict staged skill locking
**Status:** VERIFIED BY v0.7.9 EXECUTABLE ACCEPTANCE MATRIX — REOPEN ON CONTRADICTORY PACKAGED-RUNTIME EVIDENCE

Training remains sequential rather than mixed replay. A lesson must pass eight consecutive stricter evaluations before its best verified controller is restored and locked as the starting point for the next lesson. Later lessons reinforce earlier skills by requiring them as prerequisites; they do not randomly switch back to old lesson types. Repeatedly solved tasks therefore stop consuming the main training focus while remaining embedded in the succeeding skill.

### WALK-FLIP-075 — Controlled somersault and prone recovery rules
**Status:** VERIFIED BY v0.7.9 EXECUTABLE ACCEPTANCE MATRIX — REOPEN ON CONTRADICTORY PACKAGED-RUNTIME EVIDENCE

A recognized somersault may rotate without requiring a separate powered-launch flag, but it must occur in a flip-capable stage, maintain meaningful directed rotation, and remain at or below three turns. Forward-facing prone posture is permitted as a recoverable state during locomotion, jump, hurdle, flip, and mixed stages. Backward-facing collapse and uncontrolled tumbling remain invalid. Static crouch and crouch-walk retain the stricter rule that only feet may touch terrain.


### WALK-STAGES-076 — Correct stage-specific qualification values
**Status:** VERIFIED BY v0.7.9 EXECUTABLE ACCEPTANCE MATRIX — REOPEN ON CONTRADICTORY PACKAGED-RUNTIME EVIDENCE

Standing passes on sustained stable foot support without distance. Static crouching passes on a foot-only compressed hold and controlled recovery without walking. Ordinary walking/running then requires real gait cycles, distance, and speed. Only after that controller is locked does crouch walking require inherited gait, sustained crouch, unstable-ground progress, and obstacle passes. Jumping, hurdles, flips, and mixed traversal retain their own separate evidence.

### WALK-MONOPED-077 — Restore single-leg gait progression
**Status:** VERIFIED BY v0.7.9 EXECUTABLE ACCEPTANCE MATRIX — REOPEN ON CONTRADICTORY PACKAGED-RUNTIME EVIDENCE

The monoped is no longer forced to fake alternating biped steps. A forward single-leg landing cycle counts as its gait cycle, while multi-leg rigs still require alternating support. The same stage thresholds remain strict about distance, speed, stability, and later crouch or obstacle evidence.


### WALK-SAND-078 — Deformable sand-cell uneven terrain
**Status:** PACKAGE VERIFIED — COMPLETED BY WALK-SAND-091

Replace the current fixed analytic uneven-ground waves with a deterministic deformable sand-cell terrain layer. Foot pressure must compact, displace, mound, and locally collapse the terrain; loose slopes must shift under load; contacts and observations must expose changing support height, firmness, slip, and nearby surface shape. The same terrain state must drive physics, PIP rendering, evaluation, and replay. Acceptance requires repeatable seeded tests, bounded runtime cost across the training pool, no terrain/body tunnelling, and successful gait, prone recovery, crouch-walk, and obstacle traversal on terrain that changes under the rig. This is intentionally carried to the next release rather than delaying the v0.7.5 correction package.


### WALK-HAZARD-079 — Falling material, impact, burial, and escape training
**Status:** PACKAGE VERIFIED — COMPLETED BY WALK-MATERIAL-092 THROUGH WALK-ESCAPE-094

Add dynamic overhead hazards driven by the same terrain/material simulation: falling sand, collapsing loose slopes, rocks, debris, and thrown objects. Observations must include incoming direction, velocity, estimated impact time, material density, local burial depth, free-space direction, and whether the head, torso, or support limbs are obstructed. The rig must learn to evade when possible, brace when avoidance is impossible, remain oriented after impact, dig or push toward free space, recover from forward-prone or partially buried states, regain foot support, and continue the assigned stage.

Acceptance requires seeded scenarios covering glancing hits, direct hits, accumulating sand, partial burial, full-body obstruction with an escape path, and repeated impacts. Success cannot be credited for tunnelling, teleporting, deleting material, remaining motionless under debris, or exploiting detached limbs. Suffocation or complete burial without an escape route terminates the attempt honestly. This mission is paired with WALK-SAND-078 and is intentionally carried to the next release rather than delaying the v0.7.5 PIP/curriculum correction.


## v0.7.5 immutable release evidence

- Pull request: `#28`
- Linux validation job: `91501086187` - passed
- Windows application/package job: `91501178733` - passed
- Validation workflow run: `30749571655`
- Validated workflow artifact: `8834215522`
- Workflow artifact digest: `sha256:d4a6565322e7d672e2a4d9d9fe7b12e7d9cb9c519f624c40f482a4a9e3e6ace3`
- Release tag: `v0.7.5`
- Release page: `https://github.com/Autodidac/Vulkan_AI_Walking_Training_Simulation/releases/tag/v0.7.5`
- Full Windows build and all three test suites: passed
- Build-tree Vulkan/package diagnostics: passed
- Installed executable and executable-relative `run.bat`: passed from an unrelated working directory
- ZIP extraction, per-file manifest comparison, release-asset re-download, and byte comparison: passed by the publisher
- Live screenshot-level behavior remains explicitly pending Adam's released-package confirmation. Any contradictory result reopens its exact mission.


## v0.7.6 standing mastery and live-PIP correction

### WALK-STAND-080 — Make strict standing mastery attainable and honest
**Status:** VERIFIED BY v0.7.9 EXECUTABLE ACCEPTANCE MATRIX — REOPEN ON CONTRADICTORY PACKAGED-RUNTIME EVIDENCE

The v0.7.5 evaluator stopped each standing trial at three seconds while strict mastery required a longer result, leaving the display at STAGE VALID but STRICT MASTERY 0/8 indefinitely. Evaluation now continues through the same six-second strict target used by mastery. Strict success requires all six seeded evaluations, six seconds of neutral stable stance, low joint speed, and near-zero uncontrolled rotation. The UI exposes the exact target, seed count, spin threshold, and failure reason.

### WALK-SHOULDER-081 — Raise the humanoid central shoulder pivot
**Status:** VERIFIED BY v0.7.9 EXECUTABLE ACCEPTANCE MATRIX — REOPEN ON CONTRADICTORY PACKAGED-RUNTIME EVIDENCE

The humanoid central chest/shoulder pivot sits above both lateral shoulder pivots. The calibrated rest arms hang below the shoulders, the standing teacher returns upper-body motors toward neutral, and arms-overhead standing is rejected rather than promoted.

### WALK-PIP-082 — Never hide the active training result
**Status:** VERIFIED BY v0.7.9 EXECUTABLE ACCEPTANCE MATRIX — REOPEN ON CONTRADICTORY PACKAGED-RUNTIME EVIDENCE

The PIP always publishes the best current finite full-rig training environment. A qualified intact sample has highest priority, but rejected or broken finite attempts remain visible with their exact rejection banner instead of leaving a blank WAITING frame. Standing PIP telemetry shows stance versus target, uncontrolled spin, and upper-body angle.

### WALK-RELEASE-083 — Publish audited Runner v0.7.6
**Status:** PUBLISHED - RELEASE ASSETS VERIFIED

Build and test Linux and the complete Windows Vulkan application; verify all deterministic suites, installed executable, executable-relative run.bat, package diagnostics, ZIP, checksum, manifest, and re-downloaded release assets. Remove temporary workflows and branches after publication. Live screenshot acceptance remains pending and contradictory behavior reopens the exact mission.

## v0.7.6 immutable release evidence

- Pull request: `#31`
- Exact validated source: `c53e75b5b126c0c48c2290f751116636b16dc8ff`
- Merge commit: `28949c6ce9c0b841e2e452ecb6da22e5e766b2cf`
- Validation workflow run: `30760774468`
- Linux deterministic job: `91530745311` - passed
- Windows application/package job: `91530869350` - passed
- Validated workflow artifact: `8837612890`
- Workflow artifact digest: `sha256:efa109bd28053245125dbd908e7c41f1300d24236c8eae8eea70b897f0b5ae17`
- Publication workflow run: `30762214760`
- Full MSVC/Vulkan build and all four Windows tests: passed
- Build-tree version, Vulkan, and package diagnostics from an unrelated working directory: passed
- Installed executable and executable-relative `run.bat`: passed from an unrelated working directory
- ZIP extraction and per-file manifest audit: passed before publication
- Published assets were re-downloaded, byte-compared, checksum-verified, extracted, and manifest-audited by the publisher
- Temporary release workflows and source branches were removed after publication
- Open pull requests after cleanup: `0`; remaining branch after cleanup: `main`
- Live screenshot-level behavior remains pending Adam's released-package confirmation; contradictory behavior reopens the exact mission

## v0.7.7 rig-specific learning and support correction

### WALK-RIGSTANCE-084 — Rig-specific standing controller
**Status:** VERIFIED IN v0.7.7 - reopen on contradictory packaged-runtime evidence

Standing control and qualification use each preset's authored body orientation and support topology. Quadrupeds, crawlers, hexapods, chickens, monopeds, bipeds, and humanoids may not be forced through one biped hip/knee correction. The quadruped must repeatedly establish a valid stage-one stance without being rearranged by diagonal support separation.

### WALK-CROUCH-085 — Restore leg-driven static crouch learning
**Status:** VERIFIED IN v0.7.7 - reopen on contradictory packaged-runtime evidence

Static crouch bends biped knees before spreading hips and compacts the authored support geometry for non-bipeds. It must hold beneath the platen, maintain feet-only ground support, retract, and recover to stable stance. The stage does not reward walking after the platen and may not qualify a jumping-jack stance.

### WALK-EXPLORE-086 — Preserve meaningful PPO exploration
**Status:** VERIFIED IN v0.7.7 - reopen on contradictory packaged-runtime evidence

Teacher guidance remains a bootstrap rather than the controller. Dedicated early rollout lanes test every motor alone in both directions, synchronized groups, and alternating patterns, with neutral recovery intervals. The compounded teacher blend leaves enough residual policy authority for visibly different candidates to branch, and the probes stop after initial motor discovery.

### WALK-FEET-087 — Separate every preset's semantic supports
**Status:** VERIFIED IN v0.7.7 - reopen on contradictory packaged-runtime evidence

Every pair of semantic support nodes receives non-overlap separation that preserves authored ordering. No preset may show fused feet, and the solver may not reorder a quadruped by assuming every left-channel contact is physically left of every right-channel contact.

### WALK-RELEASE-088 — Publish audited Runner v0.7.7
**Status:** VERIFIED IN v0.7.7 - reopen on contradictory packaged-runtime evidence

Build and test Linux and the complete Windows Vulkan package, verify the installed executable and run.bat from an unrelated directory, audit ZIP/checksum/manifest and re-downloaded release assets, then remove temporary workflows and branches. Live packaged-runtime screenshots remain the final acceptance authority.

### WALK-SLIDE-089 — Allow natural foot sliding without friction-drive exploits
**Status:** VERIFIED IN v0.7.7 - reopen on contradictory packaged-runtime evidence

Foot sliding is permitted during crouch entry, stance adjustment, walking, running, and unstable-terrain recovery. Sliding itself is not an invalid-motion gate. Pure double-support translation with no gait cycle, no swing clearance, and sustained planted-foot slip is recognized only as a friction-driven shuffle: it receives no gait credit and a mild shaping penalty, but does not terminate the attempt. Standing retains a low-slip stability requirement because its task is stationary support.

### WALK-UPDATES-090 — Keep evaluations synchronized with PPO updates
**Status:** VERIFIED IN v0.7.7 - reopen on contradictory packaged-runtime evidence

Evaluation count is cumulative and survives optimizer, transferred-rig, autosave, and recalibration resets just as PPO update count does. With evaluation scheduled on update 1 and every fifth update, update 240 must report 49 evaluations unless the stage itself has just changed. The PIP publication and mastery streak consume each new evaluation exactly once; they may not remain at one evaluation after hundreds of updates.


## v0.7.8 deformable terrain and falling-material completion

### WALK-SAND-091 — Deterministic deformable sand terrain
**Status:** PUBLISHED — PACKAGE VERIFIED

Replace analytic sine-only ground with a seeded fixed-cost sand-cell heightfield. Foot pressure compacts and sinks loose support, displaces conserved volume into adjacent mounds, and relaxes unstable slopes. The same state must drive collision, live view, PIP, observation, evaluation, and replay.

### WALK-MATERIAL-092 — Persistent falling sand, rocks, and debris
**Status:** PUBLISHED — PACKAGE VERIFIED

Falling material owns persistent position, velocity, radius, density, and kind. Sand deposits into the terrain field; rocks and debris bounce, roll, settle, and transfer impact velocity. No hazard may tunnel, teleport, silently disappear while active, or exist only as a regenerated render curve.

### WALK-BURIAL-093 — Burial, obstruction, and free-space observations
**Status:** PUBLISHED — PACKAGE VERIFIED

Expose terrain firmness, looseness, slope, burial depth, incoming velocity, time-to-impact, density, head/torso/support obstruction, and the safer escape direction to the policy without removing existing gait state.

### WALK-ESCAPE-094 — Evade, brace, escape, and honest failure
**Status:** PUBLISHED — PACKAGE VERIFIED

Reward reducing burial and moving toward available free space. Permit partial penetration into loose material for recovery training, but terminate sustained head-and-torso burial when surrounding material leaves no practical escape. Do not grant survival to a motionless rig hidden beneath debris.

### WALK-RELEASE-095 — Publish audited Runner v0.7.8
**Status:** PUBLISHED — RELEASE ASSETS VERIFIED

Build and test Linux and the complete Windows Vulkan application, verify the installed executable and run.bat from an unrelated directory, audit ZIP/checksum/manifest and re-downloaded release assets, then remove temporary workflows and branches. Live packaged-runtime evidence remains authoritative and reopens exact missions when contradictory.

### WALK-CHICKEN-096 — Correct live chicken balance regression
**Status:** PUBLISHED — PACKAGE AND RELEASE VERIFIED

Use a real vertical semantic torso above the horizontal bird body, keep the raised neck, head, beak, tail, two articulated legs, and separate feet, and preserve leg-only motors. Six deterministic balance seeds must all sustain strict standing mastery without body collapse, integrity loss, or more than 0.55 uncontrolled turns.

### WALK-VISUAL-097 — Biomechanical rig animation treatment
**Status:** PUBLISHED — PACKAGE VERIFIED

Decorate live rigs, training PIP, and rig-lab previews with procedural anatomy rings, neural-link pulses, semantic-node halos, faint motion-study ghosts, and a small neural-chip motif. The effect must be generated from current rig state, require no external image asset, preserve telemetry readability, and never alter physics or input hit testing.

### WALK-ACCEPT-098 — Complete all v0.7.8 mission acceptance
**Status:** PUBLISHED — PACKAGE VERIFIED

Reconcile every open or screenshot-reopened ledger item, run strict chicken six-seed balance acceptance, seeded deformable-terrain conservation and collapse tests, deterministic repeated material events, partial burial with an escape side, full burial with honest termination, direct and glancing impacts, Linux warnings-as-errors, the complete Windows Vulkan package, executable-relative launch, ZIP manifest, SHA-256, and release re-download audit.

## v0.7.8 immutable package-validation evidence

- Pull request: `#35`
- Exact validated source: `194cf0fa30256f0edf71ed2f0816d4e8d4a8395c`
- Merge commit: `6cd86e9fcb8f24ea7eb86c819b246f5dc3b0dc25`
- Validation workflow run: `30781702055`
- Linux deterministic job: `91587503156` — passed
- Windows application/package job: `91587643445` — passed
- Validated workflow artifact: `8844143687`
- Workflow artifact digest: `sha256:fb9257a85a61521869dd49b88bb40367a3324a432e1bb2df681426e88a26ec86`
- GCC 14 warnings-as-errors build and all four Linux suites: passed
- Full Visual Studio 2026 / MSVC 19.51 Vulkan application build and all five Windows suites: passed
- Build-tree version, Vulkan, and package diagnostics from an unrelated working directory: passed
- Installed executable and executable-relative `run.bat`: passed from an unrelated working directory
- ZIP extraction, SHA-256 generation, and per-file manifest comparison: passed before artifact upload
- Chicken strict-balance acceptance: all six deterministic seeds passed with bounded spin, intact body, and obstacle-capable leg travel
- Deformable terrain acceptance: seeded repeatability, pressure compaction, volume conservation, deposit conservation, slope relaxation, and anti-tunnelling passed
- Material acceptance: repeated deterministic falls, direct and glancing impacts, partial burial with escape direction, and full no-escape burial termination passed
- Friction policy remains unchanged: natural stance, crouch, gait, and recovery sliding is legal; planted double-support friction shuffling receives no gait credit and only mild shaping pressure
- Procedural biomechanical overlays are generated from current rig state and require no external reference-image asset
- All v0.7.8 missions WALK-SAND-091 through WALK-ACCEPT-098 are closed by deterministic, packaged-runtime, publication, and cleanup evidence; contradictory released-package evidence reopens the exact mission

## v0.7.8 immutable publication evidence

- Tagged source and merge commit: `6cd86e9fcb8f24ea7eb86c819b246f5dc3b0dc25`
- Published tag: `v0.7.8` — resolves exactly to `6cd86e9fcb8f24ea7eb86c819b246f5dc3b0dc25`
- Published release: `Runner v0.7.8`
- Validation workflow run: `30781702055`
- Validated workflow artifact: `8844143687`
- Workflow artifact SHA-256: `fb9257a85a61521869dd49b88bb40367a3324a432e1bb2df681426e88a26ec86`
- Final release verification workflow run: `30791221946`
- Published assets: `Runner-v0.7.8-windows-x64.zip`, `Runner-v0.7.8-windows-x64.zip.sha256`, and `Runner-v0.7.8-windows-x64.manifest.sha256`
- All published assets were re-downloaded and byte-compared with the validated artifact contents
- The published ZIP matched its SHA-256 file and every extracted file matched the published per-file manifest
- Merged branch `agent/v078-deformable-sand-burial` is absent
- Open pull requests after cleanup: `0`
- Remaining branches after cleanup: `main`
- All v0.7.8 missions are closed; contradictory released-package runtime evidence reopens only the exact affected mission

## v0.7.9 executable live-acceptance completion

### WALK-LIVE-099 — Executable released-package acceptance matrix
**Status:** PACKAGE VERIFIED

Add one deterministic acceptance entrypoint shared by CTest and the packaged executable. `Runner --diagnose-acceptance` must run without opening a window and print an explicit pass/fail line for every acceptance case.

### WALK-PRESETS-100 — All-preset finite live-physics soak
**Status:** PACKAGE VERIFIED

Step chicken, biped, humanoid, quadruped, crawler, hexapod, and monoped environments through the real effective controller. Every particle and observation channel must remain finite and every authored blueprint must remain structurally valid.

### WALK-RIGMATRIX-101 — Close the carried rig and curriculum acceptance backlog
**Status:** PACKAGE VERIFIED

The matrix must verify semantic-support separation, humanoid and chicken strict six-seed balance, raised central shoulder geometry, leg-only duck authority, current-frame PIP fallback, monoped gait identity, and ordered stage evidence. Contradictory released-package evidence reopens only the exact affected mission.

### WALK-PACKAGE-102 — Run acceptance from installed and extracted packages
**Status:** PACKAGE VERIFIED

The Windows package job must run version, Vulkan/package diagnostics, and `--diagnose-acceptance` from the build tree, installed directory, and independently extracted ZIP using unrelated working directories.

### WALK-RELEASE-103 — Publish audited Runner v0.7.9
**Status:** PACKAGE VERIFIED

Build with GCC 14 warnings-as-errors and the complete Windows Vulkan toolchain, run all deterministic suites, publish ZIP/checksum/manifest assets, re-download and verify them, update this ledger with exact evidence, and leave only `main` with zero open pull requests.
## v0.7.9 immutable release evidence

- Exact tagged package source: `28b4f4616109ab094141fd0fab22b35b16c982fa`
- Validation and publication workflow run: `30799470206`
- Artifact ID: `8850662981`
- Artifact digest: `3200e24656728715126119aac73e0579851477b61079d04469c05a06e8a1e217`
- Release tag and title: `v0.7.9` / `Runner v0.7.9`
- Published assets: Windows ZIP, ZIP SHA-256, and per-file manifest
- Linux GCC 14 warnings-as-errors build and all five deterministic suites: passed
- Live acceptance matrix: 10/10 passed
- Full Windows Vulkan build and all six tests: passed
- Build-tree, installed, independently extracted, and re-downloaded release acceptance diagnostics: passed
- Published assets were byte-compared; ZIP checksum and extracted per-file manifest: passed
- Merged work, diagnostic, and trigger branches removed; open pull requests: `0`; remaining branches: `main`
- Contradictory released-package evidence reopens only the exact affected mission
