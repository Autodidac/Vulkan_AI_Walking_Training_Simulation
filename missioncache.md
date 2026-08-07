# Runner cache-first engineering policy and active release plan

`missioncache.md` is the single authoritative active mission ledger. Closed historical mission definitions and their exact release evidence remain preserved in immutable Git history and tags; duplicate imported copies were consolidated out here so active work is not hidden beneath stale ledgers. No open mission was discarded.

## Mandatory refinement loop

1. Cache requested behavior and observable acceptance criteria before product-source edits.
2. Inventory interactions across anatomy, physics, gait, curriculum, policy state, persistence, UI, rendering, terrain, tests, packaging, branches, and releases.
3. Record compatibility and regression risks before implementation.
4. Implement the smallest coherent system change rather than a screenshot-only patch.
5. Add deterministic positive, negative, adversarial, and repeated-seed tests.
6. Run Linux warnings-as-errors, the complete Windows SDL3/Vulkan build and tests, build-tree/installed/extracted diagnostics, checksum/manifest audits, and visual review where appearance or motion matters.
7. Re-read source and this ledger after validation. New consequences stay explicit and OPEN until resolved.
8. Merge, tag, publish, re-download, byte-verify, and clean branches only after exact evidence is recorded.
9. Released-package eye testing outranks automated closure and reopens only the matching mission.

# Runner v0.7.18 runtime recovery, controls, and observability

**Release state:** PUBLISHED — LINUX/WINDOWS/PACKAGE/RE-DOWNLOAD/CLEANUP VERIFIED.

The v0.7.17 packaged terrain is explicitly retained. Direct runtime observations reopen only these facts: useful course reference markers are missing from the starting view; the visible update count reaches about 10 and appears to reset; cumulative total updates are not continuously visible; the trainer remains effectively at the beginning and ordinary walking has regressed; controls and telemetry are difficult to discover or interpret; and the optional torso/helmet skin is visually unacceptable.

Source audit found the exact update-loop contradiction: policy evaluation occurs at update 1 and every fifth update, while v0.7.17 resets a no-champion policy on every third invalid evaluation. That can reset the local policy counter at update 10 although Stand requires 120 fresh updates before its dwell gate can complete. `reset_training_state()` preserves cumulative totals, so training history survives but the primary UI hides it behind a resettable counter.

### WALK-RUNTIME-RESET-211 — Remove the update-10 nursery reset contradiction
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

A no-champion controller may not be automatically discarded before the current stage has received a meaningful training budget. Stand must be able to accumulate its full 120-update fresh-work requirement. Any later automatic nursery restart requires a substantially larger fresh-update/evaluation budget and preserves cumulative totals.

### WALK-TOTAL-UPDATES-212 — Make cumulative training progress continuously visible
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

The live world and Training Results show all-time `total_updates` continuously alongside the resettable policy/stage update, evaluation count, reset count, and updates/second. A policy restart must never look like all training progress disappeared.

### WALK-STAGE-PROGRESS-213 — Explain current stage work
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Publish fresh updates, episodes, and evaluations since stage entry plus each required threshold. The UI states whether the trainer is waiting on work, strict evidence, or mastery confirmations instead of only saying it is starting.

### WALK-MARKERS-214 — Restore useful markers without touching terrain
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Keep the current terrain, collision, pressure, treadmill transform, and course physics unchanged. Add a visible START reference and recurring near-course distance markers inside the initial viewport. Marker positions remain world/course-progress correct.

### WALK-MARKER-LABELS-215 — Use practical near-distance labels
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Metric reference markers use metres near the start and kilometres only at kilometre scale. Imperial markers use feet near the start and miles only at mile scale. Do not show nearby signs as `0.00 KM` or `0.00 MI`.

### WALK-CONTROLS-216 — Make runtime controls match their documentation
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

`Tab` switches Live/Rig Lab; `Space` runs/pauses background training; `1/2/3` select Normal/Faster/Max CPU; `T` toggles Results/Lifetime Totals; `U` toggles Metric/Imperial; `A` toggles optional body armor; `R` resets only live preview/camera state. Runtime mappings and README must agree.

### WALK-CONTROL-UI-217 — Put control help and trainer state in the application
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

The top bar/live panel advertise controls and continuously expose training state, speed mode, pause state, pipeline stage, stage-work progress, and throughput without requiring source knowledge.

### WALK-SKIN-218 — Disable the unacceptable fake body skin by default
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Optional torso/helmet/weapon overlays default OFF and remain explicitly toggleable. Forward sprite feet remain independently available. Optional art never affects physics, observations, policy state, terrain, package startup, or deterministic acceptance.

