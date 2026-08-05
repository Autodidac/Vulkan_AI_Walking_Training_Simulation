# Runner cache-first engineering policy and active release plan

**Authoritative rule:** `missioncache.md` is updated before implementation. Every request, dependency, interaction, likely regression, acceptance test, packaging obligation, and unresolved uncertainty remains explicit. No source change may silently narrow the scope. No mission becomes VERIFIED from source presence or a passing compile alone. Contradictory deterministic, packaged, or screenshot evidence reopens the exact mission.

## Mandatory refinement loop for every Runner change

1. Cache the requested behavior and its observable acceptance criteria before editing source.
2. Inventory every system the change can affect: anatomy, constraints, contacts, gait metrics, curriculum, policy inputs/outputs, checkpoints, persistence, editor, rendering, terrain, performance, tests, packaging, branches, and release assets.
3. Record compatibility and regression risks before choosing an implementation.
4. Implement the smallest coherent change that satisfies the whole mission rather than patching the visible symptom alone.
5. Add deterministic positive, negative, adversarial, and repeated-seed tests for the identified risks.
6. Run local/static review, Linux warnings-as-errors, full Windows SDL3/Vulkan build, all tests, build-tree diagnostics, installed diagnostics, extracted-package diagnostics, checksum/manifest audit, and screenshot-level review where appearance or motion matters.
7. Re-read the diff and mission cache after testing; search for new consequences, stale assumptions, duplicated logic, misleading status, incompatible state, and untested branches.
8. Repeat review and testing until no material unaddressed consideration remains. Anything still uncertain stays OPEN and carries forward.
9. Merge, tag, publish, re-download, verify, and clean branches only after the ledger records exact evidence.

### WALK-PROCESS-138 — Cache-first dependency and regression discipline
**Status:** ACTIVE — RELEASE BLOCKING

The process above applies to this repository and is the required pattern for future project work. A release is blocked if implementation precedes mission capture, if a known interaction lacks an acceptance test, if an unfinished item disappears, or if evidence is described more strongly than it supports.

**Acceptance:** commit history shows the mission-cache update before implementation; each active mission names affected systems and tests; final release evidence records every remaining OPEN item instead of hiding it.

## Two-release consolidation plan

The repository currently contains one obsolete observer branch and one active v0.7.15 branch that diverged from workflow-only commits on `main`. The observer branch contains no product source and must be discarded after its historical purpose is confirmed. The active source branch must absorb the useful v0.7.15 work without losing mainline validation changes.

- **Runner v0.7.15:** terrain/render synchronization, camera scale, failed-policy recovery, structural rig evolution, real crouch qualification, side-view gait crossing, proper feet/traction, and related editor/diagnostic controls.
- **Runner v0.7.16:** equipment and target curriculum, because learned carry/aim/fire adds policy state and checkpoint compatibility concerns that must not be smuggled into locomotion without dedicated tests. Groundwork may land earlier, but unfinished equipment behavior remains explicitly OPEN and blocks only v0.7.16.

This split is a risk boundary, not a silent deferral. Both release targets remain in this ledger until published or explicitly superseded.

# Runner v0.7.15 locomotion, terrain, and structural-evolution completion

**Release state:** ACTIVE — no publication claim until every v0.7.15 release gate below passes.

### WALK-TERRAIN-139 — One visible and physical terrain state
**Status:** IMPLEMENTED — REVALIDATION REQUIRED

Flat lessons render their actual y=0 collision plane. Deformable lessons render the same treadmill-transformed fine-cell map used by collision, pressure, deposits, observations, and obstacles. Exposed/active surface regions remain granular; only deep inactive uniform regions may render as macro tiles. Duplicate height lines, fake moving ground, zero-distance columns, and stale coordinate transforms are forbidden.

**Affected systems:** environment coordinates, terrain sampling, material deposit, pressure, live renderer, PIP, camera, markers, obstacle placement, tests, package docs.

**Acceptance:** deterministic coordinate round-trip and render/collision sampling tests; Linux and Windows suites; live screenshot shows feet, terrain cells, obstacles, and pressure marks locked together with no duplicate ground.

### WALK-CROUCH-140 — Real squat-shaped crouch, not a forward bow
**Status:** OPEN — SCREENSHOT REOPENED

A crouch must lower the pelvis through bilateral leg compression. Clearing the platen by folding the torso forward is invalid even when feet remain on the ground.

Required evidence:
- meaningful pelvis drop relative to the verified standing pose;
- bilateral knee flexion and bounded hip flexion;
- torso remains within a bounded forward pitch instead of approaching horizontal;
- head clearance comes primarily from leg compression;
- center of mass remains over the semantic heel/ball/toe support interval;
- no hand, arm, knee, torso, head, or tail ground contact;
- stable held crouch followed by controlled standing recovery;
- arm motors cannot substitute for the squat.

**Affected systems:** duck teacher, posture stabilizer, gait metrics, stage qualification, evaluation quality key, PIP selection, rig evolution score, observations, telemetry, autosave semantics, tests.

**Acceptance:** an adversarial hip-hinge pose that clears the press is rejected; a bilateral squat passes repeated seeded hold/recovery tests; all seven presets retain valid Stand behavior; released screenshot visibly shows pelvis-down/knees-bent posture.

### WALK-SIDEGAIT-141 — Normal side-view limb crossing and alternating steps
**Status:** OPEN — SCREENSHOT REOPENED

Near and far legs may pass one another in screen space. One foot must be able to land ahead of the other without a support-separation solver forcing a split, fused plate, or jumping-jack pose. Semantic identity stays distinct even while silhouettes overlap.

Required evidence:
- left/right swing phases alternate;
- the swing foot passes the stance foot in x during a normal cycle;
- heel/ball/toe strike order and useful swing clearance are measurable;
- same-side repeated taps, wheel cycling, double-support translation, and friction-only shuffling receive no gait credit;
- rendering uses stable near/far depth order, outline/shading, and current limb geometry so crossing remains readable without changing physics.

**Affected systems:** support separation, contact clusters, self-overlap assumptions, gait counters, rendering order, PIP, teacher action, anti-exploit gates, preset geometry, tests.

**Acceptance:** deterministic gait-cycle fixture proves alternating crossing, forward progress, and nonzero swing clearance; adversarial sliding and same-side strike fixtures fail; live view shows one leg passing in front of the other.

### WALK-FEET-142 — Proper forward articulated feet and physical traction
**Status:** OPEN — SCREENSHOT REOPENED

Bipeds use explicit ankle, heel, ball, and toe geometry. The rear foot is a stable plate; the toe is an articulated segment. Contact transitions support heel strike, flat-foot loading, toe roll, toe-off, and swing clearance.

Traction must be physical and state-aware rather than an unconditional position pin or cosmetic anti-slide gate:
- static friction holds a loaded planted foot below a bounded tangential threshold;
- dynamic friction permits controlled breakaway, toe roll, crouch adjustment, and unstable-ground recovery;
- terrain firmness/looseness modifies traction consistently;
- moving limbs are not frozen by stance friction;
- course motion, world motion, and visual terrain motion use the same frame of reference;
- planted double-support translation cannot qualify as walking.

**Affected systems:** foot topology, support semantics, inverse mass, toe controller, ground solver, terrain pressure, gait metrics, rig save/load, editor deletion/remap, live/PIP rendering, tests.

**Acceptance:** heel/ball/toe phase tests, planted-foot static-slip bound, dynamic breakaway test, toe angular-rate test, loose/firm terrain traction comparison, no wheel-skating qualification, all-preset finite and Stand acceptance.

### WALK-EVOLUTION-143 — Structural rig evolution with nursery adaptation
**Status:** IMPLEMENTED — DETERMINISTIC AND PACKAGE VALIDATION REQUIRED

Evolution must begin from a minimal valid scaffold option and support real topology changes, not only strength and coordinate micro-tuning.

Required operators:
- split a bone by inserting a node;
- append a connected branch;
- duplicate or mirror a support branch;
- remove a nonsemantic passive leaf safely;
- mutate node position/radius, bone stiffness, motor range/strength, and semantic support assignment;
- rebuild rest lengths and calibrate motors;
- preserve existing motor slots where possible and initialize new slots neutrally;
- reject disconnected, cyclically invalid, unsupported, nonfinite, or semantically ambiguous candidates.
- protect semantic heel/ball/toe edges and weak visual braces from destructive bone splitting.

