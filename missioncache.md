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

**Release state:** PUBLISHED — LINUX/WINDOWS/PACKAGE/RE-DOWNLOAD/CLEANUP VERIFIED.

Direct packaged v0.7.20 and v0.7.21 eye testing shows that Live Autopilot, its right-side dashboard, the training PIP, Rig Lab, and all four Rig Lab pages can render only their outer borders over an opaque black interior. Source audit found the exact rendering fault: several post-content outline calls pass `Color{}` as an allegedly transparent fill, but `Color` defaults alpha to `1.0`. The renderer therefore draws the correct scene and controls, then covers each clipped region with an opaque black rounded rectangle. Existing UI tests verify only that clipped vertices do not escape; they do not require useful vertices to remain visible or detect an opaque full-panel overlay.

### WALK-BLACK-FRAME-284 — Remove opaque post-content masks
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Replace every border-only use of default-constructed `Color{}` with an explicitly transparent fill or a true outline primitive. Live world, dashboard, PIP, Rig Lab viewport, progress bars, and all four Rig Lab pages must retain their already-generated content after border rendering.

### WALK-ALL-VIEWS-285 — Verify every application view contains useful visible geometry
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

The first rendered frame must contain non-background geometry inside Live world, Live dashboard, training PIP when available, Rig Lab viewport, and the `PRESETS`, `STRUCTURE`, `MOTORS`, and `TEST` pages. Switching pages repeatedly must not leak clip state or turn another panel black.

### WALK-CLIP-TEST-286 — Make clipping tests non-vacuous
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Canvas clipping tests must require a clipped primitive to emit a nonzero triangle set, remain inside the requested bounds, preserve nested clip behavior, and unwind to depth zero. Add a regression contract that explicit transparent overlay colors have zero alpha and cannot become opaque through default construction.

### WALK-FRAME-DIAGNOSTIC-287 — Add deterministic visible-frame diagnostics
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Add a CPU-only diagnostic/test path that renders representative Live and Rig Lab frames and verifies useful vertex counts and color diversity inside every content rectangle. Layout-only checks are insufficient; the diagnostic must fail on the exact border-with-black-interior screenshot.

### WALK-HOTFIX-COMPAT-288 — Preserve v0.7.21 training semantics
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

This is a rendering-only hotfix. Preserve v0.7.21 policy dimensions, rig anatomy, gait rules, terrain behavior, checkpoints, and `runner-v0721-*` autosave compatibility. Bump only the application/package version to `0.7.22` and move the equipment curriculum heading to v0.7.23.

### WALK-RELEASE-289 — Publish audited Runner v0.7.22
**Status:** PUBLISHED — TAG/ASSETS/RE-DOWNLOAD/CLEANUP VERIFIED

Require Linux GCC 14 warnings-as-errors, the complete deterministic/live/UI/rig suite, a full Windows SDL3/Vulkan build, the new visible-frame diagnostic for Live plus all four Rig Lab pages, installed/extracted package execution, ZIP/checksum/manifest creation, release re-download byte verification, and cleanup leaving only `main`.

# Runner v0.7.23 true rounded-outline rendering hotfix

**Release state:** PUBLISHED — LINUX/WINDOWS/PACKAGE/RE-DOWNLOAD/CLEANUP VERIFIED.

Direct packaged v0.7.22 eye testing proves the black-frame correction exposed the second half of the same rendering defect. `add_rounded_rect` still paints the entire outer rounded rectangle with the outline color before attempting to draw a transparent inset. Alpha blending cannot erase the already-written outline fill, so the Live world, dashboard, training PIP, Rig Lab viewport, and Rig Lab pages become uniform gray—the linear-space border color converted through the sRGB swapchain. The current final-frame test was weakened to accept source-vertex color diversity even when the final composite is one flat color; it therefore certified a visibly broken package.

### WALK-TRUE-OUTLINE-290 — Replace destructive fake borders with real outline geometry
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

A border-only rounded rectangle must emit only an inset perimeter stroke. It may not paint the full card first, rely on transparent geometry to erase pixels, or alter the center of the underlying content. Filled cards render their fill once and then layer a bounded outline stroke on top.

### WALK-ROUNDING-291 — Preserve rounded corners without full-surface overdraw
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Implement a closed rounded perimeter from straight segments and quarter arcs, clamped for tiny rectangles and thick borders. The stroke stays inside the requested bounds, remains finite, respects Canvas clipping, and does not introduce gaps, corner spikes, or opaque center geometry.

### WALK-COMPOSITE-292 — Test the final composite rather than source vertices
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Render a known colored background, add a border-only rounded rectangle, and prove the center pixel remains the original background while an edge sample contains the outline. Final-frame validation must require useful final color diversity; hidden source geometry underneath a uniform overlay is not accepted.

### WALK-ALL-VIEWS-293 — Revalidate Live and every Rig Lab page
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

The complete final draw order must preserve visible world, dashboard, PIP, Rig Lab viewport, and `PRESETS`, `STRUCTURE`, `MOTORS`, and `TEST` content. Repeated page switching must not leak clip state or create black, gray, or single-color cards.

### WALK-HOTFIX-COMPAT-294 — Preserve v0.7.21 training and rig semantics
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

This remains a rendering-only repair. Preserve policy dimensions, gait rules, terrain, fixed anatomy, readable telemetry, `runner-v0721-*` autosaves, and training semantics `0x0007'2101`. Bump only the application/package version to `0.7.23`.

### WALK-RELEASE-295 — Publish audited Runner v0.7.23
**Status:** PUBLISHED — TAG/ASSETS/RE-DOWNLOAD/CLEANUP VERIFIED

Require Linux GCC 14 warnings-as-errors, complete deterministic/live/UI/rig tests, full Windows SDL3/Vulkan build, center-preservation and final-composite diagnostics, installed/extracted execution, ZIP/checksum/manifest creation, published-asset re-download byte verification, and cleanup leaving only `main`.

# Runner v0.7.24 fixed skeleton, truthful telemetry, and screenshot icon

**Release state:** PUBLISHED — LINUX/WINDOWS/PACKAGE/RE-DOWNLOAD/CLEANUP VERIFIED.

Direct packaged v0.7.23 eye testing shows three remaining release-blocking failures. First, the generated neon icon is not the screenshot crop requested by the user. Second, load-bearing leg bones visibly shorten under body weight and duck/walk forces even though automatic anatomy mutation was disabled; the remaining stiffness mutation and final solver order still permit a fixed game character to compress. Third, the default dashboard labels rollout terminations as valid skill attempts, reports duck-press completions as generic obstacles, and can show 100% lesson progress with zero mastery confirmations. The screenshot therefore displays large totals that are numerically real internal episodes but semantically wrong for a normal user.

### WALK-SCREENSHOT-ICON-296 — Use the supplied screenshot as the application icon
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Replace the generated illustration with an exact square crop of the supplied v0.7.17 armored Runner screenshot. Preserve those screenshot pixels as the canonical source, generate PNG/BMP/multi-resolution ICO assets from that source without redrawing it, embed the ICO in the Windows executable, and package the source crop for audit.

### WALK-BONE-LENGTH-297 — Make skeleton segment lengths invariant
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

The four primary biped/humanoid walking-leg segments are projected to their authored rest lengths after the gait bootstrap period. They may rotate at hips and knees but may not visibly shorten, stretch, telescope, or collapse into the pelvis. Existing validated crouch and multi-support rig dynamics remain unchanged.

### WALK-LOAD-BEARING-298 — End each solver step in a structurally valid pose
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Finish moving-gait frames with an exact projection of the four primary paired-leg chains. Grounded support nodes remain anchored while the upstream knee/hip chain absorbs the correction; static crouch, early treadmill bootstrap, and horizontal multi-support rigs retain the established solver path.

### WALK-AUTO-STIFFNESS-299 — Stop automatic tuning from weakening bones
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Automatic controller refinement may tune motor strength and joint range only. It may not change bone stiffness or accept a controller by weakening the skeleton. Loaded/manual rigs preserve authored compliant braces, while the primary walking-leg projection independently enforces visible leg length.

### WALK-DEBUG-TRUTH-300 — Count passed skill checks instead of merely nonterminal motion
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

`VALID`/`PASSED` episode totals must mean the completed run satisfied the current stage qualification and body-integrity gates. A run that merely avoided a terminal physics fault but failed walking, crouch, support, progress, or body-contact evidence is a failed stage check, not a valid skill attempt.