### WALK-WALK-BOOTSTRAP-219 — Restore useful early walking guidance
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Once Walk begins, paired-leg policies receive sufficient sagittal fore/aft teacher/bootstrap authority to demonstrate alternating foot passing and forward progress long enough for PPO to learn it. Existing crab-walk rejection, support integrity, and sustained-distance mastery remain strict.

### WALK-STATE-220 — Isolate corrected v0.7.18 learned/runtime state
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Bump training and autonomy-state semantics and use `runner-v0718-*` autosave paths so v0.7.17 reset-loop state cannot silently resume. Manual compatible weight transfer remains explicit.

### WALK-SOURCE-AUDIT-221 — Reconcile stale runtime assumptions across the source tree
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Audit application input/rendering, autonomy, PPO, curriculum, persistence, UI layout, CMake, tests, repository audit, docs, package contents, and release workflow for stale versions, stale controls, contradictory counters, and dead temporary infrastructure.

### WALK-REGRESSION-222 — Deterministically test reset, marker, state, and gait recovery
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Tests prove update 10 cannot trigger nursery reset; the complete Stand dwell can accumulate; a later bounded nursery restart remains possible; starting marker spacing is visible; v0.7.18 semantics are isolated; and paired-leg walking assistance produces meaningful opposite-phase sagittal drive without changing terrain coordinates.

### WALK-DOC-223 — Consolidate v0.7.18 documentation
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Update README, CHANGELOG, focused v0.7.18 documentation, this ledger, CMake install contents, and repository/package audits. Do not create another changelog or mission ledger.

### WALK-PACKAGE-224 — Audit the complete v0.7.18 package
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Require Linux GCC 14 warnings-as-errors, full Windows SDL3/Vulkan build, every deterministic suite, 24+ live locomotion acceptance, camera/package diagnostics, installed and extracted execution, optional-art fallback, executable-relative `run.bat`, ZIP/checksum/manifest, and workflow artifact upload.

### WALK-RELEASE-225 — Publish and verify Runner v0.7.18
**Status:** PUBLISHED — TAG/ASSETS/RE-DOWNLOAD/CLEANUP VERIFIED

Merge only validated source, tag `v0.7.18`, publish audited assets, re-download and byte-verify them, record exact evidence, delete temporary workflows/branches, close cleanup PRs, and leave only `main`.

# Runner v0.7.18 treadmill-coordinate walking correction

**Release state:** PUBLISHED — LINUX/WINDOWS/PACKAGE/RE-DOWNLOAD/CLEANUP VERIFIED.

The overnight v0.7.17 eye test reaches Walk but reports only zero-to-two credited steps while the course itself moves at walking speed. Source audit found a coordinate-frame contradiction: moving lessons scroll terrain with `course_progress()`, but gait strike displacement, `distance_travelled_`, `forward_speed_`, and forward reward are measured only in fixed screen/world X. A correct treadmill gait can therefore walk in place relative to the camera yet receive zero travelled distance, fail the 5.5 cm step-displacement gate, fail the 6 m qualification gate, and never create a valid Walk champion. The existing qualification gate also conflates a safe incremental candidate with final stage mastery, so a two-step improvement is discarded instead of checkpointed.

### WALK-COURSE-FRAME-226 — Use terrain-relative locomotion coordinates
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Moving-course locomotion distance and per-frame forward progress use the same transform as the scrolling terrain: world X plus `course_progress()`. Static Stand/Crouch/Jump lessons remain unchanged because their course speed is zero.

### WALK-STEP-FRAME-227 — Credit real alternating strikes on the treadmill
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Alternating step displacement is measured in terrain-relative locomotion X while foot crossing, swing-air time, swing clearance, and contact transitions remain physical world-space evidence. A walker may stay camera-centered without losing legitimate step credit.

### WALK-SPEED-FRAME-228 — Report and train terrain-relative forward speed
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Logical forward speed on moving lessons includes course speed plus physical root speed. PPO evaluation, speed mastery, reward shaping, telemetry, and overspeed use the resulting ground-relative speed; static lessons are numerically unchanged.

### WALK-INCREMENTAL-CHAMPION-229 — Separate safe candidate qualification from mastery
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Walk may checkpoint a physically valid incremental sagittal candidate after two alternating steps, at least one genuine limb crossing, one metre of terrain-relative progress, and two seconds of survival. Final Walk mastery remains strict at the existing 18 m / 16 stride / speed / survival requirements, and crab walking, body contact, invalid motion, and structural failures remain rejected.