A topology candidate must receive a bounded nursery adaptation period before comparison. The champion rig and policy remain immutable rollback anchors. Repeated invalid candidates reset the nursery rather than poisoning live training. Accepted rigs synchronize to live view, editor, autosave, statistics, and the current lesson without restarting the application process.

**Affected systems:** blueprint validity/signature, policy transfer, action-slot mapping, checkpoint semantics, curriculum, persistence, editor, PIP/live publication, statistics, tests, performance.

**Acceptance:** scaffold is selectable and editable; each operator has positive/negative tests; candidate adaptation is bounded and deterministic; accepted/rejected/rollback counters update; live rig changes only after valid improvement; all existing presets remain valid.

### WALK-EDITOR-144 — Complete controls for gait, feet, evolution, and diagnostics
**Status:** OPEN

The rig lab must expose enough control to inspect and reproduce every active locomotion mission without editing files:
- preset and minimal scaffold selection;
- semantic root/torso/head and near/far support assignment;
- node/bone/motor creation, safe deletion, limits, strength, radius, stiffness, and rest calibration;
- near/far depth lane and draw order;
- foot contact visualization for ankle/heel/ball/toe;
- per-joint, paired-chain, alternating-chain, crouch, and gait-cycle test controls;
- friction/terrain test mode and live slip/strike/toe-phase telemetry;
- evolution enable/pause, generation, candidate/champion status, accept/reject/rollback counts, and explicit restart-nursery control;
- reset/load/save actions that state whether policy is preserved, transferred, or restarted.

**Affected systems:** UI layout, input hit testing, command queue, worker ownership, rig validity, telemetry, persistence, package diagnostics.

**Acceptance:** editor controls remain responsive under MAX training; invalid edits are rejected nonblockingly; deterministic UI-layout/control tests and packaged visual review pass.

### WALK-STATE-145 — Isolate corrected locomotion/evolution semantics
**Status:** OPEN

Any change to crouch qualification, gait evidence, foot traction, topology evolution, policy output mapping, or observations requires a new training-semantics value and isolated autosave/checkpoint/state paths. v0.7.14 and earlier policies may be explicitly imported as transfer weights only when dimensions match; they cannot silently resume as valid mastery.

### WALK-REGRESSION-146 — Exhaustive interaction audit for v0.7.15
**Status:** OPEN — RELEASE BLOCKING

Before release, re-evaluate at minimum:
- all seven preset Stand and static-crouch behavior;
- monoped single-support semantics;
- quadruped/crawler/hexapod multi-contact behavior;
- chicken anatomy and passive appendages;
- terrain volume, macro promotion/demotion, pressure, deposits, burial, and obstacles;
- feet, toe rates, support separation, crossing, friction, and gait evidence;
- curriculum order, champion selection, imitation, evolution, rollback, PIP, live preview, and statistics;
- editor responsiveness, save/load, incompatible state rejection, launch, package files, and optional-asset fallback;
- CPU worker determinism and bounded runtime cost.

The audit must be repeated after final fixes, followed by a diff/ledger reread. Newly discovered consequences are added here rather than waived.

### WALK-RELEASE-147 — Publish audited Runner v0.7.15
**Status:** OPEN — RELEASE BLOCKING

Required evidence: clean consolidated source branch; obsolete observer branch removed; no hidden applicators; Linux GCC 14 warnings-as-errors; full Windows SDL3/Vulkan build; every deterministic suite; build-tree/installed/extracted diagnostics; `run.bat` from unrelated working directory; acceptance matrix; package checksum and per-file manifest; release asset re-download and comparison; exact ledger evidence; zero open cleanup PRs; only `main` unless a documented next-release branch remains.

# Runner v0.7.16 equipment, carry, and target curriculum

**Release state:** CACHED AND OPEN — must not disappear if v0.7.15 publishes first.

### WALK-EQUIPMENT-148 — Unarmed, unequipped, safe, ready, and disarmed states
**Status:** OPEN

A rig must train and render with no item, an item carried safely, an item held ready, and an item disabled/disarmed or dropped. State transitions are explicit, observable, finite, and cannot corrupt the locomotion controller. Equipment mass and inertia affect balance honestly.

### WALK-WEAPONS-149 — Multiple abstract weapon classes
**Status:** OPEN

Provide simulation-level weapon classes with distinct mass, length, grip, recoil, cadence, projectile speed, spread, and handling envelopes. Initial classes should cover a compact one-hand weapon, a two-hand long weapon, and a heavier slow-handling weapon. Values remain fictional/gameplay-oriented; no real-world construction data is required.

Each class must support safe carry, ready carry, aim, fire, recoil recovery, and empty/disabled behavior. Two-hand equipment requires valid hand/grip topology and cannot attach to rigs without usable arms.

### WALK-TARGET-150 — Aim and fire at targets across distances
**Status:** OPEN

Generate stationary and moving targets at deterministic near, medium, and far distances, varied heights, and approach angles. The controller must acquire a target, align the weapon, fire only while ready, absorb recoil without losing foot support, and recover aim. Accuracy, reaction time, hit count, miss distance, unsafe discharge, balance loss, and locomotion continuity are measured separately.

Targets are training objects, not rewards for indiscriminate firing. Projectiles collide with targets and terrain, remain bounded, and cannot tunnel or be mistaken for existing thrown hazards.

### WALK-COMBAT-CURRICULUM-151 — Preserve locomotion while carrying and shooting
**Status:** OPEN

Evidence-gated order:
1. stand unarmed;
2. stand carrying a safe item;
3. walk/run carrying it;
4. crouch and recover while carrying it;
5. ready/disarm transitions while stationary;
6. aim at near targets;
7. fire and recover from recoil;
8. medium/far targets;
9. move, stop, crouch, aim, fire, and continue traversal;
10. mixed target and terrain course.

No shooting stage may bypass the corresponding unarmed locomotion prerequisite. Weapon handling cannot use ground contact, detached arms, impossible grip stretching, or recoil-driven movement as an exploit.

### WALK-EQUIPMENT-EDITOR-152 — Equipment and target editor controls
**Status:** OPEN

Expose controls for equipment class, none/safe/ready/disarmed state, handedness, primary/secondary grip nodes, mass/length/recoil/cadence gameplay parameters, target type, distance, height, velocity, spawn/reset, test fire, projectile visibility, hit markers, recoil vector, and curriculum stage. Invalid grip assignments are rejected without blocking training. The editor must clearly state when a rig lacks sufficient arms or motor slots.

### WALK-POLICY-153 — Separate locomotion motors from equipment actions
**Status:** OPEN — ARCHITECTURE DECISION REQUIRED BEFORE IMPLEMENTATION

The eight motor slots remain anatomy controls. Equipment state, aim, and trigger actions require a separately versioned policy-action extension rather than stealing leg or arm motors. Observations must add target/equipment/recoil state without overlapping existing channels. This changes network dimensions, checkpoint format, transfer rules, evaluation, PIP, autosaves, and performance tests.

**Acceptance:** dimension/layout static tests, checkpoint incompatibility tests, explicit transfer behavior, deterministic policy evaluation, and no regression to unarmed v0.7.15 acceptance.

### WALK-EQUIPMENT-REGRESSION-154 — Equipment interaction audit
**Status:** OPEN

Test every supported humanoid/biped equipment state plus honest rejection for rigs without usable hands. Re-run unarmed locomotion, crouch, gait, terrain, evolution, persistence, editor, renderer, and package acceptance with equipment disabled to prove the new subsystem is optional and nonregressing.

### WALK-RELEASE-155 — Publish audited Runner v0.7.16
**Status:** OPEN

Requires all equipment missions above or an explicitly documented reduced release whose unfinished items remain OPEN; Linux/Windows/package/re-download evidence; clean repository and branch state; exact mission-cache closure evidence.

# Runner v0.7.15 viewport, terrain, and failed-policy recovery

**Release state:** IMPLEMENTED — Linux and Windows package validation in progress.

