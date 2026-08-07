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

**Release state:** PUBLISHED — LINUX/WINDOWS/PACKAGE/RE-DOWNLOAD/CLEANUP VERIFIED.

The current Walk/Run trainer still treats locomotion primarily as forward cadence plus speed. Direct training observation shows a critical game-AI failure on structural plateaus and ledges: a rig can reach a transition with insufficient support reserve, stumble, and lose the episode instead of slowing, shifting its center of mass, loading a stance leg, levering the body upward, taking a deliberate recovery step, or stopping briefly to regain control. The same architecture does not explicitly train acceleration from walk to run, controlled deceleration, reversal, turn-away behavior, or fleeing from an approaching threat. Crawling is a valid survival behavior but must remain an emergency escape mode, never a shortcut around learning upright gait.

The target is terrain-agnostic locomotion behavior that can be transferred to a game runtime through height/support/threat observations rather than SandHybrid-specific rules. SandHybrid remains one training environment, not the behavioral contract.

### WALK-BALANCE-RESERVE-233 — Track and reward usable balance reserve
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Derive a normalized support reserve from torso uprightness, semantic support state, and root position relative to the active support interval. Moving quickly with almost no reserve is penalized; increasing reserve during a stumble is positive progress even before forward distance resumes. Stable two-foot support and controlled single-support both remain valid.

### WALK-PLATEAU-LEVER-234 — Learn step-up and plateau levering
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

When near/mid terrain probes show a reachable positive step or plateau edge, reduce cadence, lift the swing chain higher, load and extend the stance chain, move the root over the planted support, then recover normal gait on top. Repeatedly striking the edge at running cadence, hanging below it, or vibrating against it receives no progress credit.

### WALK-SLOW-RECOVER-235 — Allow deliberate slow movement, stop, and regain
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

The policy may slow below nominal walking speed, take short corrective steps, or briefly hold position when balance reserve is low or terrain demand rises. Recovery progress must not be mistaken for zero-motion failure while the rig is measurably regaining uprightness or support reserve.

### WALK-WALK-FIRST-236 — Make correct walking the primary locomotion skill
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Upright sagittal walking with repeated left/right crossing, bounded slip, support reserve, and terrain adaptation must be established before speed incentives can dominate. Running cannot be used to blast through a weak walking policy.

### WALK-RUN-237 — Train true run acceleration, cadence, and braking
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

After walking is established, clear terrain and sufficient balance reserve may raise target speed and cadence into a run. The controller must accelerate without collapsing stride quality, then decelerate before ledges, hazards, sharp terrain changes, or depleted reserve. Overspeed without control is not mastery.

### WALK-DIRECTION-238 — Train reversal and 2D turn-away behavior
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

The side-view trainer must support a signed travel intent. A reversal requires controlled braking, support transfer, opposite-direction gait, and continued upright locomotion; simply falling, rolling, or being pushed backward does not count. Game integration may mirror facing visually, while the physical policy learns both +X and -X traversal.

### WALK-FLEE-239 — Run away from imminent threats
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

In the mixed hazard lesson, incoming direction, time-to-impact, density, and free-space information choose an escape direction. When the threat is urgent and escape space exists, the policy must turn/reverse if needed and accelerate away while preserving support. Evade, brace, or recover are valid context-dependent choices; standing in the impact path is not.

### WALK-CRAWL-LAST-240 — Crawl only as an emergency survival fallback
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Crawling is enabled only in late mixed/recovery training when the rig is already non-upright, upright recovery is not immediately viable, and obstruction/burial or a blocking ledge leaves an escape path. Crawl motion may preserve life and create space to stand, but it receives no upright gait credit and cannot seed Walk/Run champions.

### WALK-RECOVER-TO-STAND-241 — Crawl/recovery must return to upright locomotion
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Emergency crawl or prone escape is temporary. Once free space and support permit, reward transition back through kneel/brace to semantic-foot support, stable stance, then walking. Remaining prone after the obstruction clears is a failed recovery.

### WALK-TERRAIN-TRANSFER-242 — Train across material-independent terrain classes
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

The locomotion strategy consumes local height deltas, slope, firmness, looseness, support state, and obstacle/threat data. Training covers flat, rough, soft, firm, ramps, step-ups, plateaus, step-downs, deforming ground, and mixed hazards without keying behavior to a specific material ID or game name.