### WALK-IDLE-GATE-230 — Preserve anti-idle and anti-vibration behavior
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

The one-second zero-progress anti-idle window stays in camera/world space and still requires useful swing lift or a credited step. Merely standing still while terrain scrolls must not count as active gait.

### WALK-BOOTSTRAP-231 — Keep useful guidance long enough to establish gait
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Early Walk bootstrap remains strongly sagittal through the first meaningful training window, then decays gradually so PPO takes control after a valid incremental walker exists.

### WALK-COORDINATE-TEST-232 — Deterministically lock the coordinate contract
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Regression tests prove terrain-relative distance/frame progress, nonzero moving-course distance with a camera-centered rig, opposite-phase teacher drive, and the existing v0.7.18 reset/marker contracts. Full Linux and Windows release gates remain mandatory.

# Runner v0.7.19 general locomotion, terrain transfer, and survival

**Release state:** CACHED BEFORE IMPLEMENTATION — RELEASE BLOCKING.

The current Walk/Run trainer still treats locomotion primarily as forward cadence plus speed. Direct training observation shows a critical game-AI failure on structural plateaus and ledges: a rig can reach a transition with insufficient support reserve, stumble, and lose the episode instead of slowing, shifting its center of mass, loading a stance leg, levering the body upward, taking a deliberate recovery step, or stopping briefly to regain control. The same architecture does not explicitly train acceleration from walk to run, controlled deceleration, reversal, turn-away behavior, or fleeing from an approaching threat. Crawling is a valid survival behavior but must remain an emergency escape mode, never a shortcut around learning upright gait.

The target is terrain-agnostic locomotion behavior that can be transferred to a game runtime through height/support/threat observations rather than SandHybrid-specific rules. SandHybrid remains one training environment, not the behavioral contract.

### WALK-BALANCE-RESERVE-233 — Track and reward usable balance reserve
**Status:** OPEN — RELEASE BLOCKING

Derive a normalized support reserve from torso uprightness, semantic support state, and root position relative to the active support interval. Moving quickly with almost no reserve is penalized; increasing reserve during a stumble is positive progress even before forward distance resumes. Stable two-foot support and controlled single-support both remain valid.

### WALK-PLATEAU-LEVER-234 — Learn step-up and plateau levering
**Status:** OPEN — RELEASE BLOCKING

When near/mid terrain probes show a reachable positive step or plateau edge, reduce cadence, lift the swing chain higher, load and extend the stance chain, move the root over the planted support, then recover normal gait on top. Repeatedly striking the edge at running cadence, hanging below it, or vibrating against it receives no progress credit.

### WALK-SLOW-RECOVER-235 — Allow deliberate slow movement, stop, and regain
**Status:** OPEN — RELEASE BLOCKING

The policy may slow below nominal walking speed, take short corrective steps, or briefly hold position when balance reserve is low or terrain demand rises. Recovery progress must not be mistaken for zero-motion failure while the rig is measurably regaining uprightness or support reserve.

### WALK-WALK-FIRST-236 — Make correct walking the primary locomotion skill
**Status:** OPEN — RELEASE BLOCKING

Upright sagittal walking with repeated left/right crossing, bounded slip, support reserve, and terrain adaptation must be established before speed incentives can dominate. Running cannot be used to blast through a weak walking policy.

### WALK-RUN-237 — Train true run acceleration, cadence, and braking
**Status:** OPEN — RELEASE BLOCKING

After walking is established, clear terrain and sufficient balance reserve may raise target speed and cadence into a run. The controller must accelerate without collapsing stride quality, then decelerate before ledges, hazards, sharp terrain changes, or depleted reserve. Overspeed without control is not mastery.

### WALK-DIRECTION-238 — Train reversal and 2D turn-away behavior
**Status:** OPEN — RELEASE BLOCKING

The side-view trainer must support a signed travel intent. A reversal requires controlled braking, support transfer, opposite-direction gait, and continued upright locomotion; simply falling, rolling, or being pushed backward does not count. Game integration may mirror facing visually, while the physical policy learns both +X and -X traversal.

### WALK-FLEE-239 — Run away from imminent threats
**Status:** OPEN — RELEASE BLOCKING

In the mixed hazard lesson, incoming direction, time-to-impact, density, and free-space information choose an escape direction. When the threat is urgent and escape space exists, the policy must turn/reverse if needed and accelerate away while preserving support. Evade, brace, or recover are valid context-dependent choices; standing in the impact path is not.