- [x] Pull the live world camera from 90 to 22 pixels per meter so the rig is a small course subject instead of filling the viewport.
- [x] Move the live ground framing upward and place the rig left of center with more course visible ahead.
- [x] Remove the duplicate interpolated terrain polyline and moving dashed pseudo-ground.
- [x] Suppress the zero-distance sign that rendered as a large opaque column.
- [x] Render exposed, active, and near-surface terrain as fine granular cells while retaining deep inactive uniform macro tiles.
- [x] Render flat lessons from the same y=0 collision plane instead of showing the hidden deformable course map.
- [x] Use one canonical world-to-terrain treadmill transform for collision, pressure, deposits, sampling, and rendering.
- [x] Isolate v0.7.15 autosaves from poisoned v0.7.14 policies.
- [x] Restore the verified champion after catastrophic invalid or backward evaluations.
- [x] Reset a failed policy nursery after three catastrophic evaluations when no champion exists.
- [x] Bump the policy-training semantics to v0.7.15.
- [ ] Pass the complete Linux deterministic suite.
- [ ] Pass the complete Windows SDL3/Vulkan build, tests, diagnostics, installation, and extracted-package audit.
- [ ] Merge, publish Runner v0.7.15, and remove temporary validation infrastructure and stale observer work.

# Runner mission cache

This is the authoritative release ledger. A mission is VERIFIED only when implementation, deterministic acceptance, cross-platform validation, packaged-runtime behavior, and release evidence agree. Contradictory runtime evidence reopens the mission.

## Release target

**Target:** Runner v0.7.14

**Release state:** PACKAGE VERIFIED — v0.7.14 Linux, Windows SDL3/Vulkan, live SandHybrid terrain, natural toe-motion gates, optional armor reference, missing-asset fallback, installed/extracted diagnostics, checksum, and manifest passed; publication in progress.

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
**Status:** PUBLISHED — RELEASE VERIFIED BY v0.7.12

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
**Status:** PUBLISHED — RELEASE VERIFIED BY v0.7.12

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
**Status:** PUBLISHED — RELEASE VERIFIED BY v0.7.12

Standing control and qualification use each preset's authored body orientation and support topology. Quadrupeds, crawlers, hexapods, chickens, monopeds, bipeds, and humanoids may not be forced through one biped hip/knee correction. The quadruped must repeatedly establish a valid stage-one stance without being rearranged by diagonal support separation.

### WALK-CROUCH-085 — Restore leg-driven static crouch learning
**Status:** PUBLISHED — RELEASE VERIFIED BY v0.7.12

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
**Status:** PUBLISHED — RELEASE VERIFIED

Add one deterministic acceptance entrypoint shared by CTest and the packaged executable. `Runner --diagnose-acceptance` must run without opening a window and print an explicit pass/fail line for every acceptance case.

### WALK-PRESETS-100 — All-preset finite live-physics soak
**Status:** PUBLISHED — RELEASE VERIFIED

Step chicken, biped, humanoid, quadruped, crawler, hexapod, and monoped environments through the real effective controller. Every particle and observation channel must remain finite and every authored blueprint must remain structurally valid.

### WALK-RIGMATRIX-101 — Close the carried rig and curriculum acceptance backlog
**Status:** PUBLISHED — RELEASE VERIFIED

The matrix must verify semantic-support separation, humanoid and chicken strict six-seed balance, raised central shoulder geometry, leg-only duck authority, current-frame PIP fallback, monoped gait identity, and ordered stage evidence. Contradictory released-package evidence reopens only the exact affected mission.

### WALK-PACKAGE-102 — Run acceptance from installed and extracted packages
**Status:** PUBLISHED — RELEASE VERIFIED

The Windows package job must run version, Vulkan/package diagnostics, and `--diagnose-acceptance` from the build tree, installed directory, and independently extracted ZIP using unrelated working directories.

### WALK-RELEASE-103 — Publish audited Runner v0.7.9
**Status:** PUBLISHED — RELEASE VERIFIED

Build with GCC 14 warnings-as-errors and the complete Windows Vulkan toolchain, run all deterministic suites, publish ZIP/checksum/manifest assets, re-download and verify them, update this ledger with exact evidence, and leave only `main` with zero open pull requests.

## v0.7.9 immutable release evidence

- Exact tagged package source: `c1748749bfc2a52f9f2add54fc8029987bb4a22f`
- Validation and publication workflow run: `30799184889`
- Artifact ID: `8850995946`
- Artifact digest: `731ecc1a6ec82667517b888018fd97a62a36dd75511ad1775f4891d1b460de94`
- Release tag and title: `v0.7.9` / `Runner v0.7.9`
- Published assets: Windows ZIP, ZIP SHA-256, and per-file manifest
- Linux GCC 14 warnings-as-errors build and all five deterministic suites: passed
- Live acceptance matrix: 10/10 passed
- Full Windows Vulkan build and all six tests: passed
- Build-tree, installed, independently extracted, and re-downloaded release acceptance diagnostics: passed
- Published assets were byte-compared; ZIP checksum and extracted per-file manifest: passed
- Merged work and diagnostic branches removed; open pull requests: `0`; remaining branches: `main`
- Contradictory released-package evidence reopens only the exact affected mission

## v0.7.10 repository consolidation and cleanup

### WALK-CHANGELOG-104 — Single authoritative changelog
**Status:** PUBLISHED — RELEASE VERIFIED

Merge every `RELEASE_NOTES*.md` document into `CHANGELOG.md`, preserve release history, package the changelog, and prevent per-release note files from returning.

### WALK-CACHE-105 — Single authoritative mission cache
**Status:** PUBLISHED — RELEASE VERIFIED

`missioncache.md` is the only mission-named Markdown document. Any legacy mission ledger is imported before deletion, and unfinished work remains explicit rather than disappearing during cleanup.

### WALK-SOURCE-106 — Source and documentation cleanup
**Status:** PUBLISHED — RELEASE VERIFIED

Remove stale release triggers and validation debris, simplify duplicated CMake setup, replace contradictory README history, normalize text hygiene, and retain current build and runtime instructions.

### WALK-ACCEPT-107 — Harden executable acceptance
**Status:** PUBLISHED — RELEASE VERIFIED

Use true two-dimensional semantic-support clearance, reject an empty acceptance report, verify every curriculum stage label, and retain deterministic live acceptance across all authored presets.

### WALK-RELEASE-108 — Publish audited Runner v0.7.10
**Status:** PUBLISHED — RELEASE VERIFIED

Pass Linux warnings-as-errors, the complete Windows Vulkan build and tests, repository hygiene, build-tree/installed/extracted acceptance diagnostics, package manifest and checksum verification, release re-download audit, branch cleanup, and open-PR audit.

## Imported legacy mission ledgers

### Imported from `MISSIONS.md`

# Runner Mission Ledger

This file is the release-blocking source of truth for the Vulkan AI walking trainer.

## Rules

- Missions remain `OPEN`, `ACTIVE`, `BLOCKED`, or `VERIFIED` until evidence closes them.
- A feature is not complete because a class, thread, coroutine, or button exists in source.
- Every release must link the implementation, tests, measured behavior, and exact source commit.
- Missed or deferred missions carry forward and may not be silently removed.

## WALK-RIG-001 — Hip/joint editing must never lock the UI

**Status:** VERIFIED

Human, chicken, biped, quadruped, and monoped joint/node edits must be queued and coalesced. The input/render thread must never wait for PPO rollout, optimizer work, deterministic evaluation, curriculum transitions, rig evolution, autosave, or checkpoint serialization.

**Acceptance:**

- Selecting, dragging, and releasing every hip and knee node remains responsive while MAX CPU training is active.
- The public edit call returns within one render frame budget.
- The trainer eventually applies the newest coalesced rig state.
- A stress test repeatedly edits hip nodes while training and detects no deadlock, long stall, data race, or invalid rig publication.

## WALK-CONC-001 — Real persistent CPU parallelism

**Status:** VERIFIED