### WALK-DOMAIN-RANDOM-243 — Randomize terrain demand without randomizing away learnability
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Seeded episodes vary roughness, plateau/ledge placement and height within reachable limits, firmness/looseness, disturbance timing, and clear-run lengths. Early lessons stay learnable; later lessons combine variations. Exact seeds remain reproducible for regression tests.

### WALK-STRATEGY-244 — Centralize reusable locomotion planning
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Add a platform-neutral locomotion strategy layer that classifies hold/walk/run/recover/crawl/flee intent from existing physical observations. PPO bootstrap, reward shaping, deterministic tests, and future game integration consume the same calculations rather than duplicating terrain heuristics.

### WALK-TEACHER-245 — Terrain-aware gait bootstrap
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Replace fixed-frequency forward-only bootstrap behavior with a strategy-driven cadence, stride amplitude, swing lift, stance extension, direction, and counterbalance plan. Teacher influence remains a decaying bootstrap; PPO must still own the final policy.

### WALK-REWARD-246 — Reward control quality before raw speed
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Replace monotonic speed reward with target-speed tracking conditioned on terrain demand, gait establishment, direction intent, and balance reserve. Reward proper stepping, reserve recovery, step-up completion, safe acceleration, safe braking, and threat escape; penalize uncontrolled overspeed, repeated ledge impacts, and speed gained without gait evidence.

### WALK-RECOVERY-WINDOW-247 — Give constrained recovery enough time without permitting body surfing
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Late mixed training may extend the recovery window only while an explicit emergency-crawl condition remains true and measurable escape/recovery progress occurs. Ordinary body rolling, head dragging, friction surfing, and prone travel on clear terrain remain invalid.

### WALK-ANTI-EXPLOIT-248 — Preserve strict gait and survival truth
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

No new recovery allowance may grant Walk/Run credit for crawling, rolling, double-support shuffling, course-only motion, obstacle pushing, or being thrown backward. Signed-direction gait still requires real swing, contact transitions, and support evidence.

### WALK-GENERAL-TEST-249 — Deterministic plateau, reserve, run, reverse, flee, and crawl tests
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Add positive and adversarial tests for balance-reserve calculation, plateau slowdown/lever plan, walk-before-run gating, run target speed on clear terrain, reversal intent, flee direction, crawl-last-resort eligibility, crawl denial on clear terrain, and return-to-stand preference. Existing v0.7.18 coordinate and package tests remain mandatory.

### WALK-STATE-250 — Isolate v0.7.19 training semantics
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Bump training semantics and use `runner-v0719-*` autosave/state paths. Older v0.7.18 policies may be explicit transfer inputs only; they cannot silently resume as mastered general-locomotion policies.

### WALK-DOC-251 — Document general locomotion/game integration contract
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Document the terrain-independent strategy inputs, balance reserve, signed travel intent, emergency crawl boundary, and expected game-runtime use. Keep one changelog and one mission cache.

### WALK-RELEASE-252 — Publish audited Runner v0.7.19
**Status:** PUBLISHED — TAG/ASSETS/RE-DOWNLOAD/CLEANUP VERIFIED

Require Linux GCC 14 warnings-as-errors, full Windows SDL3/Vulkan build, all deterministic and live acceptance suites, installed/extracted diagnostics, ZIP/checksum/manifest, release re-download verification, clean branch state, and user eye-test reopening rules.

# Runner v0.7.20 viewport, preview continuity, and application identity

**Release state:** PUBLISHED — LINUX/WINDOWS/PACKAGE/RE-DOWNLOAD/CLEANUP VERIFIED.

The v0.7.19 user eye test proves two release-blocking defects remain. First, the live preview still restarts near the beginning despite background training showing longer motion. Source inspection identifies an unconditional `live_.set_course(...)` call during every published snapshot synchronization, which resets the live environment even when neither the rig nor course changed. Second, Windows high-DPI operation feeds framebuffer dimensions into application layout while Vulkan presents with a different surface extent, so the entire UI is effectively scaled down, the side panel is narrower than designed, text becomes unreadable, and world geometry can remain visible in the inter-panel gap. The screenshot also shows inconsistent spacing, weak telemetry contrast, crowded status text, and no coherent application icon.

### WALK-DPI-253 — Use one explicit application coordinate space
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Application layout, input, text, and widgets use logical window coordinates. Vulkan independently maps that logical canvas to the actual swapchain extent. Mouse coordinates, text measurement, layout calculations, and renderer push constants must agree at 100%, 125%, 150%, and 200% Windows display scaling without half-size UI or repeated swapchain recreation.

