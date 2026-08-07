## 0.7.23

- Replaced destructive rounded-card border overdraw with a true inset perimeter ring.
- Prevented border-only cards from writing any center geometry.
- Added direct final-composite center and edge sampling.
- Restored final color-diversity requirements for Live, dashboard, PIP, and all Rig Lab pages.
- Preserved v0.7.21 training/checkpoint semantics and moved equipment work to v0.7.24.

## 0.7.22

- Fixed the black-interior regression covering Live Autopilot, its dashboard and PIP, Rig Lab, and all four Rig Lab pages.
- Replaced opaque default `Color{}` border fills with the explicit zero-alpha UI transparency contract.
- Made clipped-geometry tests non-vacuous and added nested clip validation.
- Added CPU final-frame compositing tests for every Live and Rig Lab content region.
- Preserved v0.7.21 training, anatomy, gait, terrain, checkpoint, and autosave semantics.

## 0.7.21

- Replaced raw default evaluation scores with plain-language training health, test result, progress, evidence, and next-goal summaries.
- Added conservative lesson progress from updates, completed attempts, and repeat evaluations.
- Added stage-specific rejection explanations and mastery targets.
- Added friendly Rig / Session / All-Time totals and inline definitions for attempts, resets, rollbacks, and total updates.
- Moved score, quality, loss, optimizer, worker, and pipeline data to Advanced Diagnostics; non-finite values show as unavailable.
- Stopped automatic training from changing limb length, support width, topology, or semantic contacts; automatic refinement now tunes controller parameters only.
- Rebuilt bipedal presets as compact side-view rigs and rebuilt quadruped, crawler, and hexapod support branches.
- Tightened gait credit to require a swing support to begin behind, clear, pass, and land ahead of the stance support.
- Split Rig Lab into Presets, Structure, Motors, and Test pages with automatic viewport fitting.
- Bumped rig/gait training semantics and isolated v0.7.21 autosave state while preserving explicit checkpoint transfer.

## 0.7.20

- Fixed high-DPI coordinate mismatch by separating logical UI dimensions from Vulkan drawable dimensions.
- Added deterministic Canvas clipping for the world viewport and training PIP.
- Rebuilt live telemetry, PIP, bottom status, panel spacing, and text sizing around shared layout boxes.
- Stopped immutable publication from resetting the large preview on every training update.
- Preserved complete live episodes when a better retained champion is published.
- Folded generated v0.7.19 patches into canonical C++23 sources and removed the source rewriter.
- Added transparent PNG, SDL BMP, and multi-resolution embedded Windows application icons.
- Added deterministic UI, DPI, clipping, preview-reset, and icon-format regression tests.

## 0.7.19

- Added balance-reserve-aware general locomotion planning for walk, run, recover, crawl, and flee behavior.
- Added terrain-aware slowdown, swing lift, stance extension, and braking for reachable plateaus and step-ups.
- Restricted anatomy motor-discovery probes to Balance so Walk training is no longer overwritten by discovery pulses.
- Added signed-direction threat escape and emergency crawl-with-return-to-stand constraints.
- Added falling/depositing sand to deformable Walk/Hurdle lessons while keeping the locomotion strategy material-independent.
- Reduced and deterministically randomized authored structural plateau heights into reachable training ranges.
- Changed the Live preview to display the retained champion when available and vary restart seeds after failures.
- Bumped training/autonomy semantics and isolated v0.7.19 runtime state.

## 0.7.18

- Fixed the moving-course coordinate-frame bug that measured treadmill gait in fixed camera X, causing real alternating steps to receive zero distance and fail qualification.
- Made locomotion distance, logical speed, step displacement, PPO shaping, and evaluation use terrain-relative progress while preserving the world-space anti-idle gate.
- Split safe incremental Walk candidate qualification from final mastery so two-step sagittal improvements can be checkpointed and refined instead of discarded.
- Extended early sagittal teacher/bootstrap authority and isolated corrected training semantics from v0.7.17 state.
- Retained the v0.7.17 terrain model, restored near-course markers/controls/telemetry, and kept optional body art disabled by default.

## 0.7.17