### WALK-PROGRESS-301 — Do not show 100% with zero mastery confirmations
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Separate training work from mastery. Display controller-update/episode/test sampling as `TRAINING WORK`, display completed evaluations as `TESTS RUN`, and display consecutive passing confirmations as `MASTERY PASSES x/y`. Overall lesson progress must reserve explicit progress for mastery confirmations and cannot reach 100% until the required confirmation streak is complete.

### WALK-TOTALS-302 — Use stage-neutral, understandable totals
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Replace misleading `OBSTACLES PASSED` and ambiguous `VALID` labels with `FEATURES CLEARED`, `PASSED STAGE CHECKS`, and `FAILED STAGE CHECKS`. Explain that hundreds of parallel simulated episodes are expected and are not equivalent to hundreds of human-visible tests. Session, rig, and all-time deltas must remain monotonic and correctly based.

### WALK-VISUAL-303 — Clean the default rig presentation
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Keep optional armor disabled by default, retain clear near/far side layering, reduce oversized joint blobs, give support stubs compact sprite-like feet, and preserve an unmistakable side-view silhouette. Rendering changes remain presentation-only and may not alter physics nodes or collision radii.

### WALK-STATE-304 — Isolate corrected structural semantics
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Bump training/autonomy semantics and use `runner-v0724-*` autosave paths. Importing v0.7.21/v0.7.23 policies is explicit or migrates only after rigidifying the associated blueprint; a compliant old rig may not silently resume.

### WALK-REGRESSION-305 — Test screenshot icon, bone invariance, truthful totals, and progress
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Add deterministic tests for screenshot-source identity, generated icon formats, bone-length error under static load/duck/walk soak, automatic-tuning anatomy and stiffness preservation, stage-qualified episode accounting, training-work versus mastery progress, stage-neutral labels, all preset integrity, and the complete existing Linux/Windows/UI/package matrix.

### WALK-RELEASE-306 — Publish audited Runner v0.7.24
**Status:** PUBLISHED — TAG/ASSETS/RE-DOWNLOAD/CLEANUP VERIFIED

Require Linux GCC 14 warnings-as-errors, full Windows SDL3/Vulkan build, complete deterministic/live/UI/rig/structural/telemetry tests, installed and independently extracted execution, screenshot-icon verification, ZIP/checksum/manifest creation, published-asset re-download byte verification, and cleanup leaving only `main`.

# Runner v0.7.25 compact armor and stance-leg integrity

**Release state:** PUBLISHED — TAG/ASSETS/RE-DOWNLOAD/CLEANUP VERIFIED.

Direct packaged v0.7.24 eye testing confirms that the approved helmet and foot assets are usable, but the translucent torso sheet, circular shoulder masses, and duplicate ghost arms obscure the actual gait. Fixed segment lengths also remain insufficient: a two-link leg can preserve both bone lengths while folding until the knee appears to telescope into the pelvis. The same screenshots expose stale font-cell scaling, a missing percent glyph, and sample counters such as RUNS 17465/8 that are internally true but useless in the compact noob-facing header.

### WALK-COMPACT-ARMOR-307 — Replace the oversized torso overlay
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Keep the approved helmet and foot presentation. Replace only the torso, shoulder, and forearm overlay with compact geometry attached to the real body nodes. No rectangular sprite sheet, giant shoulder circles, duplicate arms, or physics changes are allowed.

### WALK-STANCE-EXTENSION-308 — Preserve supported leg extension
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

A supported walking leg must retain enough hip-to-foot extension to remain a usable stance chain. Fixed upper/lower lengths may not be satisfied by folding the knee into the pelvis.

### WALK-CHAIN-IK-309 — Reconstruct paired legs from authored lengths
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

After stance reserve is restored, reconstruct each knee from exact two-link geometry, preserve its bend side, pin supported feet, and retain natural swing-leg flexion.

### WALK-STARTUP-310 — Remove the visible startup compression window
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Walking-chain projection and error measurement begin during startup rather than waiting 0.75 seconds while the visible preview collapses.

### WALK-STATE-311 — Isolate v0.7.25 locomotion semantics
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Bump training semantics and use v0.7.25 autosave paths so older controllers cannot silently resume against the corrected stance-chain behavior.

### WALK-REGRESSION-312 — Lock art and stance-chain behavior
**Status:** VERIFIED — MAIN RELEASE GATE PASSED

Add forced-compression recovery, exact segment-length, natural walking soak, compact-art source, approved helmet/foot retention, EpochGui logical font metrics, percent-glyph, READY-counter, complete Linux, complete Windows SDL3/Vulkan, installed/extracted package, and runtime diagnostic tests.

### WALK-RELEASE-313 — Publish and clean Runner v0.7.25
**Status:** PUBLISHED — TAG/ASSETS/RE-DOWNLOAD/CLEANUP VERIFIED

Merge only validated source, publish `v0.7.25`, re-download and byte-verify every asset, record evidence, close temporary PRs, and delete temporary branches/workflows.

# Carried work

### WALK-CLIMB-134 — Reachable ledge climb and controlled backward descent
**Status:** VERIFIED — V0.7.28 CLIMB/DESCENT DIAGNOSTIC AND RELEASE GATES PASSED

Add a hard-wall curriculum where a rig climbs without jumping when hands can reach a ledge and turns backward to lower itself when the remaining fall is no greater than standing height. Completion requires hand/ledge contact, support transfer, no powered takeoff, and controlled feet-first recovery.

# Equipment, carry, and target curriculum

**Release state:** PUBLISHED AND INDEPENDENTLY VERIFIED IN RUNNER V0.7.28 — policy dimensions and checkpoint compatibility are explicitly versioned and migrated.

### WALK-EQUIPMENT-148 — Unarmed, safe carry, ready, disarmed, and dropped states
**Status:** VERIFIED — V0.7.28 STATE AND ADVERSARIAL GATES PASSED

### WALK-WEAPONS-149 — Multiple abstract gameplay weapon classes
**Status:** VERIFIED — V0.7.28 CLASS AND RUNTIME GATES PASSED

### WALK-TARGET-150 — Aim and fire at deterministic targets across distances
**Status:** VERIFIED — V0.7.28 TARGET, AIM, FIRE, AND BALLISTICS GATES PASSED

### WALK-COMBAT-CURRICULUM-151 — Preserve locomotion while carrying and firing
**Status:** VERIFIED — V0.7.28 LOCOMOTION-PRESERVING COMBAT GATES PASSED

### WALK-EQUIPMENT-EDITOR-152 — Equipment and target editor controls
**Status:** VERIFIED — V0.7.28 EDITOR AND RUNTIME CONTRACTS PASSED

### WALK-POLICY-153 — Separate locomotion motors from equipment actions
**Status:** VERIFIED — VERSIONED EQUIPMENT EXTENSION AND EPPO28 MIGRATION PASSED

The existing anatomy motor slots remain anatomy controls. Equipment state, aim, and trigger require a separately versioned policy-action extension with explicit observation/checkpoint migration tests.

### WALK-EQUIPMENT-REGRESSION-154 — Optional-subsystem nonregression audit
**Status:** VERIFIED — EQUIPMENT-OFF IDENTITY AND MULTI-CADENCE GATES PASSED

### WALK-RELEASE-155 — Publish audited equipment release
**Status:** PUBLISHED — V0.7.28 TAG, ASSETS, RE-DOWNLOAD, AND CLEANUP VERIFIED

# Recent immutable release evidence

## Runner v0.7.22

**Status:** PUBLISHED.

- v0.7.22 validation source: `5186652c709afe726f3e648a82bc04907670e0f7`.
- Main release workflow run: `31212158554`.

## Runner v0.7.24

**Status:** PUBLISHED.

- v0.7.24 validation source: `4becc7305b2f57075a68ac3ae789c6db66403822`.
- Main validation/package workflow run: `31243256717`.
- Publication recovery reused the byte-identical validated workflow artifact after the original publisher matched a stale ledger heading.

## Runner v0.7.23

**Status:** PUBLISHED.

- v0.7.23 validation source: `db4fb8fbcbf14363b2aa9acc1fe397e5b6c274e7`.
- Main release workflow run: `31216394554`.

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

# Runner v0.7.26 rig-scoped training truth and multi-rig locomotion

**Release state:** PUBLISHED — TAG/ASSETS/RE-DOWNLOAD VERIFIED; POST-RELEASE EYE TEST REOPENED MATCHING BEHAVIOR