- Keep persistent rollout workers.
- Parallelize independent environment simulation.
- Parallelize deterministic evaluation where safe.
- Parallelize PPO minibatch gradient generation using worker-local gradients followed by deterministic reduction.
- Reserve capacity for input/render and avoid transient thread creation.
- Report real worker count, utilization, rollout throughput, update throughput, and queue depth.

**Acceptance:** measured training throughput increases across NORMAL, FASTER, and MAX CPU modes on a multicore CPU without freezing the UI.

## WALK-CORO-001 — Useful C++23 coroutine pipeline

**Status:** ACTIVE

Coroutines must divide meaningful trainer stages rather than merely wrapping one serial update:

1. apply queued commands,
2. launch/await rollout collection,
3. compute advantages,
4. launch/await parallel gradient batches,
5. reduce/apply optimizer update,
6. evaluate/curriculum/evolution,
7. publish immutable snapshot,
8. schedule autosave,
9. throttle according to speed mode.

**Acceptance:** coroutine suspension points correspond to independently schedulable work and are visible in diagnostics.

## WALK-UI-001 — Functional speed controls

**Status:** VERIFIED

NORMAL, FASTER, and MAX CPU must change measured trainer duty cycle and throughput. They must not merely batch additional serial updates behind the same mutex.

Show:

- updates per second,
- environment steps per second,
- rollout worker count,
- CPU mode,
- pending commands,
- trainer busy/idle state.

**Acceptance:** automated control tests verify mode latching; runtime benchmark shows distinct throughput and CPU-use behavior for all modes.

## WALK-ARCH-001 — Remove coarse trainer mutex stalls

**Status:** ACTIVE

No mutex may be held across an entire PPO update, multi-agent evaluation, rig evolution, or disk autosave while UI-facing operations can request the same lock.

Use immutable snapshots, command queues, worker-owned mutable state, short publication locks, and asynchronous persistence.

## WALK-OPT-001 — Parallel PPO optimizer

**Status:** VERIFIED

The current rollout simulation is parallel but the policy-gradient minibatch loop is serial. Add worker-local policy/gradient scratch, partition minibatches, reduce gradients deterministically, and apply Adam once per reduced batch.

**Acceptance:** identical seed/config produces deterministic parameters within the documented floating-point tolerance, and profiler data shows optimizer work distributed across workers.

## WALK-IO-001 — Asynchronous checkpoint/autosave

**Status:** OPEN

Serialize immutable policy/optimizer/rig snapshots on an I/O task without holding the live trainer lock. Use temporary-file plus atomic rename semantics.

**Acceptance:** checkpoint saves do not create visible input stalls or pause rollout workers for disk latency.

## WALK-TEST-001 — Concurrency regression suite

**Status:** OPEN

Required fixtures:

- hip drag during NORMAL training,
- hip drag during MAX CPU training,
- repeated preset swaps during training,
- speed-mode switching under load,
- pause/single update/background resume,
- checkpoint save/load under load,
- clean cancellation and shutdown,
- deterministic update comparison,
- ThreadSanitizer-capable CPU test configuration.

## WALK-REL-001 — Verified v0.5 release

**Status:** VERIFIED

Do not publish the next release until:

- Windows Release build passes,
- core and concurrency tests pass,
- Vulkan diagnostic passes,
- hip-edit stress test passes,
- all speed modes show measured differences,
- packaged executable reports the correct version,
- release asset identifies the exact source commit.

## WALK-COURSE-001 — Procedural obstacle and recovery treadmill

**Status:** VERIFIED

The live and training environments must continuously expose movement even when a controller produces no forward translation. A bounded virtual course advances independently of the walker and remains synchronized with physics, observations, rendering, rewards, and diagnostics.

Required terrain classes:

- flat road,
- inclines,
- elevated plateaus,
- declines,
- rolling hills,
- rough and uneven ground.

Required physical obstacle classes:

- rocks and low ground clutter,
- hurdles,
- overhead and duck-under bars,
- moving hazards,
- thrown projectiles with visible trajectories and physical impulse.

Required visual references:

- moving road dashes,
- numbered metre markers,
- mile conversion on markers,
- obstacle labels and approach visibility,
- actual walker distance separated from virtual course distance.

Required recovery behavior:

- collisions or major balance loss open a bounded recovery window,
- improving uprightness earns incremental reward,
- regaining supported upright balance earns a recovery bonus,
- falling or timing out applies a recovery penalty,
- live telemetry shows recovery-active state, attempts, and successes.

**Acceptance:**

- Course progress advances while the creature remains stationary.
- Terrain movement remains continuous across incline, plateau, decline, hill, and uneven sections.
- Every requested obstacle class is generated procedurally and represented in observations.
- Projectile and moving-hazard collisions apply bounded physical disturbance.
- Road markers make virtual movement obvious without faking actual walker distance.
- Recovery reward/state remains finite and deterministic.
- Core tests, full Windows build, Vulkan diagnostic, packaging, and exact release checksum pass before this mission becomes `VERIFIED`.


## WALK-OBS-001 — Complete obstacle sensing and recovery reward integrity

**Status:** VERIFIED

Every physical obstacle class must expose its actual collision size to the policy. Radial rocks, projectiles, and moving hazards use radius; hurdles and overhead bars use rectangular extent. Harmless upright contact must not open a rewardable recovery event.

**Acceptance:**

- Rock, projectile, and moving-hazard radius is present in observations.
- Hurdle and overhead-bar extent remains present in observations.
- Destabilizing impacts and major balance loss start recovery.
- Harmless upright contact cannot farm recovery bonuses.
- Hard falls remain terminal.
- Full Windows/Vulkan build, deterministic tests, diagnostics, package, and checksum pass.

## WALK-PHYS-001 — Biped support, traction, and world-anchored debris

**Status:** VERIFIED

Passive heel/toe geometry must participate in semantic left/right support. Designated feet require usable traction, while incidental head, tail, and body contacts must not pin the creature. Procedural rocks, hazards, and debris must remain in course/world coordinates and may not inherit actor translation.

**Acceptance:**

- Passive biped heel/toe contacts drive support observations, gait validation, airborne checks, rewards, and recovery.
- Foot contact retains substantially less horizontal velocity than incidental body contact.
- Head, tail, and torso ground contact slide rather than becoming unintended brakes.
- A course feature's world position depends on its stable sequence and course progress, never root position.
- Incompatible v0.6.1 autosaves are not resumed automatically.
- Full Windows/Vulkan build, deterministic tests, Vulkan diagnostic, package, checksum, and exact-source evidence pass.

## WALK-UI-002 — Readable responsive telemetry

**Status:** VERIFIED

The live and rig interfaces must remain readable at ordinary desktop sizes. Text may not overlap adjacent labels, buttons, cards, or the title bar. Long status messages must wrap or fit within their panel instead of being clipped into neighboring content.

**Acceptance:**

- Increase the default bitmap-font scale and minimum fitted scale.
- Use a taller responsive title bar with non-overlapping tabs and subtitle.
- Give live and rig side panels enough width for their controls.
- Group live metrics into readable cards with larger vertical spacing.
- Wrap long trainer/status lines and fit world telemetry to viewport width.
- Full Windows/Vulkan build and executable diagnostics pass with the responsive layout.

## WALK-COURSE-002 — Mile-marker obstacle schedule

**Status:** VERIFIED

All generated course elements must be anchored to shared course/mile-marker coordinates rather than actor position. Rocks, hurdles, overhead bars, moving hazards, and thrown projectiles must each appear in the advanced lesson. Every lesson receives a visible safe runway before its first obstacle, while bounded virtual course speed keeps the next marker from taking excessive real time to arrive.

**Acceptance:**

- Marker rendering and obstacle generation use the same spacing constant.
- The first three markers form a 24-metre safe runway.
- Advanced lessons cycle rock, hurdle, overhead bar, moving hazard, and projectile on consecutive markers.
- Moving hazards oscillate around their marker; projectiles originate and arc around their marker.
- No obstacle position includes actor/root translation.
- Virtual course speed brings the first obstacle into view promptly without removing the safe runway.
- Full Windows/Vulkan build, deterministic schedule tests, diagnostics, package, and exact-source evidence pass.

## WALK-GAIT-002 — Real stepping instead of wheel sliding

**Status:** VERIFIED