- Reopened released runtime failures from the v0.7.16 eye test.
- Replaced multi-node heel/ball/toe collision feet with terrain-conforming support stubs and non-physical side-view boot sprites.
- Added sustained sagittal gait evidence and explicit crab-walk rejection.
- Added fresh stage dwell so Stand, Crouch, and Walk cannot be skipped after a short inherited sample.
- Added a shallower rig-aware quadruped press, bounded four-chain crouch guidance, and stable post-retraction recovery.
- Added all four supplied optional concept sheets, provenance/hashes, derived P3 foot/helmet/torso/weapon sprites, safe fallback, layered side-view rendering, and Rig Lab controls.
- Isolated v0.7.17 learned state and added deterministic eye-test regression coverage.

# Changelog


## Runner v0.7.16 — Adaptive viewport and 25-mission usability batch

- Replaced the overly distant fixed 22 px/m live view with bounded rig-height fitting and a closer corrected default.
- Added viewport-only mouse-wheel zoom, Zoom Out/Auto View/Zoom In panel controls, reset-to-auto behavior, live px/m telemetry, forward lookahead, elapsed-time smoothing, and a screen-space dead zone.
- Enlarged the training PIP and tightened its local course window while preserving distant-hazard labels.
- Added shared camera math, deterministic camera/layout tests, and `Runner.exe --diagnose-camera` in build-tree, installed, and extracted package gates.
- Isolated v0.7.16 training semantics and autosave paths.
- Added `AGENTS.md`, the focused camera document, updated README, mission ledger, repository audit, package contents, and release automation.
- Preserved the equipment, target, policy-extension, and combined carry/fire curriculum intact for Runner v0.7.17.

## Runner v0.7.15 — measured static-friction contacts

- Kept terrain coordinate round-trip verification compile-time portable across GCC 14 and MSVC 19.51 without depending on `std::abs(float)` constexpr support.
- Removed all authored-coordinate foot pinning from the crouch curriculum.
- Added ground-solver contact anchors captured from actual heel/ball/toe collisions.
- Static support resolves zero-slip friction against the measured contact; moving stages retain bounded release for swing, toe-off, and jumping.
- Added adversarial tests proving the curriculum cannot touch feet and moving contacts are not magnetic.

## Runner v0.7.15 — active joint growth and state transfer

- Bone-split mutations may now activate one free anatomy action slot as a real articulated joint.
- Newly activated slots have their transferred actor row and bias zeroed before bounded nursery adaptation, preventing stale unused-output motion.
- Normal resume rejects older training semantics, while explicit dimension-compatible transfer imports weights only and clears optimizer, champion, curriculum, and mastery state.
- Expanded deterministic and executable acceptance from seven presets to eight by including the scaffold.

## Runner v0.7.15 — locomotion and evolution editor

- Added ALT-click bone selection, stiffness editing, selected-bone highlighting, and safe connected-rig deletion checks.
- Added near/far leg display control, explicit champion restore and fresh-policy controls, and live generation/accept/reject/rollback telemetry.
- Added manual, sweep, squat, and alternating gait-cycle previews plus firm/loose-ground traction diagnostics and heel/ball/toe labels.

## Runner v0.7.15 — side-view gait and traction

- Allowed near/far legs to cross during locomotion while retaining fused-foot separation in static lessons.
- Required paired-leg step credit to include a genuine lifted swing crossing before the next strike.
- Added terrain-aware static/dynamic foot friction, heel/flat/toe-off phases, crossing and slip telemetry, and readable near/far leg rendering.
- Added an alternating gait teacher and deterministic crossing, contact-phase, and firm-versus-loose traction tests.

## Runner v0.7.15 — real crouch correction

- Replaced head-clearance-only duck evidence with pelvis drop, bilateral knee flexion, bounded torso pitch, center-of-mass support, feet-only contact, held crouch, and upright recovery evidence.
- Reworked the paired-leg crouch guide to lower the pelvis and upper body as a unit while driving knees into a squat instead of shrinking the torso into a forward bow.
- Added explicit hip-hinge rejection, adversarial posture tests, and isolated autosaves for the corrected training semantics.

## Runner v0.7.15 — structural evolution completion