### WALK-CLIP-254 — Keep world and PIP rendering inside their viewports
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Terrain, distance signs, hazards, particles, rigs, and PIP content may not appear in the side panel, panel gap, margins, title bar, or outside their intended cards. The rendering path must provide deterministic clipping or an equivalent final mask; draw order alone is not accepted as the boundary contract.

### WALK-READABILITY-255 — Make all live text readable at normal desktop distances
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Use consistent text scales, line heights, contrast, card backgrounds, and minimum fitting limits. Primary status, current lesson, stage work, training results, controls, PIP state, and bottom controller state must remain readable without microscopic fallback text. Do not cram unrelated telemetry onto one line merely to avoid layout work.

### WALK-LAYOUT-256 — Align the complete live interface
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Top bar, world, PIP, side panel, buttons, cards, tabs, labels, and margins use shared layout constants and consistent alignment. Validate 1280x820, 1600x900, 1920x1080, the observed 2047x1112 high-DPI window, and 2560x1440. No box overlap, cropped label, negative dimension, or content escaping its parent is allowed.

### WALK-PREVIEW-CONTINUITY-257 — Stop resetting live motion on every publication
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

A normal immutable training publication must update telemetry and controller parameters without restarting the large live environment. Reset only for a real rig change, real course/difficulty change, explicit user reset, or terminal episode. A newly improved champion may be adopted without forcing the visible rig back to the starting line.

### WALK-PREVIEW-TRUTH-258 — Make the large preview represent retained progress
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

When a validated champion exists, the large preview uses retained champion parameters and runs a complete deterministic episode. Before a champion exists it may show the current policy, but it must not imply progress by replaying a two-step fragment. UI state clearly distinguishes current exploratory policy, retained champion, and terminal restart.

### WALK-SOURCE-CLEANUP-259 — Remove configure-time source patch indirection
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Fold the generated v0.7.19 source patches into canonical source files, remove the Python source-rewriter and generated-source CMake path, eliminate stale v0.7.6/v0.7.17 strings and paths, and keep one direct C++23 implementation. Release builds and IDE navigation must compile the same files developers edit.

### WALK-ICON-260 — Add a complete Runner application icon set
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Add a high-contrast Runner robot/speed icon as a transparent PNG, multi-resolution Windows ICO, and runtime window icon. Embed the ICO into the Windows executable and package the source PNG/BMP assets. The icon must remain recognizable at 16, 32, 48, 64, 128, and 256 pixels.

### WALK-UI-TEST-261 — Add deterministic UI, DPI, clipping, and preview tests
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Tests cover logical-to-surface scaling, mouse mapping, all supported layout sizes, panel/PIP containment, gap masking or clip ranges, readable minimum text scales, preview reset decisions, and the presence/validity of every icon size. Existing locomotion, terrain, concurrency, and acceptance suites remain mandatory.

### WALK-RELEASE-262 — Publish audited Runner v0.7.20
**Status:** PUBLISHED — TAG/ASSETS/RE-DOWNLOAD/CLEANUP VERIFIED

Require Linux GCC 14 warnings-as-errors, full Windows SDL3/Vulkan build, complete deterministic and live acceptance suites, UI diagnostic at all required dimensions, installed/extracted execution, runtime icon verification, ZIP/checksum/manifest, release re-download byte verification, and a final branch/workflow cleanup leaving only `main`.

# Runner v0.7.21 plain-language training dashboard

**Release state:** PUBLISHED — LINUX/WINDOWS/PACKAGE/RE-DOWNLOAD/CLEANUP VERIFIED.

The application now exposes enough internal training data to debug the trainer, but the default interface still requires reinforcement-learning knowledge. A normal failed evaluation can show a huge red negative score, `-INF`, hexadecimal quality keys, abbreviated counters, and rejection terminology without explaining whether training is healthy, what improved, what failed, or what must happen next. The default dashboard must answer five ordinary questions: Is it still learning? Is it getting better? What just happened? What is it trying to learn now? What specifically must improve before the next lesson?

The v0.7.20 Rig Lab screenshots also reopen anatomy and locomotion correctness. Automatic training is accepting geometry mutations that alter leg length instead of learning to control a fixed game character. The biped presets are authored as wide frontal splits rather than compact side-view bodies, multi-legged presets use malformed support plates or unarticulated branches, and the two-page Rig Lab places unrelated preset, file, structure, policy, motor, and test controls into one overflowing panel. These are part of the same release because unreadable diagnostics cannot be separated from visibly incorrect rigs and gait evidence.