Forward reward must represent alternating supported locomotion, not a body sliding across planted contacts. Natural knee lead and bent-leg clearance are allowed; only an egregious low-foot body/joint-first shove into a rock or hurdle receives a mild shaping penalty. Sustained double-supported sliding remains an invalid gait exploit.

**Acceptance:**

- Zero-step sliding receives no positive forward-progress multiplier.
- Alternating contact plus visible swing-foot clearance earns the strongest gait multiplier.
- Grounded foot-cluster slip is measured and penalized.
- Sustained double-supported root motion with slipping feet terminates as wheel sliding.
- A knee may lead naturally while the foot is rising, close to the obstacle, or already above useful clearance.
- Only a large knee lead with a substantially trailing, low foot receives a mild shaping penalty and increments telemetry.
- The joint-clearance rule never terminates an otherwise valid episode and never overrides learned get-up or obstacle strategies.
- Foot-first and useful-clearance traversal are not penalized.
- Full Windows/Vulkan build, deterministic gait tests, diagnostics, package, and exact-source evidence pass.

## WALK-SAND-001 — Simulation-enemy locomotion curriculum

**Status:** VERIFIED

Retarget the curriculum from a generic treadmill demonstration to a grounded enemy controller suitable for later integration into a cellular sand simulation.

**Acceptance:**

- Spawn stance and flat patrol precede terrain and hazards.
- Long flat sections separate sand mounds and loose/deformed patches.
- Early debris and low-clearance hazards appear only on flat ground.
- Terrain-plus-hazard combinations unlock only in later combat traversal at higher difficulty.
- Deterministic evaluation actually runs long enough to encounter the first hazard.

## WALK-ROLL-003 — Head, tail, and body rolling are invalid locomotion

**Status:** VERIFIED

Non-foot body contact may slide during a fall but may not become a movement strategy.

**Acceptance:**

- Head contact cannot remain grounded long enough to propel the rig.
- Tail, torso, knee, and other non-foot ground contacts are detected semantically.
- Sustained body-ground rotation terminates as `HEAD / TAIL / BODY ROLLING`.
- Body-ground motion receives no gait progress multiplier and receives a strong penalty.
- A new autosave namespace prevents the v0.6.2 rolling policy from resuming.

## WALK-HAZARD-003 — Obstacles are hazards, never pickups or rewards

**Status:** VERIFIED

- Obstacles remain present through contact and are culled only after passing behind the actor.
- Ordinary obstacle contact cannot open a positive recovery-reward loop.
- Collision penalties exceed any incidental contact benefit.
- Hazard labels communicate danger rather than collectible/reward semantics.

## WALK-UI-003 — User-verified readable typography

**Status:** IMPLEMENTED — USER VISUAL REVIEW

The previous UI mission was incorrectly closed from compilation evidence without a visual acceptance pass. Increase all bitmap text substantially, enlarge minimum fitted text, marker signs, hazard labels, panels, and the default window. This mission remains active until the packaged application is visually confirmed readable by the user.

## WALK-LOCO-004 — Obstacle-capable bipeds, quadrupeds, and multi-leg enemies

**Status:** VERIFIED

The biped still fails to establish a usable gait, and the quadruped can stall and quiver at hazards because the old safe motor envelope cannot lift a leg high enough. Add explicit four-legged and six-legged simulation-enemy bodies and eliminate the high-energy no-lift local optimum.

**Acceptance:**

- Biped and humanoid hips/knees have bounded travel sufficient to clear configured rocks and hurdles without requiring an artificial foot-before-knee ordering.
- Quadruped can articulate a foot above the first debris target without excessive joint strength.
- High-energy obstacle quivering with no useful leg lift is detected, penalized, and eventually invalidated.
- Approaching a rock or hurdle creates a measurable foot-lift objective before collision.
- The QUADRUPED preset is a true four-leg body, not a two-leg articulated biped with a long torso.
- Near/far legs are staggered enough to remain distinguishable in side view.
- Four-legged crawler and six-legged hexapod presets are structurally valid, selectable, trainable, and use multi-foot semantic support groups rather than treating extra feet as body contact.
- Orange semantic foot nodes use flat-sole ground contact and cannot retain wheel-like horizontal velocity while grounded.
- Sustained double-supported pivoting around stationary orange foot nodes is invalidated as `FOOT-NODE ROLLING`.
- The UI renders primary orange foot contacts as flat soles rather than circular wheels.
- Deterministic tests cover all built-in presets, leg travel, support clustering, obstacle approach, quiver rejection, flat-foot contact, and foot-pivot rolling rejection.
- No release is prepared until every non-visual `OPEN` or `ACTIVE` mission in this ledger has passing evidence.

## WALK-IDLE-005 — Zero movement resets the episode

**Status:** VERIFIED

A non-balance controller may not occupy a rollout for most of an episode while producing no translation, no new gait step, and no useful leg lift.

**Acceptance:**

- Startup settling and active recovery are exempt.
- Two consecutive one-second zero-progress windows terminate as `ZERO MOVEMENT - RESET`.
- A real step, useful obstacle lift, or meaningful translation clears the idle accumulator quickly.
- Live telemetry shows the accumulated idle time.
- Deterministic tests and a packaged runtime trace confirm prompt reset without rejecting useful get-up behavior.

## WALK-GUIDE-006 — Automatic best-result self-imitation prior

**Status:** VERIFIED

The trainer automatically converts its best valid result into a small behavioral guide. The user does not need to author a demonstration, but may still correct the rig or controller through the existing tools.

**Acceptance:**

- Only a valid, grounded, stepped best result can seed the guide.
- Frames containing body contact or orange-foot pivot rolling are excluded.
- The guide contributes a bounded actor-only gradient and never replaces PPO reward learning.
- Prior weight decays from a modest maximum toward a small floor as the best result ages.
- A new stage, incompatible rig, reset, or better best result clears or rebuilds the guide.
- UI reports guide frame count and current weight.

## WALK-PIP-007 — Real training picture-in-picture

**Status:** IMPLEMENTED — USER VISUAL REVIEW

Publish one representative worker-owned rollout environment as an immutable snapshot and render it in a small upper-right picture-in-picture before the controls. It must show actual exploratory training, not a second deterministic live replay.

**Acceptance:**

- Snapshot copying occurs only at publication boundaries under worker ownership.
- The PIP identifies itself as a raw training sample and displays its own distance and invalid-motion state.
- Live rendering remains responsive while MAX CPU training is active.
- User confirms the PIP is visible, useful, and does not obscure primary telemetry.

## WALK-SKILL-008 — Ordered locomotion and acrobatics curriculum

**Status:** IMPLEMENTED — USER TRAINING REVIEW

Teach reusable skills in prerequisite order instead of exposing terrain and combat hazards before the controller owns basic body control:

1. stand upright,
2. duck and return to standing,
3. jump from joint power and land upright,
4. walk, then run,
5. duck or jump while walking/running,
6. perform controlled airborne flips and land,
7. combine standing, ducking, jumping, walking/running, and up to three spins to pass a mixed goal course.

Hazard contact is allowed. Touching an obstacle applies physical response and a bounded event penalty, but contact alone never terminates the episode. Passing the obstacle is the goal and earns progress. Ground rolling/body surfing remains invalid. A powered launch may remain airborne for a bounded stage-specific interval; hovering or unpowered sustained flight remains invalid. A fourth spin invalidates the run.

**Acceptance:**

- Curriculum labels and advancement follow the seven prerequisite stages above.
- Stationary duck, jump, and flip lessons do not trigger zero-movement rejection.
- Powered takeoff is recognized only when joint action energy and vertical launch speed exceed thresholds.
- Jump and flip lessons require supported upright landings.
- Flip lessons allow up to three airborne spins, while ground rolling and a fourth spin remain invalid.
- Moving-skill and mixed-goal lessons require forward gait and at least one passed obstacle.
- Ordinary hazard contact never creates a pickup/reward loop and never terminates by itself.
- Evaluation, self-imitation eligibility, telemetry, and deterministic tests use duck, jump, landing, spin, and obstacle-pass evidence.
- Real walking steps require measurable swing airtime and foot clearance; contact wiggles may not count as gait.
- Straight double-supported skating and pivot rolling around planted semantic feet are invalid.
- Policy actions are smoothed, early gait exploration receives a decaying periodic guide, and phase observations make repeatable cadence learnable before 10,000 updates.
- The best valid policy is a protected champion: substantial or invalid evaluation regression immediately restores it, reduces learning rate/exploration, and prevents the 15,000-update collapse.
- PPO uses a smaller clip range, fewer optimization passes, lower gradient norm, decaying entropy, bounded exploration, and a light champion anchor.
- The four-action controller remains intact for this pass. Independently controllable humanoid arms require a later controller/output and checkpoint-format expansion rather than stealing leg controls.