- Added a selectable minimal scaffold rig with two articulated leg joints per side and proper semantic feet.
- Added deterministic topology mutations for bone splitting, branch growth/removal, and support duplication alongside parameter evolution.
- Added a bounded 16-environment topology nursery that transfers the champion policy, adapts candidates before evaluation, accepts only stage-valid improvement, and restores the exact champion on failed application.
- Isolated the expanded evolution semantics from earlier v0.7.15 checkpoints.

## v0.7.15 - Viewport, terrain, and failed-policy recovery

- Pulled the live course camera back and reframed the terrain so the rig appears as a small training subject with meaningful course visibility.
- Removed the duplicate surface polyline, moving pseudo-ground dashes, and zero-distance obstruction.
- Preserved fine granular cells at exposed and active terrain while restricting macro-tile quads to deep inactive uniform material.
- Synchronized rendered terrain with the treadmill-space collision map and rendered flat lessons from their actual y=0 collision plane.
- Added automatic champion restoration for catastrophic invalid or backward generations and nursery reset when no valid champion exists.
- Isolated v0.7.15 autosaves and policy semantics from failed v0.7.14 training state.

All notable Runner changes are recorded here. The authoritative work ledger is `missioncache.md`; this file is the single release-history document.

## [0.7.14] - 2026-08-04

### Added

- Packaged the user-supplied modular armor direction as an optional, non-runtime concept contact sheet with a mandatory no-asset fallback.

- Linked the complete platform-neutral `SandHybrid::SandHybrid` library at pinned commit `99dd8acddfa9be1402981052b39cbf6284ed99ae` into RunnerCore.
- Added a live canonical fine-cell terrain with derived 8×8 macro metadata and SandHybrid 64×64 dirty-section scheduling.
- Added package-preserved integration ownership and upstream mission-ledger bridge documentation.

### Changed

- Moved the canonical fine-cell terrain arrays off each rig's Windows stack while preserving independent deep-copy terrain state.

- Rescaled terrain so chicken, biped, and humanoid bodies occupy approximately 3–5 macro tiles.
- Replaced the smooth preview fill with the same macro/fine pixel terrain used by collision and training.
- Preserved irregular granular edges while allowing deterministic structural 90-degree ledges.
- Folded the validated v0.7.13 toe command/hinge rate gates into the combined release and isolated v0.7.14 learned state.

## [0.7.13] - 2026-08-04

### Fixed

- Added stance/swing-specific slew limits to articulated toe commands so balance corrections cannot reverse every frame.
- Added a physical angular-velocity gate to the toe hinge after iterative solving, preserving propulsion while removing visible chatter.
- Added a command dead zone and adversarial alternating-input regressions for natural toe motion.
- Isolated v0.7.13 policy, rig, and autonomy state from earlier toe-control semantics.

## [0.7.12] - 2026-08-04

### Fixed

- Replaced impossible two-recovery static-crouch mastery with the single authored press hold/recovery cycle.
- Made Stand mastery use an explicit five-of-six robust seed gate instead of contradicting evaluation validity.
- Added topology-aware support-chain teaching for passive-foot, monoped, quadruped, crawler, and hexapod rigs.
- Added named Stand and static Crouch acceptance for all seven presets.
- Removed the unrequested top-bar artwork card and ornamental biomechanical overlay without adding more controls.
- Isolated corrected training with v0.7.12 semantics and autosave names.
- Rebuilt paired feet as forward-facing articulated heel-ball-toe chains with automatic toe stabilization and push-off.
- Added anatomy-aware simultaneous hip/knee discovery lanes and strong same-side crouch coordination.
- Narrowed the authored biped, humanoid, and chicken neutral stance and removed duplicate monoped feet.
- Added stance traction to semantic foot contacts to stop uncontrolled preview skating without crediting friction-only gait.

## [0.7.11] - 2026-08-03

### Fixed

- Restored six omitted dark-background pixels across five original Runner-art scanlines, producing the declared 32×20 image without shifting visible artwork.
- Replaced the Windows-fragile formatted-stream P3 parser with a portable binary tokenizer supporting comments, CRLF, and an optional UTF-8 BOM.
- Made decorative artwork failure nonfatal during normal startup while keeping packaged-release validation strict.
- Extended `--diagnose-package` and deterministic tests to parse the exact packaged `assets/chicken.ppm` file.

## [0.7.10] - 2026-08-03


### Live regression fixes