### WALK-HUMAN-STATUS-263 — Summarize learning health in ordinary language
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Derive one stable headline from trainer state: `STARTING`, `TRAINING NORMALLY`, `TESTING CURRENT POLICY`, `VALID ATTEMPT FOUND`, `IMPROVING BEST RESULT`, `RETRYING AFTER FAILED TEST`, `TRYING A FRESH POLICY`, `PAUSED`, or `LESSON MASTERED`. The headline must not infer failure merely because an internal score is negative.

### WALK-LESSON-PROGRESS-264 — Show understandable lesson completion
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Display total updates prominently, then show current-lesson progress as a percentage and a simple bar. Progress is the conservative minimum of required update, episode, and evaluation work, capped at 100%; mastery confirmation is shown separately. Labels use full words such as `UPDATES`, `ATTEMPTS`, and `TESTS`, not `UPD`, `EPS`, or `EVAL`.

### WALK-LATEST-TEST-265 — Explain the latest evaluation result
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

The default results page says `LATEST TEST: PASSED` or `LATEST TEST: NOT YET PASSED`, followed by one plain-English reason such as `body touched the ground`, `needs more alternating steps`, `did not travel far enough`, `could not hold balance long enough`, or `joint motion was too violent`. A rejected test is presented as useful feedback, not as proof that learning is permanently broken.

### WALK-BEST-RESULT-266 — Report useful accomplishments instead of arbitrary score
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

The default page reports stage-relevant best/current evidence: standing time and valid seeds; crouch/hold/recovery; walking distance, steps, and survival; jump landings; obstacles passed; controlled flip landings; or mixed-course survival. Raw evaluation score and packed quality key are hidden from the default page.

### WALK-NEXT-GOAL-267 — State exactly what advances the lesson
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Show a concise stage-specific next goal. Examples: stay upright for six seconds across test seeds; crouch, hold, and stand back up; take real alternating steps and cover the required distance; land a powered jump; clear a hurdle; land a controlled flip; or survive the mixed course. When the required training budget is incomplete, say that more training/test samples are needed before mastery can be judged.

### WALK-COLOR-SEMANTICS-268 — Reserve red for actionable current failure
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Negative score magnitude, uninitialized best score, ordinary rejection, and incomplete work do not paint the entire card red. Cyan indicates information/work in progress, yellow indicates an unmet current goal or retry, green indicates valid evidence/mastery, and red is limited to broken rig, invalid numerical state, package/runtime failure, or a presently terminal motion fault.

### WALK-ADVANCED-269 — Preserve expert diagnostics behind an explicit page
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Add an `ADVANCED DIAGNOSTICS` page containing raw evaluation score, best score/update, quality key, rejection mask/name, policy/value loss, entropy, learning rate, environment steps, worker throughput, optimizer state, and pipeline details. The default page remains understandable without opening it.

### WALK-TOTALS-PLAIN-270 — Translate lifetime totals
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

The totals page groups data under `THIS RIG`, `THIS SESSION`, and `ALL TIME` with full labels. It explains that `ATTEMPTS` are completed simulated episodes, `VALID` means the motion passed safety/skill gates, `RESETS` are policy/episode restarts rather than lost all-time progress, and `ROLLBACKS` mean the trainer restored a better retained controller.

### WALK-INLINE-HELP-271 — Make unfamiliar terms self-explanatory
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Provide a compact on-screen legend or explanatory lines for total updates, lesson progress, retained champion, latest test, attempts, valid attempts, resets, and rollbacks. The UI must not require README knowledge to interpret its primary state.

### WALK-TELEMETRY-TEST-272 — Deterministically test every human-facing state
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Add pure C++23 tests covering no-evaluation startup, active training, valid evaluation, invalid evaluation, uninitialized infinities, fresh-policy retry, paused state, mastery, conservative progress calculation, stage-specific goal text, rejection translation, color severity, and advanced raw-value availability. Existing locomotion, UI, Windows, and package gates remain mandatory.

### WALK-STATE-273 — Isolate corrected rig and gait semantics
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

The readable dashboard remains presentation-only, but corrected preset geometry, gait evidence, and automatic tuning semantics invalidate silent reuse of v0.7.20 autosaves. Bump training/autonomy semantics and use `runner-v0721-*` autosave paths. Older checkpoints remain explicit transfer inputs only; malformed or structurally evolved v0.7.20 rigs may not silently resume as current presets.