## v0.6.5 release closure

All non-visual locomotion missions introduced or reopened after v0.6.3 have passing deterministic tests, full Windows SDL3/Vulkan/built-in bitmap UI build evidence, and Vulkan diagnostics. The release includes true four-leg and six-leg support semantics, flat semantic feet, mature anti-rolling gates, a longer obstacle runway, zero-motion reset, automatic best-result imitation, relaxed joint-clearance guidance, and actual training picture-in-picture publication.

`WALK-UI-003` and `WALK-PIP-007` remain explicitly marked for user visual review because compilation cannot prove readability or preferred placement. They no longer conceal unfinished implementation work and do not block the requested v0.6.5 package.

The coroutine, ownership, asynchronous persistence, and ThreadSanitizer missions remain tracked for the separate v0.7 runtime pipeline in `V070_MASTER_PLAN.md`; they were never silently deleted or misrepresented as part of this locomotion release.

### Imported from `published-audit/missioncache.md`

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

### Imported from `release-stage/missioncache.md`

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

### Imported from `validation/mission-cache-locomotion.md`

# Mission-cache locomotion validation

- Exact tested source commit: $sourceSha
- Full SDL3/Vulkan/built-in bitmap UI Windows build: passed
- Core and concurrency tests: passed
- Biped/humanoid obstacle-capable motor-range tests: passed
- Quadruped anti-quiver and obstacle-lift tests: passed
- Four-legged crawler structural tests: passed
- Six-legged hexapod structural tests: passed
- Semantic multi-foot support clustering tests: passed
- Vulkan diagnostic: passed
- Release publication: intentionally blocked until remaining mission ledger items are complete

## v0.7.10 live regression correction

### WALK-PRESS-109 — Duck press must not drag the rig backward
**Status:** PUBLISHED — RELEASE VERIFIED

The press stays fixed over the authored test station, resolves contact vertically only, preserves particle velocity during positional correction, and has deterministic regression coverage for zero horizontal authority and zero solver-injected impulse.

### WALK-CURRICULUM-110 — Stand mastery must advance into crouch training
**Status:** PUBLISHED — RELEASE VERIFIED

Stand qualification and mastery use the same visible 10 rad/s joint-speed limit. Three consecutive all-six-seed strict Stand evaluations advance to Static Crouch; later stages retain eight-confirmation mastery. The status line exposes stance, spin, joint-speed, and mastery blockers.

### WALK-RELEASE-111 — Revalidate and publish corrected Runner v0.7.10
**Status:** PUBLISHED — RELEASE VERIFIED

Re-run Linux warnings-as-errors, all deterministic tests, full Windows Vulkan build/tests, installed and extracted package diagnostics, acceptance matrix, checksum/manifest audit, published-asset re-download, and branch/PR cleanup after WALK-PRESS-109 and WALK-CURRICULUM-110 pass.

### WALK-ART-112 — Display and package the original Runner artwork
**Status:** PUBLISHED — RELEASE VERIFIED

The original `assets/chicken.ppm` artwork is loaded during application startup and rendered in the live top bar when valid. Release/package validation requires a valid packaged asset; normal startup shows an artwork warning and continues without the decoration if a user-side copy is missing or malformed.

### WALK-AUTOSAVE-113 — Isolate corrected training state from v0.7.8
**Status:** PUBLISHED — RELEASE VERIFIED

Runner v0.7.10 writes and loads only `runner-v0710-*` autosave, evolved-rig, and autonomy-state files. It cannot silently resume the stale v0.7.8 curriculum or evolved rig that reproduced the live regression.

## v0.7.10 package validation evidence

- Exact tested implementation source: `041bf4c7480b8429c95fa2c344dee11662311a6c`
- Validation workflow run: `30806258665`
- Linux deterministic job: `91729382610` — passed
- Windows application/package job: `91729635569` — passed
- Validated artifact ID: `8861165056`
- Validated artifact digest: `sha256:a76e72afae050dbf053976cfba9c8f76f8de7e10762f33ed104235e72728324a`
- Validated binary ZIP SHA-256: `09B4EB3523E9E7A92B30999C073FDD2987E6E317F923DBF8D33C14341EB4B3DA`
- Duck-press anchor and velocity-preservation regressions: passed
- Stand mastery/progression regressions: passed
- Original packaged artwork load/render path: built and package-verified
- v0.7.10 autosave isolation: verified
- GCC 14 warnings-as-errors and all Linux suites: passed
- Full MSVC/SDL3/Vulkan build and all Windows suites: passed
- Build-tree, installed, and independently extracted diagnostics and acceptance: passed
- ZIP checksum and per-file manifest audit: passed

## v0.7.10 immutable release evidence

- Exact tested implementation source: `041bf4c7480b8429c95fa2c344dee11662311a6c`
- Exact tagged package/document source: `c3b9fd2daeac58405db55317b8ca959a3934418e`
- Validation workflow run: `30806258665`
- Linux deterministic job: `91729382610` — passed
- Windows application/package job: `91729635569` — passed
- Validated artifact ID: `8861165056`
- Validated artifact digest: `sha256:a76e72afae050dbf053976cfba9c8f76f8de7e10762f33ed104235e72728324a`
- Published tag and title: `v0.7.10` / `Runner v0.7.10`
- Published Windows ZIP SHA-256: `70756359E3596C27E2BB5E0E25717B531D8308CFCD683CC777E101E09EE61A2D`
- Published assets: Windows ZIP, ZIP SHA-256, and per-file manifest
- Duck press fixed-anchor and no-injected-drag tests: passed
- Stand all-six-seed progression and visible joint-speed gate tests: passed
- Original `assets/chicken.ppm` artwork is packaged, startup-loaded, and live-rendered
- Stale v0.7.8 autosaves cannot resume; v0.7.10 uses `runner-v0710-*`
- Linux GCC 14 warnings-as-errors and all deterministic suites: passed
- Full Windows SDL3/Vulkan build and all tests: passed
- Build-tree, installed, validated-artifact, final-extracted, and re-downloaded release diagnostics and acceptance: passed
- Published assets were byte-compared; ZIP checksum and extracted per-file manifest: passed
- Temporary publisher removed before tagging; open pull requests: `0`; remaining branches: `main`
- Contradictory released-package evidence reopens only the exact affected mission

## v0.7.11 packaged artwork startup correction

### WALK-PPM-114 — Repair and parse the original packaged P3 artwork
**Status:** PUBLISHED — RELEASE VERIFIED

Restore the six omitted dark-background pixels at the ends of five authored scanlines without shifting or replacing visible artwork. Replace formatted-stream parsing with a binary byte tokenizer that accepts standard ASCII whitespace, CRLF, comments, and an optional UTF-8 BOM while enforcing bounded dimensions, channel ranges, exact pixel count, and no unexpected trailing tokens.

### WALK-ARTSAFE-115 — Decorative artwork cannot brick Runner startup
**Status:** PUBLISHED — RELEASE VERIFIED

The application uses the original artwork when valid. A missing or malformed user-side decorative asset produces a visible warning and continues without the decoration; it cannot terminate the trainer. Release/package validation remains strict and rejects an invalid packaged asset.

### WALK-PKGART-116 — Package diagnostics must exercise the real artwork loader
**Status:** PUBLISHED — RELEASE VERIFIED

`Runner.exe --diagnose-package` parses `assets/chicken.ppm` with the same loader used at application startup. Deterministic tests parse the exact repository asset and BOM/comment/CRLF fixtures, so a release cannot pass by checking only that the asset directory exists.