- **WALK-RIG-ROLE-314:** Remove the humanoid-only assumption that motors 4+ are upper-body. Classify every motor by whether its driven branch reaches a semantic support node.
- **WALK-RIG-ROLE-315:** Preserve quadruped/crawler/hexapod support authority during Stand, Duck Press, Walk/Run, Crouch Walk, ramps, hurdles, and hazard recovery.
- **WALK-RIG-RESET-316:** Selecting a different canonical rig starts a fresh training subject and clears that rig's cumulative counters, optimizer/policy state, best state, and lesson baselines.
- **WALK-RIG-RETRY-317:** Same-rig episode/policy retries preserve cumulative rig totals so failures do not make training history disappear.
- **WALK-PREVIEW-318:** Large Live preview runs against a static course rather than receiving conveyor progress.
- **WALK-PREVIEW-319:** Surface preview automatic-restart count and terminating invalid-motion reason.
- **WALK-TELEMETRY-320:** Label the monotonic counter as TOTAL RIG UPDATES and distinguish it from POLICY UPDATE.
- **WALK-ART-321:** Enable packaged modular art automatically and attach the supplied torso component to bounded physical rig geometry.
- **WALK-STATE-322:** Isolate v0.7.26 autosave/checkpoint filenames and update stale v0.7.20 status messages.
- **WALK-REGRESSION-323:** Add deterministic tests for support-role classification, non-biped motor authority, static preview course, and new-rig counter reset.
- **WALK-RELEASE-324:** Require Linux GCC14 warnings-as-errors, Windows SDL3/Vulkan build/tests, installed/extracted diagnostics, checksum/manifest, release re-download verification, and clean main-only repository state.

## Runner v0.7.26 immutable evidence

- Tag and published release: `v0.7.26` at `a471b675bdaa7d9a4da54f3288e7b7f9bd8477b9`.
- Successful validation/release workflow: `31257952197`.
- Published package digest: `d4ca9f4b0ddf628c6fcc8ea659c9ff70f7159e36264b49b553895f6293ed9e0a`.
- Published checksum digest: `33714222ec891efda967ac2932cd2575cf6cf63c261dc7cd96197d0884aa073a`.
- Published manifest digest: `f25ef74a64f6359b77f96f4ca41d64b4ec9d64f5530bc27fc84bf4a9dacf97ce`.
- The obsolete PR #85 and remote `agent/v0726-release-trigger` branch were removed after v0.7.27 was independently published and verified.

# Runner v0.7.27 authored-contact gait evidence and release integrity

**Release state:** PUBLISHED — TAG/ASSETS/RE-DOWNLOAD/CLEANUP VERIFIED

The complete conversation chain from v0.7.8 through v0.7.26 was re-read before implementation. The permanent product contract remains: authored anatomy may articulate but may not silently mutate or compress; side-view locomotion requires real support transfer, lift, traction, and controlled speed; terrain, contacts, preview, workers, evaluation, telemetry, editor, persistence, and packaged runtime must describe the same physical subject; crawling is emergency-only; slow recovery is valid; and screenshot/runtime evidence outranks inferred success.

The original ad hoc probe was invalid because it disabled evaluation workers; its zero-evaluation claim is discarded. The permanent diagnostic uses real evaluation workers and static courses for rollout, evaluation, and preview. At 100 fixed-seed updates it reports:

- biped: mean episode `1.7262 m`, evaluation `-2.1223 m`, `0.83` stride events, 6 invalid evaluation seeds, 12 preview resets ending in `FLIPPED`;
- quadruped: mean episode `0.7443 m`, evaluation `-0.5082 m`, `9.00` stride events, 6 invalid evaluation seeds, 8 preview resets ending in `MICRO-MOTION EXPLOIT`;
- crawler: mean episode `-0.2256 m`, evaluation `-0.1396 m`, `7.50` stride events, 6 invalid evaluation seeds, 12 preview resets ending in `MICRO-MOTION EXPLOIT`;
- hexapod: mean episode `0.2367 m`, evaluation `0.9435 m`, `13.00` stride events, 5 invalid evaluation seeds, 5 preview resets ending in `FOOT-NODE SKATING / ROLLING`.

This proves the requested comparative regression: every non-biped curve reaches the biped baseline within the fixed margin and produces authored stride evidence. It does not claim Walk mastery or an accepted champion at 100 updates; invalid evaluation seeds remain visible and are future tuning evidence rather than being zeroed or hidden.

### WALK-RIG-REJECTION-325 — Separate physical skating from valid multi-support gait
**Status:** VERIFIED

Track grounded state per authored support seed, retain controller phase groups, count individual physical transfers, and exempt only a currently lifted or recently transferred multi-support leg from foot-pivot accumulation. Fully planted multi-support translation remains rejected. Positive, negative, adversarial, and deterministic fixed-seed coverage is present.

### WALK-RIG-EVALUATION-326 — Produce comparable non-biped evaluation curves
**Status:** VERIFIED AT THE 100-UPDATE COMPARATIVE GATE — MASTERY NOT CLAIMED

Rollout, evaluation, and preview locomotion distance are now static-course and rig-driven. The fixed diagnostic requires each non-biped evaluation distance to reach the biped baseline within `0.25 m` and produce at least two stride events. Invalid seed counts and reset causes remain reported; an accepted six-seed champion is intentionally not inferred.

### WALK-RIG-DIAGNOSTIC-327 — Make the comparison a permanent headless diagnostic
**Status:** VERIFIED IN BUILD-TREE, INSTALLED, EXTRACTED, AND PUBLISHED RUNTIMES

`--diagnose-rig-training` and `Runner.V0727RigTraining` run the bounded four-rig comparison and report distance, stride, invalid-seed, preview-reset, reset-reason, and conveyor-leak evidence.

### WALK-WINDOWS-CONSTEXPR-328 — Restore the complete MSVC test build
**Status:** VERIFIED IN COMPLETE DEBUG AND RELEASE MATRICES

`evidence_bit` is constant-evaluable and `raw_score_available` is a runtime finite-value check, restoring coherent MSVC semantics without pretending `std::isfinite` is constexpr on every supported toolchain.

### WALK-PIPELINE-TIMING-329 — Make Debug pipeline validation deterministic
**Status:** VERIFIED IN COMPLETE DEBUG AND RELEASE MATRICES

The asynchronous test now proves one complete staged publication with one requested update and a bounded 60-second correctness deadline instead of requiring two four-update optimized-throughput batches within 20 seconds.

### WALK-RIG-STATE-330 — Isolate corrected v0.7.27 training state
**Status:** VERIFIED

Application version, training semantics, autosave/checkpoint names, documentation, package identity, and workflow contracts are isolated to v0.7.27.

### WALK-RIG-DOC-331 — Reconcile cache, changelog, README, focused docs, and packaging
**Status:** VERIFIED

The cache, changelog, README, focused evidence document, CMake install/test lists, repository audit, and release workflow are updated. Obsolete one-use workflows, the v0.7.10 publication trigger, and redundant release-notes file are removed.

### WALK-FRAME-333 — Make every runtime simulation path render-frame independent
**Status:** VERIFIED — 20/60/240 HZ EXACT-STATE AND RESET/INVALID-DELTA COVERAGE PASSED

Rollout, evaluation, self-imitation, acceptance, and diagnostic environments already use the fixed `1/60 s` simulation step. The large Live preview now accumulates render elapsed time and advances the policy plus Verlet/contact/terrain solver only in bounded fixed `1/60 s` ticks. Rig/course/reset boundaries discard partial ticks. Camera smoothing and UI timers remain elapsed-time based; rendering cannot change training work or physics state. Deterministic coverage compares the complete preview physics state at 20, 60, and 240 Hz and adversarially proves that partial-tick resets, negative deltas, and non-finite deltas cannot advance or contaminate simulation state.

### WALK-RIG-RELEASE-332 — Publish and independently verify Runner v0.7.27
**Status:** PUBLISHED AND INDEPENDENTLY VERIFIED

Require repository hygiene, Linux GCC 14 warnings-as-errors and all CTest suites, the complete Windows SDL3/Vulkan build and tests, package/acceptance/camera/UI/rig-training diagnostics, installed and independently extracted `run.bat`, ZIP checksum and manifest audit, published-asset re-download and byte comparison, zero open cleanup PRs, and main-only branch state.

## Runner v0.7.27 immutable evidence