- Fixed the duck press so it remains anchored over the test station instead of following a displaced rig.
- Removed solver-injected press velocity that could convert vertical compression into backward sliding.
- Aligned Stand mastery with the visible 10 rad/s qualification limit and exposed joint-speed blockers in the status line.
- Reduced only the Stand lock to three consecutive all-six-seed confirmations; later curriculum stages retain eight confirmations.
- Loaded and rendered the original Runner pixel artwork from the packaged `assets/chicken.ppm` asset.
- Isolated v0.7.10 autosaves from stale `runner-v078-*` policy, rig, and curriculum-state files.

### Changed

- Consolidated every per-release notes file into this single changelog.
- Consolidated all mission documents into the single authoritative `missioncache.md` ledger.
- Replaced stale and contradictory README history with current build, curriculum, validation, and repository guidance.
- Simplified CMake setup and added a permanent repository-hygiene CTest.

### Fixed

- Corrected semantic-support overlap acceptance to use true two-dimensional clearance instead of horizontal distance only.
- Prevented an empty acceptance report from passing by vacuous truth.
- Expanded curriculum acceptance to verify every stage label and added matrix-shape checks.
- Removed stale one-shot validation artifacts and obsolete release tooling from source control.

## [0.7.9]

- Adds a deterministic executable live-acceptance matrix instead of leaving major rig and curriculum missions at vague manual-only acceptance.
- Exercises all seven presets through finite live physics stepping and verifies authored semantic-support separation.
- Re-runs strict six-seed standing acceptance for both humanoid and chicken rigs.
- Verifies the raised humanoid shoulder pivot, leg-only duck authority, current-frame PIP fallback, monoped gait identity, and ordered stage evidence.
- Adds `Runner --diagnose-acceptance` so the same acceptance matrix runs from build-tree, installed, extracted, and released packages.
- Adds a dedicated CTest target and carries contradictory packaged-runtime evidence forward as an exact mission reopen rather than silently ignoring it.
- Validates the exact packaged binary from unrelated working directories before publication.

## [0.7.8]

- Replaces sine-only uneven ground with a deterministic 224-cell, 56 m deformable sand heightfield shared by physics, observations, evaluation, replay, and both live/PIP rendering.
- Foot loading compacts loose cells, retains natural slip on soft support, displaces conserved volume into adjacent mounds, and relaxes over-steep slopes without creating or deleting terrain mass.
- Adds persistent falling sand, rocks, and debris; sand deposits into the terrain while rocks and debris bounce, roll, settle, and transfer impact velocity.
- Expands policy observations from 40 to 50 with firmness, looseness, burial depth, escape direction, incoming velocity, time-to-impact, density, obstruction mask, and surface slope.
- Adds burial/obstruction tracking, escape shaping, honest sustained no-escape termination, and PIP material telemetry.
- Adds seeded conservation, compaction, slope-collapse, material-spawn, observation-finiteness, and anti-tunneling regressions.
- Invalidates earlier policy/autonomy state with training semantics v0.7.8 and RUNAUTONOMY 13.
- Rebuilds the chicken around a vertical semantic torso and central load-bearing brace while retaining its horizontal bird body, raised head, beak, tail, leg-only motors, and separate feet; six seeded strict-balance runs guard the live 0/6 regression.
- Adds procedural biomechanical animation overlays to live rigs, the training PIP, and rig lab: semantic anatomy rings, neural-link pulses, motion-study ghosts, node halos, and a compact neural-chip motif with no new asset dependency.
- Expands material acceptance with deterministic repeated events, partial burial and escape-side detection, full no-escape burial termination, and direct/glancing impact anti-tunneling checks.

## [0.7.7]