### WALK-RELEASE-117 — Publish corrected Runner v0.7.11
**Status:** PUBLISHED — RELEASE VERIFIED

Run Linux warnings-as-errors and all tests, full Windows SDL3/Vulkan build and tests, build-tree/installed/extracted package diagnostics, executable-relative `run.bat`, checksum/manifest audit, release re-download, and repository cleanup before publication.

## v0.7.11 package validation evidence

- Exact tested implementation source: `cf5df4bd661821896ab51a1716f9d84609b3ead4`
- Clean validated head: `873144e587a879074e78e57fc35c7069b3921ce7` — differs only by deleting the PR trigger
- Clean release source: `bb63c54af7554d7399e084eb6afb274567f00936`
- Validation workflow run: `30831831894`
- Linux deterministic job: `91747416005` — passed
- Windows application/package job: `91747657685` — passed
- Validated artifact ID: `8863835903`
- Validated artifact digest: `sha256:8b65628030b469b114f44582b9b8c26f7cd6ee8a118819f45f03f1df4dd43de7`
- Validated Windows ZIP SHA-256: `5BDA297C836AFB3E50B7D868A54953992B305FA7D1E747C7AD8DD59709389067`
- Original art repaired from 634 to 640 pixels by appending six dark-background pixels to five scanlines
- Exact 20×32 scanline structure, portable parser fixtures, and malformed-data rejection: passed
- GCC 14 warnings-as-errors and all Linux deterministic suites: passed
- Full MSVC/SDL3/Vulkan build and all Windows tests: passed
- Build-tree, installed, independently extracted, and re-downloaded package diagnostics: passed
- ZIP checksum and all 11 per-file manifest entries: passed

## v0.7.11 immutable release evidence

- Exact tested implementation source: `cf5df4bd661821896ab51a1716f9d84609b3ead4`
- Clean validated head: `873144e587a879074e78e57fc35c7069b3921ce7`
- Merged implementation: `1aca8a6cca5931c7a5f75d82478876d0fe68304c`
- Exact tagged clean source: `bb63c54af7554d7399e084eb6afb274567f00936`
- Validation workflow run: `30831831894`
- Linux deterministic job: `91747416005` — passed
- Windows application/package job: `91747657685` — passed
- Validated artifact ID: `8863835903`
- Validated artifact digest: `sha256:8b65628030b469b114f44582b9b8c26f7cd6ee8a118819f45f03f1df4dd43de7`
- Validated Windows ZIP SHA-256: `5BDA297C836AFB3E50B7D868A54953992B305FA7D1E747C7AD8DD59709389067`
- Published tag and title: `v0.7.11` / `Runner v0.7.11`
- Published Windows ZIP SHA-256: `74F28FADB684D0D96B3E7330263FCB46BB59780876B04A9E03FCFC83E0241044`
- Published assets: Windows ZIP, ZIP SHA-256, and per-file manifest
- Original artwork is exactly 640 pixels in 20 scanlines of 32 pixels
- Portable P3 parser, exact-art test, BOM/comment/CRLF fixture, and malformed-data rejection: passed
- Decorative artwork failure is nonfatal in normal startup; package validation remains strict
- Linux GCC 14 warnings-as-errors and all deterministic suites: passed
- Full Windows SDL3/Vulkan build and all tests: passed
- Build-tree, installed, validated-artifact, final-extracted, and re-downloaded release diagnostics: passed
- Published assets were byte-compared; checksum and all manifest entries passed
- Contradictory released-package evidence reopens only the exact affected mission

## v0.7.12 rig progression and UI rollback

### WALK-RIGPROG-118 — Every preset must complete Stand then static Crouch
**Status:** PUBLISHED — RELEASE VERIFIED

Chicken, biped, humanoid, quadruped, crawler4, hexapod, and monoped each require named deterministic multi-seed Stand and static crouch/hold/recover acceptance. Aggregate finite-soak checks and two-rig standing checks are not sufficient.

### WALK-TOPOLOGY-119 — Drive support chains by rig topology
**Status:** PUBLISHED — RELEASE VERIFIED

Static crouch and crouch-walk teachers must discover the motor subtree that reaches semantic support nodes. Passive feet and non-biped body plans cannot be skipped because a motor's immediate driven node is not itself the final support seed.

### WALK-MASTERY-120 — Remove contradictory and impossible stage gates
**Status:** PUBLISHED — RELEASE VERIFIED

Stand mastery accepts five of six strict evaluation seeds while retaining posture, spin, survival, and joint-speed gates. Static crouch mastery requires the one authored press hold/recovery that an episode can actually produce, not two recoveries after the press has been removed.

### WALK-UICLEAN-121 — Remove unrequested ornamental UI
**Status:** PUBLISHED — RELEASE VERIFIED

Remove the large top-bar artwork card, labels, ghost skeleton, animated packets, pulsing rings, and floating torso chip. Do not add replacement controls or another toggle. Preserve the actual trainer, rig editor, telemetry, package validation, and artwork file support.

### WALK-STATE-122 — Isolate corrected rig training state
**Status:** PUBLISHED — RELEASE VERIFIED

Bump training semantics and use `runner-v0712-*` autosaves so stale v0.7.10/v0.7.11 policies cannot immediately recreate the reported Stand/Crouch stalls.

### WALK-RELEASE-123 — Publish Runner v0.7.12 only after packaged progression proof
**Status:** PUBLISHED — RELEASE VERIFIED

Require Linux warnings-as-errors, full Windows SDL3/Vulkan tests, all-seven Stand and Crouch acceptance, installed/extracted diagnostics, package checksum/manifest audit, and a clean repository before publication.

### WALK-FOOT-124 — Articulated forward heel-ball-toe feet
**Status:** PUBLISHED — RELEASE VERIFIED

Chicken, biped, and humanoid feet point forward in the side view and use a rigid rear foot plus a ball-to-toe hinge. Grounded toes plantar-flex for push-off; swing/crouch toes dorsiflex for clearance and stability. The monoped keeps its existing authored heel/toe motors instead of receiving duplicate feet.

### WALK-SYNERGY-125 — Discover and execute simultaneous joint chains
**Status:** PUBLISHED — RELEASE VERIFIED

Motor discovery includes left-chain, right-chain, bilateral crouch, and bilateral extension probes. Static crouch couples same-side hip, knee, and toe motion in one policy step, prioritizes hip flexion, and brakes excessive support-span widening instead of teaching a split.

### WALK-PREVIEW-126 — Stop uncontrolled preview foot sliding
**Status:** PUBLISHED — RELEASE VERIFIED

Semantic heel, ball, and toe contacts receive stance traction while retaining ordinary controlled sliding outside the static lessons. All seven named Stand/Crouch package gates must remain valid; friction-only shuffling still receives no gait credit.

## v0.7.12 package validation evidence

- Exact tested implementation source: `341c4b53a612c600386a521ac91900f2f70cf9f7`
- Validation workflow run: `30908862143`
- Linux deterministic job: `91990357553` — passed
- Windows SDL3/Vulkan package job: `91990645166` — passed
- Validated artifact ID: `8892807829`
- Validated artifact digest: `sha256:8667fa247e70d05654b8a7f8e2da69b732d0b673fb6c70b4b388ddd1544986c1`
- Validated binary ZIP SHA-256: `680DFA57F06F8853B52E33F8DD652348B8CE8E628A868411FD7C827E3D0CB1FD`
- Chicken, biped, humanoid, quadruped, crawler4, hexapod, and monoped Stand: `6/6` seeds each
- Chicken, biped, humanoid, quadruped, crawler4, hexapod, and monoped static Crouch/hold/recover: `4/4` seeds each
- Measured strict-Stand slip across all seven presets: `0`
- Forward articulated heel-ball-toe geometry, live toe hinge motion, and simultaneous bilateral hip/knee exploration: passed
- Full Linux warnings-as-errors, core, terrain, concurrency, runtime, and 22-case live acceptance: passed
- Full Windows application build/tests, build-tree, installed, and independently extracted diagnostics: passed
- ZIP checksum and 11-entry per-file manifest audit: passed

## v0.7.12 immutable release evidence

