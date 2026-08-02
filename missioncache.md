# Runner mission cache

This is the authoritative release ledger. A mission is VERIFIED only when implementation, deterministic acceptance, cross-platform validation, packaged-runtime behavior, and release evidence agree. Contradictory runtime evidence reopens the mission.

## Release target

**Target:** Runner v0.7.4

**Release state:** PUBLISHED — v0.7.4 assets independently audited; awaiting Adam's live packaged-runtime confirmation

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
**Status:** PACKAGE VERIFIED

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
**Status:** PACKAGE VERIFIED — AWAITING LIVE CONFIRMATION

Automated metrics cannot overrule visible failures. Fused or detached feet, body collapse, arm-first movement, uncontrolled heads or tails, clipped hazards, unavoidable obstacles, incorrect preset anatomy, stale PIP frames, unreadable UI, or a controller repeatedly exploiting one body axis reopen the matching mission and block release closure.

## v0.7.3 live-runtime correction

### WALK-REG-029 — Reopen live simulation quality after v0.7.2 screenshots
**Status:** PACKAGE VERIFIED — AWAITING LIVE CONFIRMATION

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
**Status:** VERIFIED

The humanoid retains forty non-overlapping observation channels covering eight angles, eight velocities, contacts, foot placement, terrain, obstacle, stage, and phase state.

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
**Status:** IN PROGRESS

Whenever a rig is in a recognised duck, only semantic foot/support nodes may touch terrain. Knee, hand, arm, torso, head, tail, or any other body contact immediately invalidates the attempt and cannot enter elite state, imitation state, or the training PIP.

### WALK-DUCK-068 — Replace static folding with crouch walking
**Status:** IN PROGRESS

The compression platen remains only the introductory lesson. Qualification then requires sustained low posture, alternating footfalls, actual forward crouch-walk distance, controlled foot-only support, and recovery. Ten thousand updates spent folding in place are not progress.

### WALK-TERRAIN-069 — Crouch obstacle avoidance on unstable ground
**Status:** IN PROGRESS

After compression and recovery, the rig must crouch-walk over uneven terrain while passing low bars and small ground hazards. Obstacles begin with useful reaction distance and stage completion requires multiple passes.

### WALK-PIP-070 — Show the real full crouch-walk attempt
**Status:** IN PROGRESS

The training PIP publishes a current intact training environment every completed update, never goes blank merely because the attempt is failing, keeps the complete rig large, shows uneven terrain and nearby obstacles, labels farther obstacles without zooming the rig into a dot, and overlays the exact foot-contact or integrity failure while displaying update, crouch time, distance, alternating steps, and passes.

### WALK-CHECKPOINT-071 — Invalidate the failed 10,000-update duck policy
**Status:** IN PROGRESS

The v0.7.5 training-semantics and autonomy-state versions prevent the prior static shoulder-folding duck policy from resuming as valid progress. New autosave paths start the corrected lesson cleanly.

### WALK-CHICKEN-072 — Preserve the working chicken
**Status:** IN PROGRESS

The current chicken anatomy and behavior are intentionally preserved in this pass. Crouch-walk and PIP changes must not regress the chicken preset.