### WALK-DOC-274 — Document the readable dashboard contract
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Update README, CHANGELOG, one focused v0.7.21 document, repository audit, package contents, and this single mission cache. Document every default label and its precise meaning without creating duplicate ledgers.

### WALK-AUTO-TUNING-275 — Stop automatic anatomy cheating
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Automatic curriculum refinement may tune motor strength, joint range, and structural stiffness only. It may not move nodes, change limb length, widen supports, add/remove/split branches, duplicate feet, or change semantic contacts while learning locomotion. Structural editing remains an explicit Rig Lab operation. Every automatically accepted candidate must preserve the exact node, radius, bone, topology, and support-semantic layout of its source rig.

### WALK-SIDE-GAIT-276 — Require real side-view fore/aft gait
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Biped, humanoid, scaffold, chicken, and monoped presentation must read as side-view anatomy rather than a frontal split. A credited crossing step requires the swing support to begin behind the stance support, clear the terrain, pass ahead of it, and land on the opposite contact phase. A leg that stays permanently ahead, spreads sideways, shuffles both supports, or gains progress only from the treadmill receives no sagittal gait credit.

### WALK-PRESET-ANATOMY-277 — Rebuild every shipped preset from explicit chains
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Audit scaffold, humanoid, biped, chicken, quadruped, four-leg crawler, hexapod, and monoped. Each preset must be connected, finite, centered, correctly scaled, have unique semantic supports, physically meaningful parent-pivot-child motor chains, no support-to-support brace masquerading as a limb, no fused feet, and a recognizable side silhouette. Presets are immutable templates; selecting one always restores its canonical anatomy.

### WALK-MULTILEG-278 — Give multi-legged rigs real support branches
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Quadruped and four-leg crawler use four distinct articulated two-segment legs with eight mapped joints and diagonal gait phases. The hexapod uses six distinct legs and alternating tripod support phases without rigid foot plates joining semantic supports. Multi-support gait bootstrap must drive support branches by semantic phase rather than returning a stationary balance action.

### WALK-RIG-LAB-279 — Replace the overflowing Rig Lab control wall
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Split Rig Lab into focused `PRESETS`, `STRUCTURE`, `MOTORS`, and `TEST` pages. Preset/file/policy/visual controls, node/bone editing, motor setup, and joint/traction testing may not share one unscrollable panel. Use responsive panel/world boxes, deterministic clipping, consistent spacing, full labels, and no overlapping or unreachable controls at every supported window size.

### WALK-RIG-FIT-280 — Center and fit every rig in the editor viewport
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Rig Lab computes bounds from the selected blueprint, centers the actual anatomy, and chooses a safe scale that keeps the full body, support nodes, labels, motor arc, and ground reference visible. Wide quadrupeds and hexapods may not be cropped or shoved to one edge; tall humanoids and monoped rigs may not overlap the joint-test area.

### WALK-RIG-TRUTH-281 — Make editor labels match actual behavior
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Replace `USE EVOLVED`, `RIG GENERATION`, and topology-nursery wording where automatic training now performs controller tuning only. Clearly distinguish canonical preset, manually edited custom rig, retained controller, fresh policy, and automatic parameter tuning. The UI may not imply that changing leg length is a valid walking solution.

### WALK-RIG-TEST-282 — Deterministically lock anatomy, gait, and layout
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Add tests proving automatic tuning preserves anatomy byte-for-byte; every preset is valid, connected, finite, centered, uniquely supported, and appropriately articulated; biped rest poses are compact side-view silhouettes; quadruped/crawler have four distinct articulated legs; hexapod has six independent supports with alternating tripod phases; crossing credit requires behind-to-ahead order reversal; and all four Rig Lab pages fit every supported window size.

### WALK-RELEASE-283 — Publish audited Runner v0.7.21
**Status:** PUBLISHED — TAG/ASSETS/RE-DOWNLOAD/CLEANUP VERIFIED

Require Linux GCC 14 warnings-as-errors, full Windows SDL3/Vulkan build, complete deterministic/live/UI/rig suites, readable-dashboard diagnostics, all-preset acceptance, installed/extracted execution, ZIP/checksum/manifest, published-asset re-download byte verification, and cleanup of temporary branches/workflows. The release includes every v0.7.20 locomotion, terrain, preview, DPI, clipping, and icon correction plus the corrected v0.7.21 rig and gait contract.

# Runner v0.7.22 black-frame rendering hotfix

**Release state:** CACHED BEFORE IMPLEMENTATION — RELEASE BLOCKING.