- Exact tested implementation source: `341c4b53a612c600386a521ac91900f2f70cf9f7`
- Exact tagged package/document source: `44a96f9dee43795db5ea0b91e15ebb1d52e3d060`
- Validation workflow run: `30908862143`
- Linux deterministic job: `91990357553` — passed
- Windows SDL3/Vulkan package job: `91990645166` — passed
- Validated artifact ID: `8892807829`
- Validated artifact digest: `sha256:8667fa247e70d05654b8a7f8e2da69b732d0b673fb6c70b4b388ddd1544986c1`
- Published tag and title: `v0.7.12` / `Runner v0.7.12`
- Published Windows ZIP SHA-256: `FAEA51619C1E47AB9EFF705A92F43009B29932AD19B13D7555CE12FE8480CB28`
- Published assets: Windows ZIP, ZIP SHA-256, and 11-entry per-file manifest
- Every preset Stand gate: `6/6`; every preset static Crouch/hold/recover gate: `4/4`
- Strict-Stand slip across all seven presets: `0`
- Forward articulated heel-ball-toe feet, live toe actuation, simultaneous hip/knee exploration, and coordinated crouch chain tests: passed
- Linux and full Windows builds/tests plus build-tree, installed, validated-artifact, final-extracted, and re-downloaded-release diagnostics: passed
- Published assets were byte-compared; ZIP checksum and extracted manifest: passed
- Temporary validator and publisher removed before tagging; open pull requests: `0`; remaining branches: `main`
- Contradictory released-package evidence reopens only the exact affected mission


## v0.7.13 toe-motion naturalness correction

### WALK-TOE-RATE-127 — Gate toe stabilization and push-off rate
**Status:** PUBLISHED — RELEASE VERIFIED

The articulated toe remains available for stance stabilization, crouch dorsiflexion, swing clearance, and forward push-off. Its command passes through a dead zone and stage/contact-specific slew limiter, and the physical hinge stays below an explicit stance/swing angular-rate ceiling even under alternating frame-by-frame policy input. The correction must preserve all seven Stand and static Crouch gates and must not reintroduce preview sliding.

### WALK-STATE-128 — Isolate corrected toe-control semantics
**Status:** PUBLISHED — RELEASE VERIFIED

Runner v0.7.13 uses training semantics `0x0007'1300` and `runner-v0713-*` policy, rig, and autonomy-state paths so learned v0.7.12 toe chatter cannot silently resume.

## v0.7.14 SandHybrid live-map integration

### WALK-SANDLIB-129 — Link the complete platform-neutral SandHybrid library
**Status:** PACKAGE VERIFIED

RunnerCore links `SandHybrid::SandHybrid` pinned at `99dd8acddfa9be1402981052b39cbf6284ed99ae` with SandHybrid native startup and Vulkan runtime disabled. Runner retains its SDL3/Vulkan application ownership while using SandHybrid material, terrain-generation, and sparse-section contracts. The pin, API identity, Linux/Windows builds, and static linkage must be package-verified.

### WALK-TERRAINSCALE-130 — Make rigs 3–5 macro tiles tall
**Status:** PACKAGE VERIFIED

One macro tile is exactly 8×8 fine cells. Chicken, biped, and humanoid authored height must remain between three and five macro tiles. The camera and collision world use the same scale; terrain may not be enlarged only cosmetically.

### WALK-HYBRIDMAP-131 — Canonical cells with instant macro promotion and demotion
**Status:** PACKAGE VERIFIED

Fine cells remain authoritative. A full uniform 8×8 region promotes immediately to derived macro metadata; changing or partially filling one cell demotes it immediately. Pressure, deposit, settling, material identity, structural state, volume conservation, promotion telemetry, and demotion telemetry require deterministic tests.

### WALK-LIVEMAP-132 — Train and render against the same SandHybrid map
**Status:** PACKAGE VERIFIED

Collision, observations, burial, material impacts, preview, training PIP, and live rendering consume one terrain state. The renderer batches macro-ready tiles and draws fine cells only for partial/mixed regions. No separate decorative heightfield is allowed.

### WALK-BRIDGE-133 — Preserve both canonical mission ledgers
**Status:** PACKAGE VERIFIED

`docs/SANDHYBRID_INTEGRATION_BRIDGE.md` pins the upstream commit and ownership boundary. Runner packaging includes that bridge and the pinned SandHybrid `missioncache.md`. No upstream `OPEN`, `PARTIAL`, `REGRESSION`, or `DEFERRED` mission is copied into history, renamed away, or marked complete by integration.


### WALK-ART-136 — Optional user armor concept assets
**Status:** PACKAGE VERIFIED

The four supplied modular sci-fi armor sheets are preserved as a compact optional contact sheet at `assets/optional/runner_armor_concepts/runner_armor_concepts.webp`, with provenance, hash, intended uses, and fallback rules documented beside it. The art may be cropped, repacked, recolored, separated into parts, or remade into a deterministic atlas before runtime use. Runtime adoption remains opt-in and the existing rig renderer remains the required fallback.

Acceptance requires the Windows package to include the optional reference and README, verify the recorded SHA-256, and still pass package and all-rig acceptance diagnostics after the entire `assets/optional` directory is removed. Missing or invalid optional art must never abort startup, alter training, or create automatic UI panels.

### WALK-CLIMB-134 — Reachable ledge climb and backward controlled descent
**Status:** OPEN — CARRIED FORWARD, NOT ORPHANED

After the combined live map is accepted, add a hard-wall curriculum where a rig climbs without jumping when its hands can reach a ledge at any ledge height, and turns backward to lower itself from a ledge when the remaining fall is no greater than its standing height. Completion requires hand/ledge contact, support transfer, no powered takeoff, and controlled feet-first recovery.

### WALK-WINSTACK-137 — Heap-backed canonical terrain storage
**Status:** PACKAGE VERIFIED

The canonical SandHybrid fine-cell map remains owned independently by every `Environment`, but its bulk arrays are heap-backed rather than embedded in the Windows thread stack. This preserves deterministic deep-copy value semantics and removes the default 1 MiB stack overflow reproduced by the Windows core, terrain, live-acceptance, concurrency, and runtime-pipeline tests.

Acceptance requires `sizeof(DeformableTerrain) < 128 KiB`, `sizeof(Environment) < 256 KiB`, Linux warnings-as-errors and the complete Windows test matrix to pass, and installed/extracted package diagnostics to remain unchanged. Raising the linker stack limit alone does not satisfy this mission.

### WALK-RELEASE-135 — Publish the combined Runner v0.7.14 package
**Status:** PACKAGE VERIFIED — PUBLICATION IN PROGRESS

Require pinned-library retrieval, Linux warnings-as-errors, full Windows SDL3/Vulkan build, SandHybrid API/scale/macro/fine/volume contracts, all existing Runner tests, all seven Stand and static Crouch gates, build-tree/installed/extracted diagnostics, both ledgers in the package, checksum/manifest audit, and clean branch/PR state.

## v0.7.14 package validation evidence

- Exact tested source: `4d7b43af2372d6f0ea3fc7739b90c8387395b51d`
- Merge commit: `57f04e0e7bc35396875a91a7a977c923df204bea`
- Validation workflow run: `30933084549`
- Validated artifact ID: `8902626678`
- Validated artifact digest: `sha256:516142ef91ac9d7f52f6e8cf259212484a201a4ccd1cc5a9e99558806b670d36`
- Validated Windows ZIP SHA-256: `A7A550DF7CC7E9FE7943C7856769DCC623816B6A6FB2CE8670E0479B6629149B`
- Linux GCC 14 warnings-as-errors build and deterministic suite: passed
- Full Windows SDL3/Vulkan build and complete test matrix: passed
- Every preset Stand and static Crouch acceptance gate: passed
- SandHybrid API, 8x8 macro/fine, scale, live-map, sparse-section, and volume contracts: passed
- Windows stack regression guards and heap-backed independent terrain storage: passed
- Build-tree, installed, and independently extracted diagnostics: passed
- Optional armor reference hash and package presence: passed
- Package and all-rig diagnostics with `assets/optional` removed: passed
- ZIP checksum and per-file manifest audit: passed
