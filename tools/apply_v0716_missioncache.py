from pathlib import Path

path = Path('missioncache.md')
text = path.read_text(encoding='utf-8')
marker = '# Runner cache-first engineering policy and active release plan\n'
if marker in text:
    raise SystemExit(0)

section = r'''# Runner cache-first engineering policy and active release plan

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
**Status:** ACTIVE — PARAMETRIC EVOLUTION EXISTS; TOPOLOGY EVOLUTION NOT YET VERIFIED

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

'''

path.write_text(section + text, encoding='utf-8')