Direct packaged v0.7.20 and v0.7.21 eye testing shows that Live Autopilot, its right-side dashboard, the training PIP, Rig Lab, and all four Rig Lab pages can render only their outer borders over an opaque black interior. Source audit found the exact rendering fault: several post-content outline calls pass `Color{}` as an allegedly transparent fill, but `Color` defaults alpha to `1.0`. The renderer therefore draws the correct scene and controls, then covers each clipped region with an opaque black rounded rectangle. Existing UI tests verify only that clipped vertices do not escape; they do not require useful vertices to remain visible or detect an opaque full-panel overlay.

### WALK-BLACK-FRAME-284 — Remove opaque post-content masks
**Status:** OPEN — RELEASE BLOCKING

Replace every border-only use of default-constructed `Color{}` with an explicitly transparent fill or a true outline primitive. Live world, dashboard, PIP, Rig Lab viewport, progress bars, and all four Rig Lab pages must retain their already-generated content after border rendering.

### WALK-ALL-VIEWS-285 — Verify every application view contains useful visible geometry
**Status:** OPEN — RELEASE BLOCKING

The first rendered frame must contain non-background geometry inside Live world, Live dashboard, training PIP when available, Rig Lab viewport, and the `PRESETS`, `STRUCTURE`, `MOTORS`, and `TEST` pages. Switching pages repeatedly must not leak clip state or turn another panel black.

### WALK-CLIP-TEST-286 — Make clipping tests non-vacuous
**Status:** OPEN — RELEASE BLOCKING

Canvas clipping tests must require a clipped primitive to emit a nonzero triangle set, remain inside the requested bounds, preserve nested clip behavior, and unwind to depth zero. Add a regression contract that explicit transparent overlay colors have zero alpha and cannot become opaque through default construction.

### WALK-FRAME-DIAGNOSTIC-287 — Add deterministic visible-frame diagnostics
**Status:** OPEN — RELEASE BLOCKING

Add a CPU-only diagnostic/test path that renders representative Live and Rig Lab frames and verifies useful vertex counts and color diversity inside every content rectangle. Layout-only checks are insufficient; the diagnostic must fail on the exact border-with-black-interior screenshot.

### WALK-HOTFIX-COMPAT-288 — Preserve v0.7.21 training semantics
**Status:** OPEN — RELEASE BLOCKING

This is a rendering-only hotfix. Preserve v0.7.21 policy dimensions, rig anatomy, gait rules, terrain behavior, checkpoints, and `runner-v0721-*` autosave compatibility. Bump only the application/package version to `0.7.22` and move the equipment curriculum heading to v0.7.23.

### WALK-RELEASE-289 — Publish audited Runner v0.7.22
**Status:** OPEN — RELEASE BLOCKING

Require Linux GCC 14 warnings-as-errors, the complete deterministic/live/UI/rig suite, a full Windows SDL3/Vulkan build, the new visible-frame diagnostic for Live plus all four Rig Lab pages, installed/extracted package execution, ZIP/checksum/manifest creation, release re-download byte verification, and cleanup leaving only `main`.

# Carried open work

### WALK-CLIMB-134 — Reachable ledge climb and controlled backward descent
**Status:** OPEN — CARRIED TO A LATER RELEASE

Add a hard-wall curriculum where a rig climbs without jumping when hands can reach a ledge and turns backward to lower itself when the remaining fall is no greater than standing height. Completion requires hand/ledge contact, support transfer, no powered takeoff, and controlled feet-first recovery.

# Runner v0.7.23 equipment, carry, and target curriculum

**Release state:** CACHED AND OPEN — intentionally separated from the v0.7.22 rendering hotfix because equipment changes policy dimensions and checkpoint compatibility.

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

## Runner v0.7.21

**Status:** PUBLISHED.

- v0.7.21 validation source: `5094b0b2d54bf38a7e961d8384d696046b6781a3`.
- Main release workflow run: `31205911599`.

## Runner v0.7.20

**Status:** PUBLISHED.

- v0.7.20 validation source: `93d5480dc7edf71d8e21c61d0538a6dab6362a05`.
- Main release workflow run: `31197421312`.
- Publication recovery reused the byte-identical validated workflow artifact after a non-fast-forward evidence push race.

## Runner v0.7.19

**Status:** PUBLISHED.

- v0.7.19 validation source: `a41d4c4de7517d3f981ffa91560c1a43f8025153`.
- Main release workflow run: `31183722468`.

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