### WALK-CRAWL-LAST-240 — Crawl only as an emergency survival fallback
**Status:** OPEN — RELEASE BLOCKING

Crawling is enabled only in late mixed/recovery training when the rig is already non-upright, upright recovery is not immediately viable, and obstruction/burial or a blocking ledge leaves an escape path. Crawl motion may preserve life and create space to stand, but it receives no upright gait credit and cannot seed Walk/Run champions.

### WALK-RECOVER-TO-STAND-241 — Crawl/recovery must return to upright locomotion
**Status:** OPEN — RELEASE BLOCKING

Emergency crawl or prone escape is temporary. Once free space and support permit, reward transition back through kneel/brace to semantic-foot support, stable stance, then walking. Remaining prone after the obstruction clears is a failed recovery.

### WALK-TERRAIN-TRANSFER-242 — Train across material-independent terrain classes
**Status:** OPEN — RELEASE BLOCKING

The locomotion strategy consumes local height deltas, slope, firmness, looseness, support state, and obstacle/threat data. Training covers flat, rough, soft, firm, ramps, step-ups, plateaus, step-downs, deforming ground, and mixed hazards without keying behavior to a specific material ID or game name.

### WALK-DOMAIN-RANDOM-243 — Randomize terrain demand without randomizing away learnability
**Status:** OPEN — RELEASE BLOCKING

Seeded episodes vary roughness, plateau/ledge placement and height within reachable limits, firmness/looseness, disturbance timing, and clear-run lengths. Early lessons stay learnable; later lessons combine variations. Exact seeds remain reproducible for regression tests.

### WALK-STRATEGY-244 — Centralize reusable locomotion planning
**Status:** OPEN — RELEASE BLOCKING

Add a platform-neutral locomotion strategy layer that classifies hold/walk/run/recover/crawl/flee intent from existing physical observations. PPO bootstrap, reward shaping, deterministic tests, and future game integration consume the same calculations rather than duplicating terrain heuristics.

### WALK-TEACHER-245 — Terrain-aware gait bootstrap
**Status:** OPEN — RELEASE BLOCKING

Replace fixed-frequency forward-only bootstrap behavior with a strategy-driven cadence, stride amplitude, swing lift, stance extension, direction, and counterbalance plan. Teacher influence remains a decaying bootstrap; PPO must still own the final policy.

### WALK-REWARD-246 — Reward control quality before raw speed
**Status:** OPEN — RELEASE BLOCKING

Replace monotonic speed reward with target-speed tracking conditioned on terrain demand, gait establishment, direction intent, and balance reserve. Reward proper stepping, reserve recovery, step-up completion, safe acceleration, safe braking, and threat escape; penalize uncontrolled overspeed, repeated ledge impacts, and speed gained without gait evidence.

### WALK-RECOVERY-WINDOW-247 — Give constrained recovery enough time without permitting body surfing
**Status:** OPEN — RELEASE BLOCKING

Late mixed training may extend the recovery window only while an explicit emergency-crawl condition remains true and measurable escape/recovery progress occurs. Ordinary body rolling, head dragging, friction surfing, and prone travel on clear terrain remain invalid.

### WALK-ANTI-EXPLOIT-248 — Preserve strict gait and survival truth
**Status:** OPEN — RELEASE BLOCKING

No new recovery allowance may grant Walk/Run credit for crawling, rolling, double-support shuffling, course-only motion, obstacle pushing, or being thrown backward. Signed-direction gait still requires real swing, contact transitions, and support evidence.

### WALK-GENERAL-TEST-249 — Deterministic plateau, reserve, run, reverse, flee, and crawl tests
**Status:** OPEN — RELEASE BLOCKING

Add positive and adversarial tests for balance-reserve calculation, plateau slowdown/lever plan, walk-before-run gating, run target speed on clear terrain, reversal intent, flee direction, crawl-last-resort eligibility, crawl denial on clear terrain, and return-to-stand preference. Existing v0.7.18 coordinate and package tests remain mandatory.

### WALK-STATE-250 — Isolate v0.7.19 training semantics
**Status:** OPEN — RELEASE BLOCKING

Bump training semantics and use `runner-v0719-*` autosave/state paths. Older v0.7.18 policies may be explicit transfer inputs only; they cannot silently resume as mastered general-locomotion policies.

### WALK-DOC-251 — Document general locomotion/game integration contract
**Status:** OPEN — RELEASE BLOCKING

Document the terrain-independent strategy inputs, balance reserve, signed travel intent, emergency crawl boundary, and expected game-runtime use. Keep one changelog and one mission cache.