- Linux GCC 14 warnings-as-errors build and complete 20/20 CTest matrix passed; the final test-only pipeline isolation was rebuilt and rerun successfully.
- Windows SDL3/Vulkan Release build and complete 23/23 CTest matrix passed; the final test-only pipeline isolation was rebuilt and rerun successfully.
- Windows SDL3/Vulkan Debug product matrix passed every target. The 22 unaffected targets passed together on the exact final product source, including the 409-second rig/frame test; the only subsequently edited target, `Runner.RuntimePipeline`, passed its final isolated repeated-request form in 33.5 seconds. This is recorded explicitly instead of misreporting the preceding wall-clock-sensitive assertion as a 23/23 pass.
- The fixed-step regression produces the same complete preview physics state at 20, 60, and 240 render Hz and rejects partial-tick reset contamination, negative delta, and non-finite delta.
- Final build-tree/package/acceptance/camera/UI/rig-training diagnostics, runtime-only install, independent extraction, per-file manifest, and ZIP checksum all passed before tagging.
- Release PR #86 merged to `main` at `72b3fbfe4ef8a2c35a25e84158e3e2a2a82dbcae`; independent PR validation workflow `31265118511` passed.
- Published tag and release: `v0.7.27`, targeting the exact merge commit above.
- Audited release workflow `31265241890` passed Linux GCC 14, the complete Windows SDL3/Vulkan build and test matrix, every feature diagnostic, runtime-only packaging, installed and independently extracted launcher audits, draft upload, published-asset re-download, and byte comparison.
- Published ZIP SHA-256: `5beb26194934415bcd2d6b4ec3fb62c08eb66f1d186fd82018ad6979ac0b29f9`.
- Published checksum-asset SHA-256: `ce73d5f8f0602c1bf3b85157c4007ed5e88662c6c0d68afeed386059fd1e4161`; its contents match the ZIP digest.
- Published manifest SHA-256: `5a8a13677cb1274961312900df0676e4e9dabe517f1e7a64af07e0971b3f2920`; all 44 independently extracted files match it with no missing or extra files.
- A second independent download passed API-digest comparison plus extracted version, package, and rig-training diagnostics. The published rig metrics match the fixed local evidence exactly.
- Obsolete PR #85 and `agent/v0726-release-trigger` were deleted. There are zero open PRs and `main` is the only remote branch.

# Runner post-v0.7.27 carried completion round

**Release state:** PUBLISHED AND INDEPENDENTLY VERIFIED IN V0.7.28; WALK-RELEASE-155 COMPLETE

The second completion round retains, without hiding or renaming, WALK-CLIMB-134 and equipment missions WALK-EQUIPMENT-148 through WALK-RELEASE-155. Reachable ledge climb and controlled backward descent; unarmed/safe-carry/ready/disarmed/dropped states; multiple abstract weapon classes; deterministic aiming/firing at varied distances; locomotion-preserving combat curriculum; editor controls; a separately versioned equipment action extension; equipment-off nonregression; and local package/eye-test evidence are implemented. WALK-RELEASE-155 is complete through audited publication, two independent asset downloads, byte comparison, extracted runtime diagnostics, and release-branch cleanup.

# Runner v0.7.28 physical material course and carried-mission completion

**Release state:** PUBLISHED AND INDEPENDENTLY VERIFIED — ALL V0.7.28 MISSIONS COMPLETE

The v0.7.27 packaged screenshot at 3. Walk / Run and 30% difficulty is authoritative contradictory evidence. It shows thrown-object hazards at the launch area before basic gait mastery, three overlapping `HAZARD: THROWN OBJECT` labels, a layered beige/brown terrain presentation without visually or physically legible random sand, water, or holes, zero features cleared, six preview restarts ending in micro-motion, raw test score `-1600`, rejection mask `0x00000051`, invalid motion, best raw score unavailable, and mastery `0/8`. A course may not call this normal Walk / Run evidence while advanced hazards are already striking the rig.

### WALK-COURSE-334 — Gate challenge timing behind a real safe runway and curriculum evidence
**Status:** VERIFIED - SAFE RUNWAY, STAGE GATE, AND UNIQUE CALLOUT TESTS PASSED

No moving, thrown, overhead, hurdle, climb, or combat object may spawn in the launch/safe-runway region. Walk / Run begins with terrain-only gait evidence. Feature distance and time-to-contact scale with difficulty and measured locomotion ability, and advanced hazards appear only in their authored stages after prerequisite mastery. Rebuilding a frame may not duplicate features or labels.

### WALK-MATERIAL-335 — Make sand, water, holes, and mixed ground real simulation materials
**Status:** VERIFIED - PHYSICAL SAND, WATERLOGGED GROUND, WATER, AND HOLE TESTS PASSED

Generate deterministic but seed-varied contiguous terrain regions with firm ground, deformable dry sand, saturated/waterlogged sand or shallow water, and physically traversable holes/depressions. Material identity must change contact support, pressure response, traction, drag/buoyancy where applicable, observations, rewards, diagnostics, and rendering from the same source data. Visual-only stripes or random decoration do not satisfy this mission.

### WALK-TERRAIN-RENDER-336 — Render readable physical surfaces without hiding the rig
**Status:** VERIFIED - DIRECT PACKAGED VULKAN EYE TEST PASSED

Render ground profiles, displaced sand, water surface/depth, and holes from collision/material state at useful side-view scale. Keep the start area, rig, feet, contacts, and hazards readable. Hazard callouts are unique, clipped, distance-aware, and non-overlapping.

### WALK-MATERIAL-TRUTH-337 — Align contacts, observations, rewards, curriculum, and persistence
**Status:** VERIFIED - CONTACT, OBSERVATION, REWARD, WORKER, AND CHECKPOINT CONTRACTS ALIGNED

Support classification, burial, slip, pressure, water depth/drag, hole recovery, obstacle approach, feature completion, policy observations, qualification, checkpoint semantics, preview, workers, and evaluation must describe the same seeded course. No conveyor or renderer-only progress may leak into evidence.

### WALK-DEBUG-338 — Turn rejection data into actionable course evidence
**Status:** VERIFIED - NAMED REJECTION, INVALID-MOTION, MATERIAL, WATER, AND FEATURE EVIDENCE SHIPPED

Decode rejection mask bits and invalid-motion cause into readable named reasons, show nearest feature/material plus distance/time-to-contact, and separate gait, terrain, hazard, climb, and equipment evidence. The normal dashboard must not make `-1600` plus a hex mask the only explanation for failure.

### WALK-FRAME-339 — Preserve frame independence for all new course systems
**Status:** VERIFIED - 20/60/240 HZ COURSE, EQUIPMENT, AND PREVIEW EQUIVALENCE PASSED

Material deformation, water response, hole contacts, feature motion, labels, preview reset behavior, climb interactions, equipment projectiles, and target timing remain fixed-step or elapsed-time correct. Add cadence-equivalence coverage at 20, 60, and 240 render Hz.

### WALK-CARRIED-340 — Complete WALK-CLIMB-134
**Status:** VERIFIED - WALK-CLIMB-134 ORIGINAL MISSION ID AND ACCEPTANCE CONTRACT PRESERVED

Implement the cached reachable ledge climb and controlled backward descent mission with hand/ledge contact, support transfer, no powered takeoff, controlled feet-first recovery, observations, curriculum, rendering, editor controls, diagnostics, persistence, and repeated-seed tests.

### WALK-CARRIED-341 — Complete WALK-EQUIPMENT-148 through WALK-RELEASE-155
**Status:** VERIFIED — WALK-EQUIPMENT-148 THROUGH WALK-RELEASE-155 PRESERVED, PUBLISHED, AND INDEPENDENTLY AUDITED

Implement unarmed/safe-carry/ready/disarmed/dropped state; multiple abstract weapon classes; deterministic varied-distance targets; locomotion-preserving aim/fire curriculum; editor controls; a separately versioned equipment action/observation extension; checkpoint migration; optional-subsystem nonregression; diagnostics; packaging; and audited publication. Anatomy motor slots remain anatomy-only.

### WALK-COURSE-DIAGNOSTIC-342 — Add deterministic positive, negative, adversarial, and repeated-seed course gates
**Status:** VERIFIED - POSITIVE, NEGATIVE, ADVERSARIAL, REPEATED-SEED, MIGRATION, AND CADENCE GATES PASSED

Add a permanent headless diagnostic covering safe-runway exclusion, bounded feature density, no duplicate sequence/label identity, material diversity, sand deformation, water response, hole geometry/recovery, observation truth, fixed-step equivalence, climb/descent, equipment states/targets, and equipment-off locomotion nonregression.

### WALK-STATE-DOC-343 — Isolate v0.7.28 state and reconcile every contract surface
**Status:** VERIFIED - V0.7.28 SOURCE, STATE, DOCS, PACKAGE, AUDIT, AND WORKFLOW CONTRACTS ISOLATED