- Restores rig-specific standing: quadrupeds and other multi-support presets no longer receive biped-only hip/knee corrections.
- Tests every motor alone in both directions, synchronized groups, and alternating patterns during dedicated early discovery lanes.
- Reduces compounded teacher dominance so PPO retains meaningful residual exploration and can branch beyond one pose.
- Replaces global left/right support pushing with authored-order non-overlap separation for every semantic support node.
- Rejects humanoid and biped jumping-jack support spans during standing and static crouch qualification.
- Changes static crouch to knee-first or authored compact-support control, then rewards the hold and recovery rather than accidental post-platen walking.
- Invalidates v0.7.6 checkpoints and autonomy state so the failed standing/crouch policies cannot be reused as valid progress.
- Keeps deformable sand terrain and falling-material/burial recovery explicitly carried in `missioncache.md` for the subsequent terrain release.
- Allows natural foot sliding during crouch and locomotion; only no-step planted-foot friction shuffling loses gait credit and receives a mild shaping penalty.
- Preserves cumulative evaluation accounting across recalibration and autosave state so evaluations, mastery, PPO updates, and the PIP remain synchronized.

## [0.7.6]

- Fixes the impossible standing-mastery loop: evaluation now reaches the same six-second target required by strict mastery.
- Requires six-of-six seeded strict standing results before one of eight mastery confirmations is counted.
- Rejects arms-overhead standing, uncontrolled standing rotation, non-foot contact, violent joints, and short stance results.
- Raises the humanoid central shoulder/chest pivot above both lateral shoulder pivots and restores hanging neutral arm geometry.
- Keeps the training PIP populated with the best current finite training environment, including rejected attempts and exact failure reasons.
- Shows standing target time, valid evaluation seeds, spin threshold, and upper-body angle directly in the UI.
- Invalidates v0.7.5 standing checkpoints and autosaves so the accepted arms-up/spinning controller cannot resume as progress.
- Carries deformable sand terrain and falling-material/burial recovery missions forward unchanged.

## [0.7.5]

- Carries falling sand/debris avoidance, impact recovery, burial escape, and continuation training into the next release mission ledger.
- Carries full deformable sand-cell terrain integration into the next release mission ledger rather than delaying this correction package.
- Uses the prerequisite order: stand, static crouch/hold/recover, walk/run, crouch-walk on unstable ground, jump/land, hurdles and low bars, controlled somersaults, then mixed traversal.
- Standing and static crouching require no movement; ordinary gait must be established before gait is combined with crouching.
- Restores monoped progression by counting real forward single-leg landing cycles instead of demanding alternating biped footfalls.
- Keeps training strictly staged: eight consecutive strict successes lock the best controller, then the next lesson builds on it without random mixed replay.
- Allows controlled somersaulting without requiring a separate powered-launch flag, permits forward-facing prone recovery outside crouch lessons, and retains the hard three-rotation limit.
- Replaces static duck folding with separate static-crouch and foot-only crouch-walk lessons.
- Adds uneven crouch terrain, low-bar avoidance, small ground hazards, and useful reaction distance.
- Invalidates any recognized duck attempt where knees, hands, torso, head, tail, or other non-foot nodes touch terrain.
- Rebuilds the training PIP as an honest live training view: the complete rig stays large, nearby terrain and obstacles remain visible, distant obstacles get a distance label instead of shrinking the rig, and failed attempts stay visible with the exact rejection reason.
- Shows update number, crouch time, crouch distance, gait cycles, and passed obstacles directly in the PIP.
- Invalidates v0.7.4 duck checkpoints and autosaves so the failed 10,000-update policy cannot masquerade as progress.
- Preserves the current working chicken preset.

## [0.7.4]

- Removes the former project brand from owned source, UI, executable, package, persistence names, documentation, and tests.
- Replaces the simulation-enemy title with `AUTONOMOUS RIG TRAINER` and `AUTONOMOUS PHYSICS LOCOMOTION LAB`.
- Removes the external GUI dependency and keeps the required bitmap font locally.
- Replaces the first moving low-bar duck lesson with a stationary overhead compression platen.
- The platen waits for stance, descends gradually, holds, retracts, and requires stable recovery.
- Adds one-way underside collision so the platen cannot clip through the model.
- Invalidates excessive press penetration and repeated robotic torso/shoulder-axis swinging.
- Keeps arms neutral during the compression lesson and teaches ducking through hips and knees.
- Moves low-bar traversal later and increases preparation distance for moving hazards.
- Invalidates v0.7.3 policy and autonomy persistence with v0.7.4 semantics.
- Carries forward and revalidates all open mission-ledger requirements before release.