### WALK-RELEASE-252 — Publish audited Runner v0.7.19
**Status:** OPEN — RELEASE BLOCKING

Require Linux GCC 14 warnings-as-errors, full Windows SDL3/Vulkan build, all deterministic and live acceptance suites, installed/extracted diagnostics, ZIP/checksum/manifest, release re-download verification, clean branch state, and user eye-test reopening rules.

# Carried open work

### WALK-CLIMB-134 — Reachable ledge climb and controlled backward descent
**Status:** OPEN — CARRIED TO A LATER RELEASE

Add a hard-wall curriculum where a rig climbs without jumping when hands can reach a ledge and turns backward to lower itself when the remaining fall is no greater than standing height. Completion requires hand/ledge contact, support transfer, no powered takeoff, and controlled feet-first recovery.

# Runner v0.7.20 equipment, carry, and target curriculum

**Release state:** CACHED AND OPEN — intentionally separated from the v0.7.19 general-locomotion release because equipment changes policy dimensions and checkpoint compatibility.

### WALK-EQUIPMENT-148 — Unarmed, safe carry, ready, disarmed, and dropped states
**Status:** OPEN

### WALK-WEAPONS-149 — Multiple abstract gameplay weapon classes
**Status:** OPEN

### WALK-TARGET-150 — Aim and fire at deterministic targets across distances
**Status:** OPEN

### WALK-COMBAT-CURRICULUM-151 — Preserve locomotion while carrying and firing
**Status:** OPEN

### WALK-EQUIPMENT-EDITOR-152 — Equipment and target editor controls
**Status:** OPEN

### WALK-POLICY-153 — Separate locomotion motors from equipment actions
**Status:** OPEN — ARCHITECTURE DECISION REQUIRED BEFORE IMPLEMENTATION

The existing anatomy motor slots remain anatomy controls. Equipment state, aim, and trigger require a separately versioned policy-action extension with explicit observation/checkpoint migration tests.

### WALK-EQUIPMENT-REGRESSION-154 — Optional-subsystem nonregression audit
**Status:** OPEN

### WALK-RELEASE-155 — Publish audited equipment release
**Status:** OPEN

# Recent immutable release evidence

## Runner v0.7.18

**Status:** PUBLISHED.

- v0.7.18 validation source: `157b1754a40193e58b457b49e17c55b2cb7ee6e7`.
- Main release workflow run: `31169049948`.

## Runner v0.7.17

**Status:** PUBLISHED — RELEASE ASSETS RE-DOWNLOADED AND VERIFIED; LATER USER EYE TESTING REOPENED ONLY MISSIONS 211–225.

- PR #56 merged to `main` at `673aade7d02523df96687479289a1a3f81729326`.
- Published tag: `v0.7.17`.
- Authoritative PR validation run: `31097579829`.
- Linux GCC 14 warnings-as-errors and deterministic suite: passed.
- Full Windows SDL3/Vulkan build and complete test matrix: passed.
- All eight six-seed Stand cases: passed.
- All eight four-seed crouch/hold/recover cases: passed.
- Live acceptance matrix: 24/24 passed.
- Build-tree, installed, optional-art-removed fallback, archive, independent extraction, checksum, manifest, and artifact gates: passed.
- Published assets were re-downloaded and byte-verified; completed and accidental v0.7.17 branches were removed.

## Runner v0.7.16

**Status:** PUBLISHED — RELEASE ASSETS RE-DOWNLOADED AND VERIFIED.

- PR #55; merge `1577706cade4a47cfde9c2834af22279e2cd793f`.
- Validation run `31030378702`.
- Adaptive camera, PIP/layout, Linux, Windows, package, installed/extracted diagnostics, ZIP/checksum/manifest, publication, and cleanup passed.

## Runner v0.7.15

**Status:** PUBLISHED — RELEASE ASSETS AND PACKAGE AUDIT VERIFIED.

- Terrain/render synchronization, real crouch qualification, side-view gait crossing, physical traction, structural rig evolution, editor diagnostics, Linux/Windows/package gates, publication, and cleanup passed at release time.
- Later contradictory runtime behavior is tracked by the current matching missions rather than rewriting historical evidence.

## Historical ledger preservation

All earlier closed mission definitions, imported legacy copies, validation findings, and exact release evidence remain available in Git history and release tags. This consolidation removes duplicate/stale copies from the active file; it does not erase or reclassify historical evidence. Any historical requirement that becomes relevant again is reopened here with a new current mission and explicit acceptance criteria.