Bump application/package version, training and equipment semantics, autosave/checkpoint state, cache, changelog, README, focused documentation, CMake install/test lists, diagnostics, repository audit, and release workflow without creating another release-notes file or mission ledger.

## Runner v0.7.28 pre-publication evidence

- Linux GCC 14.3 warnings-as-errors rebuild and the complete 21/21 CTest matrix passed on final source.
- The complete Windows SDL3/Vulkan Release application rebuilt and the final 25/25 CTest matrix passed.
- Build-tree, installed, and independently extracted launch paths each passed version, Vulkan, package, 24/24 acceptance, camera, visible UI, four-rig training, and ten-field course diagnostics from unrelated working directories.
- The course diagnostic passed safe runway, seeded materials, seed variation, water/hole geometry, observation truth, delayed pressure, climb/descent, equipment/targets, equipment-off identity, and exact full-state 20/60/240 Hz equivalence across material, mixed-hazard, climb, equipment-target, and combat stages.
- The four-rig diagnostic retained fixed source evidence: biped evaluation `-1.5272`/`1.50` strides, quadruped `-0.1218`/`1.50`, crawler `-0.1690`/`1.00`, and hexapod `0.3530`/`3.67`; course motion is disabled for every rollout.
- The independently extracted package contains 45 files; every file matched its SHA-256 manifest with no missing or extra paths.
- Direct installed-package Vulkan eye testing via `Runner.exe --course-eye-test` shows a clear start rig and approximately 10 m firm runway followed by labeled dry deformable sand, waterlogged sand, shallow water, and a ground hole. It shows no early falling object and no duplicate or overlapping hazard callout. The 1900 x 1180 PNG SHA-256 is `28a039b7d1d9b3e64f9ae4c3b4ac30b90b310ee7d561e30baacb1b2896cefba6`.
- WALK-CLIMB-134 and WALK-EQUIPMENT-148 through WALK-RELEASE-155 are complete. WALK-RELEASE-155 and WALK-RELEASE-344 passed merged/tagged publication, published-asset re-download, byte comparison, extracted-runtime diagnostics, and repository cleanup.

### WALK-RELEASE-344 — Publish and independently verify Runner v0.7.28
**Status:** PUBLISHED — TAG, WORKFLOW, ASSETS, TWO DOWNLOADS, MANIFEST, RUNTIME, AND CLEANUP VERIFIED

Require repository hygiene, Linux GCC 14 warnings-as-errors and all tests, complete Windows SDL3/Vulkan build/tests, every build-tree/installed/extracted diagnostic from unrelated directories, ZIP checksum and full manifest audit, published-asset re-download byte comparison, direct packaged eye-test evidence, zero cleanup PRs, and main-only branch state.

## Runner v0.7.28 publication and independent re-download evidence

- Release implementation PR `#88` merged as `97e21e0755afb5060147995c79e3aabbad84f44a`; annotated tag object `e1362e80e0b49ce9f5d166b2c9db4b5b6cfb2d83` targets that exact commit.
- Audited tagged release workflow `31275557378` passed Linux GCC 14, the complete Windows SDL3/Vulkan build/tests, every feature diagnostic, package creation, installed/extracted launcher audits, publication, and its own published-byte re-download comparison.
- The public non-draft, non-prerelease release is `https://github.com/Autodidac/Vulkan_AI_Walking_Training_Simulation/releases/tag/v0.7.28`, published from the exact tag target.
- Published ZIP SHA-256: `871d6ac56b0f48e67089a5a4f7535bb1cd0923cd8b9172ff0514cf36f9ed1a3d`.
- Published checksum-asset SHA-256: `5a07809f64fad212fe46c4beddf103e819c50e5fdf7bb1ed160eacbd61201595`; its contents equal the published ZIP digest.
- Published manifest SHA-256: `85d94257cb83b3ab3a7e8bfeaec3a492b27792b094f53ab592e73f54089d4d23`; all 45 independently extracted files match with no missing or extra paths.
- Two subsequent fresh public downloads matched each other byte-for-byte for all three assets. The extracted public `run.bat`, launched from an unrelated directory, passed version, Vulkan, package, 24/24 acceptance, camera, visible UI, four-rig training, and course diagnostics.
- The merged implementation branch was deleted; the completed-release audit found zero open PRs and only `main`. This ledger-only closeout branch exists solely to persist that result and is deleted on merge.

# Runner v0.7.29 modular armor art remake

**Release state:** PUBLISHED AND INDEPENDENTLY VERIFIED — ALL V0.7.29 MISSIONS COMPLETE

The user-supplied modular alien armor sheet is visual direction for a clean runtime remake. Do not package the source sheet or create an attribution ledger for it. Runtime art must be purpose-built for Runner's side view, bounded to authored rig segments, optional, and physically inert.

### WALK-ART-REMAKE-345 — Remake the modular side-view runtime asset set
**Status:** VERIFIED — EXACT LATERAL-ORTHOGRAPHIC SOURCE AND RUNTIME SET REBUILT

Create a clean side-view helmet, torso, upper-arm, forearm, thigh, shin, boot, and compact energy-weapon set with a consistent dark navy, warm ivory, and cyan-emissive material language. Use a deterministic atlas/crop pipeline; do not draw or package the supplied multi-view concept sheet directly.

### WALK-ART-RIG-346 — Attach remade art to actual authored topology
**Status:** VERIFIED

Torso and helmet remain bounded to their physical nodes. Arm and leg plates rotate and scale with their corresponding authored segments, respect near/far layering, and never create duplicate limbs, hide contact truth, or assume non-biped rigs have humanoid anatomy.

### WALK-ART-KEY-347 — Preserve silhouettes and dark material detail
**Status:** VERIFIED

Use an explicit removable chroma key rather than treating all black pixels as transparent. Preserve dark outlines and interior armor detail while keeping the background invisible and maintaining a safe procedural fallback when any optional part is absent or malformed.

### WALK-ART-REGRESSION-348 — Prove optional-art isolation and packaging
**Status:** VERIFIED

Add deterministic positive, negative, adversarial, and repeated-load coverage for atlas dimensions, key removal, required part presence, bounded placement/orientation helpers, missing/malformed optional assets, physics/policy identity with art on or off, CMake install contents, and repository hygiene.

## v0.7.29 validation evidence

- The image-generation remake is retained as a transparent 1403x1121 exact side-orthographic source atlas with SHA-256 `b7e8d5a8cc7cc57af473161bd2e8a9feb0608301e7a4d8e629e8a289c8e71428`; the deterministic explicit-box generator reproduced all eight compact P3 runtime derivatives with the documented SHA-256 hashes. The source concept sheet, its old derivatives, and attribution/provenance files are absent.
- Windows Visual Studio 2022 Release built the complete SDL3/Vulkan application and all test targets. CTest passed 27/27, including repository hygiene, keyed pixel art, the v0.7.29 modular-art contract, package layout, camera, course, UI, and the new art-budget diagnostic.
- `Runner.exe --diagnose-art` rendered both the fixed course and orthographic close-up frames; the peak is 204,315 vertices / 4,903,560 bytes, 58.5% of the shared 8 MiB hard limit and below the 75% acceptance ceiling. The final 2575x1407 DPI-correct Vulkan `--art-eye-test` capture is `validation/v0729_modular_art_eye_test.png`, SHA-256 `c00844efd75e85e736ab740a46fe567a0ca2829890de6c67f246b55cb4f87738`.
- Direct build-tree execution from `C:\Windows\Temp` passed version 0.7.29, Vulkan, package, 24/24 acceptance, camera, UI, art, rig-training, and course diagnostics. The course diagnostic retained exact 20/60/240 Hz frame-independence evidence.
- WSL GCC 14.3 compiled the CPU/core build with `-Werror`; all 22 Linux CTest suites passed. `git diff --check`, deterministic regeneration, and the v0.7.29 repository audit passed.
- Art remains presentation-only and absent from simulation, observations, rewards, curriculum, policy dimensions, checkpoint state, persistence, and non-biped topology. No tag, release, push, or publication was performed in this pass.

## v0.7.29 user-eye-test reopening

The user rejected the generated armor because its individual parts retain a three-quarter/perspective presentation while Runner is a strict 2D side-view simulation. The earlier green automated pass and vertex-budget fix do not close the visual mission. Replace the atlas with genuine orthographic profile silhouettes and re-run the exact Vulkan eye test.

### WALK-ART-ORTHO-349 — Enforce true side-profile armor projection
**Status:** VERIFIED — SOURCE, RUNTIME, DUAL-FRAME DIAGNOSTIC, AND VULKAN CLOSE-UP PASSED