- Restored genuinely rounded local UI panels after removing the external GUI dependency.
- Corrected PPO optimization-pass terminology that was accidentally changed during rebranding.
- Split training results and complete lifetime totals into readable panel pages.
- Added persisted cumulative training time plus complete per-rig, session, and all-time environment, episode, distance, step, fall, collision, jump, flip, obstacle, rig-change, reset, and rollback telemetry.
- Passed Linux GCC 14 and full Windows Server 2025 MSVC build, test, Vulkan/package diagnostics, installed launcher, checksum, and independent extraction audit.

## [0.7.3]

- Reopens v0.7.2 simulation-quality claims from Adam's August 2 live screenshots.
- Replaces each dangling three-contact foot cluster with one rigid ankle/heel/toe triangle and two semantic contacts.
- Rejects stretched bones, detached feet, exploded bodies, and non-finite body snapshots before elite or preview publication.
- Makes feet, knees, and hips the primary early balance actuators while strongly gating arms until stable support exists.
- Weakens bilateral coupling so it guides coordination without forcing mirrored leg collapse.
- Gives heads and passive tails realistic mass, endpoint damping, and torso-relative passive stabilization on every rig.
- Auto-fits the complete training-preview body and refuses to render disconnected fragments as a verified sample.
- Invalidates v0.7.2 learned state with v0.7.3 training semantics and autonomy-state format 6.
- Adds deterministic runtime-shaped tests plus Linux and full Windows Vulkan package gates.
- Fixes the short header/background at Windows DPI scaling by using drawable coordinates end-to-end.
- Adds Metric and Imperial display modes with 0.25 km / 0.25 mile course markers.
- Separates landed powered flips from uncontrolled generic spin and penalizes destabilizing spin outside flip training.
- Adds current-rig lifetime counters plus session and cumulative runtime totals to the live panel.
- Passed Linux GCC 14 tests and the full Windows 2025 SDL3/Vulkan build, test, launch, package, checksum, and independent extraction audit.

## [0.7.2]

- Reopens the simulation-quality missions after packaged runtime screenshots contradicted v0.7.1 validation.
- Adds coordinated bilateral joint synergies while preserving learned residual control.
- Replaces lower-leg endpoint feet with dedicated passive foot plates, heel contacts, and toe contacts.
- Restricts traction and foot support to explicit semantic foot nodes.
- Adds an obstacle-conditioned low-bar duck, clearance, pass, and return-to-stance lesson.
- Applies the same coordinated controller in PPO rollout, deterministic evaluation, rig evaluation, preview, and live execution.
- Rejects a previously qualified rollout when its current displayed frame is collapsed or unsupported.
- Bumps checkpoint and autonomy semantics so v0.7.1 behavior cannot silently resume.
- Makes source-tree run.bat prefer the current Release build instead of a stale root executable.

## [0.7.1]

- Fixes humanoid shoulder and elbow motors pinning the parent chest/body.
- Adds reciprocal rotational-inertia-weighted motor reaction with center-of-mass preservation.
- Requires sustained stance and controlled recovery evidence before a policy is valid.
- Selects best policies, rollback anchors, rig candidates, imitation trajectories, and PIP samples lexicographically by stage-valid evidence before reward.
- Rejects collapsed, unsupported, body-contact, violent-joint, motionless, skating, rolling, hovering, and prerequisite-incomplete candidates.
- Adds a strong neutral-action standing bootstrap that decays as the policy learns.
- Invalidates v0.7.0 checkpoints and autosaves with a new training-semantics signature and v0.7.1 paths.
- Adds bounded deterministic training, adversarial collapsed-pose, reciprocal-motor, and cross-rig regression coverage.
- Expands humanoid observations from 32 to 40 channels so all eight motor angles and velocities are independent.
- Uses the same effective balance controller in rollout collection, deterministic evaluation, self-imitation, live preview, and displayed execution.
- Latches completed standing lessons and requires at least four of six deterministic perturbed starts to pass.
- Adds bounded stance-evidence hysteresis so brief solver contact transitions do not erase a valid sustained stand.
- Fixes startup from Visual Studio, shortcuts, extracted release folders, and unrelated working directories by resolving shaders and assets beside the executable.
- Restores `run.bat` as the supported one-click source-tree and extracted-release launcher.
- Packages and validates `run.bat`, shaders, assets, and runtime DLLs from an unrelated working directory.