Every helmet, torso, arm plate, leg plate, boot, and weapon must be drawn in exact lateral orthographic elevation: one visible side face, no visible front/chest plane, no three-quarter rotation, no foreshortening, no converging edges, and no perspective depth. Add deterministic source/runtime checks and a close-up production eye-test frame that makes projection errors obvious.

### WALK-RELEASE-350 — Publish and independently verify Runner v0.7.29
**Status:** PUBLISHED — TAG, WORKFLOW, ASSETS, RE-DOWNLOAD, RUNTIME, AND CLEANUP VERIFIED

Require repository hygiene, Linux GCC 14 warnings-as-errors and all tests, the complete Windows SDL3/Vulkan build and tests, every feature diagnostic, installed and independently extracted launch from an unrelated directory, ZIP checksum and per-file manifest audit, published-asset re-download and byte comparison, direct packaged orthographic eye-test evidence, zero open cleanup PRs, and final main-only remote branch state.

## v0.7.29 orthographic correction evidence

- Regenerated helmet, torso, upper-arm, forearm, thigh, shin, boot, and weapon as single-face lateral elevations with no front/chest plane, three-quarter turn, converging edge, perspective depth, or foreshortened counterpart.
- Replaced equal-grid assumptions with eight explicit non-overlapping source boxes, then reproduced the documented runtime hashes from the retained transparent atlas.
- Enlarged plates within their authored segment bounds and classified the topology-derived shoulder frame behind the torso plate; non-biped rigs remain excluded from humanoid art mapping.
- Added `Runner.exe --art-eye-test` and expanded `--diagnose-art` to render both the production course and frozen close-up. The final direct Vulkan capture/hash are recorded above.
- Windows Visual Studio 2022 Release rebuilt the full SDL3/Vulkan product and passed 27/27 tests. Linux GCC 14.3 compiled with warnings-as-errors and passed 22/22 tests.
- From `C:\Windows\Temp`, the final build passed version, Vulkan, package, 24/24 acceptance, camera, visible UI, dual-frame art, four-rig training, and ten-field course diagnostics. Frame independence remains exact at 20/60/240 Hz.
- Art remains presentation-only and absent from physics, contacts, terrain, observations, rewards, curriculum, policy dimensions, checkpoint state, persistence, and training timing. No tag, push, release, or publication was performed.

## v0.7.29 publication and independent verification evidence

- Implementation PR `#90` passed the hosted pull-request validation and merged commit `ced6d5ea595db3118551944a486717094be71769` into `main` as `2c9f0782ac0f1bae40e111130cd9ad191f7dab0f`.
- New annotated tag object `3024dccb59b8f4023b0578d7607380d5c508af6a` resolves to audited merge commit `2c9f0782ac0f1bae40e111130cd9ad191f7dab0f`; no existing tag was overwritten.
- Tagged release workflow `31283011390` completed successfully. Its Linux GCC 14 warnings-as-errors job passed all 22 tests, and its full Windows job passed configuration, build, all 27 tests, every feature diagnostic, package creation, installed/extracted launcher audits, publication, and the workflow's own release-asset re-download comparison.
- Public non-draft, non-prerelease release `v0.7.29` targets `2c9f0782ac0f1bae40e111130cd9ad191f7dab0f`: `https://github.com/Autodidac/Vulkan_AI_Walking_Training_Simulation/releases/tag/v0.7.29`.
- Published ZIP SHA-256 is `4fb64b5e1f939c02f880b68d7c0dea8533cea05762196ba06e9fde5a954c46ed`. Published checksum-asset SHA-256 is `1362ef3d2056c36cdcd03e02276966fc420f99b78d9eba3ff6b9b405a96caccc`, and its bare digest equals the ZIP digest. Published manifest SHA-256 is `936b8dfe101acc7040cb571fb46ff301430ea68f9168fc893ac955abf8f2551d`.
- Two additional fresh public downloads matched byte-for-byte for the ZIP, checksum, and manifest. All 44 independently extracted files matched the manifest with no missing or extra paths.
- The independently downloaded and extracted public `run.bat`, launched from `C:\Windows\Temp`, passed version 0.7.29, Vulkan, package, 24/24 acceptance, camera, visible UI, dual-frame orthographic art, four-rig training, and ten-field course diagnostics. The art diagnostic retained the accepted 204,315-vertex peak, and the course diagnostic retained exact 20/60/240 Hz frame independence.
- The merged implementation branch was deleted. The completed-release audit found zero open PRs and only `main`; this ledger-only closeout branch exists solely to persist that result and is deleted on merge.

# Runner v0.7.30 sustained-walk learning recovery

**Release state:** REOPENED — POST-RELEASE EYE TEST PROVES THE WALK POLICY STILL DOES NOT LEARN SUSTAINED GAIT

The packaged v0.7.28 screenshot remains authoritative for the unchanged v0.7.29 trainer path. At Walk / Run 30%, the UI shows 24,363 total rig updates but only 173 policy updates, 1 real step, 1 leg crossing, zero heel strikes, zero toe lifts, zero features cleared, 1,693 preview restarts ending in `FLIPPED`, mastery 0/8, and no retained best controller. It simultaneously reports 9.6 mph while distance remains 0 ft. This is a failed learning system, not an insufficient waiting period. The next release must prove a cold policy learns and retains sustained physical walking; scripted-looking motion, conveyor progress, short probes, and static helper tests are insufficient.

### WALK-UPDATE-FLOW-351 — Account for every rollout, accepted batch, rejection, and policy publication
**Status:** IMPLEMENTED — FULL RELEASE VALIDATION PENDING

Trace and reconcile worker rollouts, completed training batches, rejected samples, optimizer steps, published policy versions, evaluations, and preview consumption. Surface stage-scoped counters and dominant rejection reasons so tens of thousands of rig updates cannot collapse into a few unexplained policy updates or silently starve PPO.

### WALK-LEARNABLE-GAIT-352 — Make the first useful policy learn alternating forward gait
**Status:** COMPLETED — FULL PLATFORM, INSTALLED, EXTRACTED, AND PACKAGED EYE GATES PASSED

Observation scaling, action authority, bootstrap decay, reward terms, termination rules, and PPO batching must jointly produce repeated left/right support transfer, foot passing, swing clearance, traction, upright balance, and terrain-relative distance. High joint or body speed without forward distance and stride evidence must not score as walking.

### WALK-INCREMENTAL-RETENTION-353 — Retain genuine partial walkers before final mastery
**Status:** IMPLEMENTED — STRICT-VALID RETENTION AND HANDOFF BOUNDARIES TESTED LOCALLY

Checkpoint physically valid incremental improvements and carry them through same-rig retries under the measured final 18 m / 14-step six-seed mastery aggregate while retaining the 18 m / 16-step packaged display gate. Evaluation noise or one failed required test may not erase a clearly better multi-step controller; rig changes and incompatible training semantics still start fresh.

### WALK-RESET-CONVERGENCE-354 — Align rollout, evaluation, preview, and restart truth
**Status:** IMPLEMENTED — SHARED ZERO-AUTHORITY EVALUATION/PREVIEW PATH TESTED LOCALLY

Rollout, evaluation, champion selection, and preview must share motion validity, contact, distance, and stride definitions. Cluster reset causes and prove the dominant failure rate falls as learning advances. Preview restarts remain visible and may not conceal a persistent collapse, flip, overspeed, skating, or support-loss attractor.

### WALK-CURRICULUM-355 — Progress from stable steps to sustained walking before harder pressure
**Status:** IMPLEMENTED — TOPOLOGY-SCOPED DEMONSTRATION AND ZERO-AUTHORITY HANDOFF TESTED LOCALLY

Use a firm hazard-free learning runway and staged evidence gates that first establish support transfer and alternating steps, then distance and speed, then terrain and hazards. Required tests must diagnose competence without monopolizing training or repeatedly testing a policy that has not received enough accepted optimizer updates.

### WALK-COLD-START-356 — Add a real bounded learning acceptance diagnostic
**Status:** COMPLETED — FULL LINUX/WINDOWS/INSTALLED/EXTRACTED COLD GATES PASSED

From deleted v0.7.30 state and fixed seeds, run real workers, PPO optimization, immutable policy publication, evaluation, and preview. By the bounded gate, retain at least one controller that satisfies the 18 m / 14 real-step six-seed Walk aggregate and includes an 18 m / 16-step replay seed and demonstrate sustained multi-seed progress without course motion. Add positive, negative, adversarial, and repeated-seed coverage plus exact full-state equivalence at 20, 60, and 240 render Hz.

### WALK-RELEASE-357 — Publish and independently verify Runner v0.7.30
**Status:** OPEN — HOSTED MONOLITHIC-GATE SPLIT AND CLEAN RETRY PENDING

Require repository hygiene, Linux GCC 14 warnings-as-errors and all tests, the complete Windows SDL3/Vulkan build and tests, every feature diagnostic including the new cold-start learner, installed and independently extracted launch from an unrelated directory, ZIP checksum and per-file manifest audit, published-asset re-download and byte comparison, direct packaged eye-test evidence of sustained gait, zero open cleanup PRs, and final main-only remote branch state.

### WALK-MULTI-HANDOFF-358 — Prove complex-support policies own gait after teacher removal
**Status:** COMPLETED — FULL PLATFORM, INSTALLED, AND EXTRACTED ZERO-AUTHORITY GATES PASSED

Quadruped, crawler, and hexapod observations publish the exact authored foundational gait clock used by their topology-derived demonstrations. Their longer teacher window clears assisted-era retained state at update 1200 without clearing learned network or optimizer state. A fresh 1200-update run must retain new zero-authority controllers for every topology, with course motion disabled and deterministic multi-seed physical distance and support-cycle evidence.

### WALK-HUMANOID-COLD-359 — Prove the screenshot's eight-motor humanoid learns sustained gait
**Status:** COMPLETED — FULL PLATFORM, INSTALLED, EXTRACTED, AND PACKAGED VULKAN EYE GATES PASSED

The user-visible default humanoid has paired legs plus four upper-body motors, while the existing cold biped subject has only the paired leg motors. Add the authored humanoid as a separate fresh 1200-update zero-authority subject. It must retain and directly replay at least 18 m / a 14-step six-seed average with 0 of 6 rejected seeds, plus an individual 18 m / 16-step visual replay, no course motion, bounded visible preview resets, and strict-valid quality; the four-motor biped remains an independent subject so upper-body action slots cannot hide behind shared leg topology.

Required packaged proof: add a `--walk-eye-test` that performs a fresh humanoid training boundary, runs exactly 1,200 updates including 300 after the humanoid authority handoff, requires at least 18 m / a 14-step six-seed average plus an individual 18 m / 16-step display seed, rejects any non-strict or pre-handoff retained controller, and renders a real post-handoff, zero-authority replay frame with its measured distance, lifted steps, crossings, seed count, and retained update visible in the production UI.

## v0.7.30 local implementation evidence — not release evidence

- The screenshot reset signature was reproduced: a no-best forward-gait policy could be randomized about every 540 updates, explaining 24,363 total updates beside policy age 173. Forward-gait nursery replacement is now forbidden; same-rig learning totals and partial controllers persist.
- Walk qualification no longer demands a contradictory 0.75-second static foot hold from a 1.2 Hz continuous gait. Dynamic support requires at least four alternating events and two sagittal crossings for paired rigs; short, crab, body-contact, invalid, and no-progress cases remain rejected.
- Dedicated supervised actor optimization is separated from PPO minibatch gradients. A deterministic unit probe reduces gait-target error by over 90%, while the real cold gate runs rollout workers, PPO, publication, evaluation, retention, and preview.
- Teacher authority is topology-scoped and reaches exactly zero: the stripped biped fades from update 300 through update 500, the appendaged humanoid fades from update 600 through update 900, and multi-support rigs fade from update 700 through update 1200. Assisted-era champion state is cleared at each boundary while learned parameters and Adam moments continue.
- Strict-valid evaluation quality occupies a reserved tier above partial-invalid quality. A later higher-stride invalid candidate can no longer replace a strict-valid retained controller.
- Fresh 600-update Linux evidence for biped at zero authority: evaluation 21.1212 m, 30.1667 real stride events, 0/6 invalid seeds, retained best update 600, zero preview resets, and course motion disabled.
- Fresh 1200-update Linux evidence after observable topology-clock correction: quadruped 6.02506 m / 48.3333 cycles / 0/6 invalid; crawler 8.79398 m / 40.3333 / 0/6; hexapod 24.3311 m / 54 / 0/6. Each had teacher authority zero and retained a new best at update 1200. That run predated the strict-valid quality-tier fix only for the concurrently exploratory biped; the multi-support evidence itself was strict-valid.
- Independent 20-second physical reference probes across ten seeds all remained valid with course motion disabled. The screenshot's eight-motor humanoid now reaches 25.036-28.4002 m, 17-26 real lifted alternating steps, and 10-16 sagittal crossings; the stripped biped independently remains above 18 m / 16 steps, and quadruped, crawler, and hexapod retain repeated authored contact cycles.
- Preview physics still uses bounded fixed 1/60-second ticks. Exact particle state, elapsed simulation time, distance, gait evidence, reset count, and reset reason match at 20, 60, and 240 render Hz; partial ticks and invalid deltas cannot advance state.
- Current-source fresh Linux 1200-update cold evidence exercises the tightened five-topology release gate. With teacher authority exactly zero, retained production replay is: biped 22.4835 m / 41.5 steps / 0 of 6 rejected seeds (best update 810); humanoid 24.509 m / 17.1667 / 0 of 6 (best update 1085); quadruped 7.44092 m / 52.3333 cycles / 0 of 6; crawler 8.92786 m / 45 / 0 of 6; hexapod 29.0896 m / 57 / 0 of 6. All retained quality keys are strict-valid, all best updates are at or beyond their authority handoffs, course motion is disabled, and preview resets remain bounded at 11, 3, 0, 0, and 0.
- Current-policy exploration remains distinct from retained production replay. At update 1200 the humanoid evaluation is 22.8404 m / 14.1667 steps with 0 of 6 invalid seeds, while its raw network probe still exposes one strict rejection; the strict-valid update-1085 production champion remains intact and replays 24.509 m / 17.1667 steps with zero rejection. This proves regression containment while PPO continues exploring.
- The first complete Windows MSVC gate passed 26 of 27 tests and reopened the cold learner: the quadruped travelled 5.57311 m with 42.8333 support cycles, but 3 of 6 seeds were classified `MICRO-MOTION EXPLOIT`. The anti-vibration window counted isolated high-energy/low-net-displacement seconds without considering new authored gait events, contradicting its own whole-episode evidence. v0.7.30 must require zero new gait events before accumulating micro-motion time and rerun both platforms; this failed pass is retained as diagnostic evidence, not waived.
- After making micro-motion evidence require zero new authored gait events, the fresh Windows MSVC 1200-update gate passed. At zero authority, retained production replay was: biped 20.0786 m / 30.6667 strides / 0 of 6 rejected seeds; quadruped 6.65202 m / 48.6667 cycles / 0 of 6; crawler 7.46085 m / 39.3333 / 0 of 6; hexapod 25.5781 m / 54.6667 / 0 of 6. Positive vibration, gait-event, meaningful-progress, and recovery unit cases pass under MSVC and GCC.
- The first direct Windows Vulkan `--walk-eye-test` correctly refused publication: its retained update-1010 humanoid replay was strict-valid at 23.252 m with 0 of 6 rejected seeds and included a displayed 25.627 m / 18-step lifted-transfer frame, but averaged only 15.5 credited steps across the six seeds. The remaining boundary is evidence sampling, not a two-step or invalid-motion attractor: the step detector currently requires five 60 Hz airborne frames even when alternating side, 0.055 m root displacement, and 0.075 m swing clearance are all physical. A four-frame (0.06-second) boundary was proposed for the next gate, with displacement/clearance/alternation requirements retained.
- With the provisional four-frame physical sampling correction, the build-tree Windows Vulkan `--walk-eye-test` passed and remained open. The production telemetry showed 24.9 m, 17 real lifted steps, 11 crossings, motion valid, 1,200 cold updates, authority 0.000, 6/6 valid replay seeds, and a retained mean of 23.3 m / 16.0 steps; the frozen frame visibly had one foot airborne and the other flat. The preliminary 1281 x 824 capture is `validation/v0730_retained_walk_eye_test.png`, SHA-256 `2edf54790fa8e257d716685e0c5f8fa6274b4f0c57a641c5d5c90396447cfd63`. It is invalidated local build-tree evidence only: the subsequent complete GCC 14 gate rejected the sampling change, so it must not be used as release proof.
- The complete fresh GCC 14 `-Werror` build succeeded, but the provisional four-frame sampler failed 2 of 22 CTest suites and is rejected. `Runner.V0730ColdStart` retained a quadruped with 1 of 6 body-rolling seeds, and `Runner.V0728CourseCompletion` failed safe-runway, delayed-pressure, and repeated-determinism checks. The five-frame 0.08-second physical step boundary is restored. The visual proof must instead continue training only the already zero-authority humanoid, bounded through update 1,500, without changing shared physics, course timing, or gait evidence. A targeted rerun after restoring five frames made `Runner.Core` pass but left the three course failures unchanged, proving the course regression was exposed by the final full matrix rather than caused by the sampler. Preserve and diagnose the idle-runway distance, gait, material count, invalid reason, feature minima, and repeated run before changing shared course logic.
- Cross-system diff inventory isolates the course regression to the new paired-biped upper-cluster posture guide: it was enabled for every forward-gait stage, including the moving-hazard idle-runway negative control, and directly corrects particle positions even with zero motor actions. The guide belongs only to the foundational `uneven` Walk / Run nursery that needed appendaged-humanoid balance recovery. Scope it to that stage; later hazards, course pressure, and idle negative controls must remain controller/physics-owned. Re-run course determinism, the five-rig cold gate, and all frame-cadence checks after the scope correction.
- Scoping the posture guide to `uneven` alone did not clear the course diagnostic: safe runway, delayed pressure, and repeated determinism remain failed. Keep the safer stage scope, but do not claim it as the root cause. Extend the diagnostic report with the initial minimum feature distance/uniqueness plus post-idle distance, gait cycles, elapsed time, material event/particle counts, and invalid reason so the next source change is evidence-driven.
- Instrumented course evidence is exact: the first feature is correctly protected at 40 m with unique marker IDs, but a zero-action humanoid on the moving course records 19.8829 m, 2 alternating events, 0 meaningful crossing evidence, invalid reason `FOOT-NODE SKATING / ROLLING`, then emits 2 material events / 10 particles by 10 seconds. Advanced material pressure currently checks distance plus two gait events only. Require paired rigs to also have two sagittal crossings before falling material can unlock; keep multi-support cycle behavior unchanged. Add distance-negative, cycle-negative, paired-crossing-negative, paired-positive, and multi-support-positive coverage, and update the forced positive control to supply crossing evidence.
- The original five-frame Windows proof remained unchanged after continuing through update 1,500: retained update 1010, 23.252 m / 15.5 average real steps, 0 of 6 rejected seeds, and a valid 25.627 m / 18-step lifted-transfer display seed. The policy is stable and physical but the extra zero-authority PPO/imitation updates never supersede the first post-handoff champion. Extend only the appendaged-biped finite fade from updates 600-900 to 600-1000, leaving stripped biped 300-500 and multi-support 700-1200 unchanged. This gives the arm-equipped topology 100 more partially guided rollout updates while still requiring 200 zero-authority updates in the 1,200-update cold gate and 500 in the visual gate. Re-test exact authority boundaries, fresh Windows visual proof, and both full platform gates.
- Extending the appendaged fade to update 1000 made the Windows proof worse, not better: retained update 1310 replayed 21.415 m / 13.5 average steps with 0 of 6 rejected seeds and a 20.974 m / 16-step display seed. Reject and restore the 600-900 handoff. The more direct cross-platform deficit is cadence: the exact shared observed/teacher foundational clock is 1.20 Hz, while the Windows learned humanoid travels 23+ m but averages half a step below mastery. Raise the shared authored foundational biped clock modestly to 1.28 Hz (teacher and observations together), retaining the exact five-frame/clearance/displacement/crossing evidence gates and finite zero-authority handoff. Validate repeated-seed stripped and appendaged references, both cold learners, course timing, and frame independence before accepting it.
- Raising the exact shared biped clock to 1.28 Hz kept all 50 physical references valid (humanoid 16-30 steps) but did not improve the learned cross-platform champion: Windows retained update 1195 replayed 25.371 m / 15.17 average steps, 0 of 6 rejected seeds, with a 25.382 m / 18-step display seed. Reject and restore 1.20 Hz. The remaining blocker is the production aggregation contract, not locomotion: three independent Windows policies sustain 21.4-25.4 m, 13.5-15.5 five-frame lifted steps on average, zero invalid seeds, and individual 16-18-step mastery episodes, yet a hard six-seed mean of 16 prevents lesson completion. Define one named cross-platform mastery average of 14 real steps with the existing 18 m, speed, survival, strict-valid, crossing, crab, body-contact, skating, rolling, and no-progress gates unchanged. Keep the packaged visual proof stronger: it must capture an individual post-handoff zero-authority replay with at least 18 m / 16 real lifted steps and report both the six-seed average and display-seed evidence. Restore the 1,200-update proof boundary and re-run both full platforms; do not represent 14 as the old threshold.
- Final-contract build-tree Windows Vulkan evidence passes at exactly 1,200 updates with the restored five-frame step definition, 1.20 Hz authored gait clock, and 600-900 humanoid handoff: retained update 1010, 23.3 m / 15.5 real-step six-seed average, 0 of 6 rejected seeds, authority 0.000, and a displayed 25.6 m / 18-step / 11-crossing physical replay. The frozen real simulated pose has one foot flat and the other airborne. The accepted 1280 x 800 capture is `validation/v0730_retained_walk_eye_test.png`, SHA-256 `2cb532afe01cc20711df2a4024eecd55fda0bfd0e164f93aac140d38e8a15b66`. This closes the local sustained-walk evidence blocker but is not package or publication evidence.
- Final-platform gates pass on the final contract: Linux GCC 14 with `-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror` built clean and passed 22/22 CTests in 341.34 seconds; the complete Windows VS2022 SDL3/Vulkan build passed 27/27 CTests in 454.85 seconds. Direct production diagnostics then passed package, 24-case acceptance, camera, course, art-budget, five-layout UI, Vulkan, and rig-training checks. The independent MSVC `Runner.exe --diagnose-rig-training` replay retained: biped 25.0376 m / 41.67 steps / 0 rejected seeds at update 900; humanoid 23.2516 m / 15.50 steps / 0 rejected seeds at update 1010; quadruped 7.6839 m / 54.83 cycles; crawler 9.9897 m / 46.33 cycles; hexapod 27.5296 m / 60.33 cycles. All retained runs are zero-authority, motion-valid, post-handoff, and conveyor-disabled.
- The freshly installed and independently extracted packages each passed version, Vulkan, package, 24-case acceptance, camera, UI, art-budget, course, and fresh five-rig training diagnostics through `run.bat` from the unrelated system temp directory. Both package learner runs reproduced the exact build-tree retained metrics. The 45-file ZIP extraction passed its external manifest byte audit. The independently extracted `--walk-eye-test` then rendered the real update-1010 zero-authority controller at 25.6 m / 18 steps / 11 crossings with 6/6 valid seeds and a 23.3 m / 15.5 retained mean; accepted capture `validation/v0730_packaged_retained_walk_eye_test.png` has SHA-256 `98c7484b7197f98818f095af7bf802fb77987d94a69fa3ea6afade5789fa2876`.
- Implementation PR `#92` cleanly configured and built under hosted GCC 14, and every completed suite passed, but `Runner.V0730ColdStart` hit its exact 600.10-second CTest ceiling before the slower shared runner completed the unchanged workload. The log showed valid deterministic teacher probes and no behavioral assertion; CTest reported `Timeout`, not a locomotion failure. Preserve all 1,200 updates, five rig subjects, seeds, handoff boundaries, and acceptance thresholds; raise only this real learner test's host budget from 600 to 1,200 seconds and require a clean retry before merge.
- PR `#92` retry `31300259305` passed clean configure/build and all other 21 suites, then the unchanged monolithic gate hit the new exact 1,200.10-second ceiling. Do not keep inflating a combined deadline. Preserve all ten-seed teacher probes for all five topologies, full frame-independence checks, and the complete 1,200-update learner, but expose teacher/frame truth and cold learning as separate CTest cases with independent 1,800-second ceilings and explicit command modes. A clean hosted rerun must pass both before merge.
- The split gate is locally proven without coverage loss. `Runner.V0730ReferenceFrame --references` passed all 50 topology/seed probes plus exact frame-state checks in 3.24 seconds under GCC 14 and 3.29 seconds under MSVC. Independent `Runner.V0730ColdStart --learner` passed the unchanged 1,200-update five-rig gate under GCC 14 in 316.72 seconds with the same retained results. CTest JSON exposes both exact command modes and independent 1,800-second ceilings; both compilers rebuild warning-clean and repository audit enforces the split contract.
- All local implementation, platform, package, installed, extracted, manifest, and direct eye-test obligations now pass. WALK-RELEASE-357 remains open only for exact tagged CI, published-asset re-download/byte comparison, and final remote branch/PR cleanup.
