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
**Status:** COMPLETED — FULL RELEASE VALIDATION PASSED

Trace and reconcile worker rollouts, completed training batches, rejected samples, optimizer steps, published policy versions, evaluations, and preview consumption. Surface stage-scoped counters and dominant rejection reasons so tens of thousands of rig updates cannot collapse into a few unexplained policy updates or silently starve PPO.

### WALK-LEARNABLE-GAIT-352 — Make the first useful policy learn alternating forward gait
**Status:** COMPLETED — FULL PLATFORM, INSTALLED, EXTRACTED, AND PACKAGED EYE GATES PASSED

Observation scaling, action authority, bootstrap decay, reward terms, termination rules, and PPO batching must jointly produce repeated left/right support transfer, foot passing, swing clearance, traction, upright balance, and terrain-relative distance. High joint or body speed without forward distance and stride evidence must not score as walking.

### WALK-INCREMENTAL-RETENTION-353 — Retain genuine partial walkers before final mastery
**Status:** COMPLETED — STRICT-VALID RETENTION, HANDOFF, AND RELEASE GATES PASSED

Checkpoint physically valid incremental improvements and carry them through same-rig retries under the measured final 18 m / 14-step six-seed mastery aggregate while retaining the 18 m / 16-step packaged display gate. Evaluation noise or one failed required test may not erase a clearly better multi-step controller; rig changes and incompatible training semantics still start fresh.

### WALK-RESET-CONVERGENCE-354 — Align rollout, evaluation, preview, and restart truth
**Status:** COMPLETED — SHARED ZERO-AUTHORITY EVALUATION/PREVIEW AND RELEASE GATES PASSED

Rollout, evaluation, champion selection, and preview must share motion validity, contact, distance, and stride definitions. Cluster reset causes and prove the dominant failure rate falls as learning advances. Preview restarts remain visible and may not conceal a persistent collapse, flip, overspeed, skating, or support-loss attractor.

### WALK-CURRICULUM-355 — Progress from stable steps to sustained walking before harder pressure
**Status:** COMPLETED — TOPOLOGY-SCOPED DEMONSTRATION, ZERO-AUTHORITY HANDOFF, AND RELEASE GATES PASSED

Use a firm hazard-free learning runway and staged evidence gates that first establish support transfer and alternating steps, then distance and speed, then terrain and hazards. Required tests must diagnose competence without monopolizing training or repeatedly testing a policy that has not received enough accepted optimizer updates.

### WALK-COLD-START-356 — Add a real bounded learning acceptance diagnostic
**Status:** COMPLETED — FULL LINUX/WINDOWS/INSTALLED/EXTRACTED COLD GATES PASSED

From deleted v0.7.30 state and fixed seeds, run real workers, PPO optimization, immutable policy publication, evaluation, and preview. By the bounded gate, retain at least one controller that satisfies the 18 m / 14 real-step six-seed Walk aggregate and includes an 18 m / 16-step replay seed and demonstrate sustained multi-seed progress without course motion. Add positive, negative, adversarial, and repeated-seed coverage plus exact full-state equivalence at 20, 60, and 240 render Hz.

### WALK-RELEASE-357 — Publish and independently verify Runner v0.7.30
**Status:** COMPLETED — TAGGED CI, PUBLICATION, RE-DOWNLOAD, AND REMOTE CLEANUP AUDITED

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
- Before v0.7.30 publication, all local implementation, platform, package, installed, extracted, manifest, and direct eye-test obligations passed; only exact tagged CI, published-asset re-download/byte comparison, and final remote branch/PR cleanup remained.
- Exact annotated tag v0.7.30 resolves to audited source 5d7e3bc4f5a7507213dce828ce3b07f2a5991b41. Tagged workflow 31302383586 passed Linux GCC 14 warnings-as-errors and every test, the full MSVC SDL3/Vulkan build and every Windows test, every feature diagnostic, exact packaging/manifests, installed and independently extracted run.bat audits, publication, and public-asset re-download byte comparison. The non-draft, non-prerelease release was published at 2026-08-09T10:32:32Z; its ZIP is 2,015,651 bytes with SHA-256 9f657540fd096dd6de89fc691fa5cf71ba6dfa3aea686c4f237d7f0f82a94768. Final remote audit found zero open PRs and only main at the tagged source.

# Runner v0.7.31 active-terrain, curriculum, rig-art, and contact-truth recovery

**Release state:** LOCAL RELEASE COMPLETE - REMOTE PUBLICATION NOT AUTHORIZED

The user's v0.7.29 packaged screenshots remain authoritative. At Walk / Run 30%, the UI shows 31,124 rig updates but only 14 policy updates, 1,707 preview restarts, no retained controller, six feet of evidence, zero real strides, and rejection because the body touched the ground before gait formation. A second screenshot shows the rig apparently tripping over collision that is not visibly represented at the start. The user explicitly requires a hazardous active sand simulation: the correction may stabilize the short launch/calibration pad and make its collision visible, but it must not freeze the course. Beyond that pad, sand must deform under load, water and waterlogged material must behave as authored hazards, holes must be real collision geometry, and all evolving collision must be rendered from the same state.

### WALK-LESSON-CLOCK-360 — Make learning schedules local to the current lesson
**Status:** COMPLETED LOCALLY — LESSON RESET/PRESERVE/CHECKPOINT, HANDOFF, AND V0.7.32 PACKAGE GATES PASS

Persist an explicit lesson update clock inside PpoTrainer. Reset it on a real course-stage boundary without resetting rig-scoped lifetime work, optimizer state, or compatible policy progress. Teacher authority, skill bootstrap, guided imitation, assisted-best clearing, evaluation eligibility, preview authority, and lesson telemetry must use the lesson clock where their meaning is stage-local. Preserve it in compatible checkpoints, migrate older state explicitly, and prove Stand → Crouch → Walk on one rig without switching presets or depending on lifetime update age.

### WALK-CROUCH-OWNERSHIP-361 — Prove a cold raw policy learns and retains crouch
**Status:** COMPLETED LOCALLY — COLD RAW ZERO-AUTHORITY CROUCH AND V0.7.32 PACKAGE GATES PASS

Replace indefinite/fixed crouch assistance with a finite, measured handoff. Train the actual policy, clear assisted-era champion state at the boundary without discarding the learned network or Adam moments, and require a post-handoff zero-authority controller to pass valid crouch depth, hold, balance, forbidden-body-contact, recovery, and repeated-seed gates. The default rig must learn Crouch from normal curriculum entry without rig-selection fiddling.

### WALK-LAUNCH-CONTACT-362 — Align every authored rig to visible launch collision
**Status:** COMPLETED LOCALLY — ALL-RIG REPEATED-SEED LAUNCH CONTACT AND V0.7.32 PACKAGE GATES PASS

After deterministic terrain creation, rigidly place every rig so authored support nodes begin on the exact sampled launch surface, with no penetration, hidden drop, anatomy distortion, or initialization-order drift. Use a short, explicit launch/calibration pad whose rendered surface, collision height, material, and stability contract are identical. Pressure or relaxation must not create a subpixel invisible lip beneath an unmoving initial stance; transition continuously into the active terrain rather than extending a long sterile runway.

### WALK-ACTIVE-TERRAIN-363 — Preserve and improve hazardous sand, water, and holes
**Status:** COMPLETED LOCALLY — ACTIVE DEFORMATION/BOUNDARY/WATER/HOLE/SEED AND V0.7.32 PACKAGE GATES PASS

Keep pressure-driven deformation and relaxation active beyond the launch pad. Render a continuous substrate and an exact collision-surface line from the same interpolated terrain samples used by physics, then layer sand, mud, water, and hole materials without exposing a changing cosmetic lower edge. Delay large course objects until a locomotion foundation exists, while seeded terrain regions vary in location and microstructure. Add positive deformation and volume checks; negative launch-pad mutation checks; adversarial boundary, deposit, wrap, water, and hole checks; repeated-seed determinism; and visible packaged evidence that active hazards move only where collision moves.

### WALK-ART-TRANSFORM-364 — Make boots and body art follow full physical transforms
**Status:** COMPLETED LOCALLY — PURE ROTATION/TRANSLATION/FALLEN TRANSFORMS AND V0.7.32 PACKAGE EYES PASS

Boot art must translate, rotate, pivot, and mirror from the terminal authored support segment instead of remaining axis-aligned at a support node. Torso, helmet, limb plates, and compact equipment must follow the relevant physical orientation for vertical, fallen, horizontal, and reversed rigs while remaining presentation-only. Add pure transform tests for arbitrary rotations, reflections, degenerate segments, and repeated poses plus a real Vulkan diagnostic that makes a horizontal fallen pose obvious.

### WALK-ART-ALL-RIGS-365 — Apply the shared armor language to every rig topology
**Status:** COMPLETED LOCALLY — GRAPH-DERIVED ART RENDERS ALL SEVEN EXPOSED RIGS IN PACKAGE QA

Replace paired-biped and hard-coded motor-slot gates with graph-derived support, manipulator, torso, and head roles. Every user-visible rig receives the same current art family, fitted to its actual topology; individual art sets are deferred. Render rigs sequentially in diagnostics so the existing shared vertex budget remains bounded. Art must not alter physics, contacts, observations, rewards, policy dimensions, persistence, or frame timing.

### WALK-RIG-IDENTITY-366 — Remove or distinguish duplicate user-facing presets
**Status:** COMPLETED LOCALLY — SCAFFOLD INTERNALIZED AND SEVEN EXPOSED SIGNATURES/PACKAGE UI PROVEN DISTINCT

Audit normalized graph, silhouette, motor, support, and role identity across Humanoid, Biped, Scaffold, Chicken, Quadruped, Four-leg Crawler, Hexapod, and Monoped. Calibration-only blueprints may remain internal, but every exposed canonical preset must have a meaningfully distinct topology or silhouette and its own rig-scoped training identity. Add deterministic structural and rendered distinctness checks; switching aliases may not masquerade as a new rig or destroy compatible progress.

### WALK-FRAME-TRUTH-367 — Prove the new systems are render-cadence independent
**Status:** COMPLETED LOCALLY — EXACT REFERENCE/PREVIEW/TERRAIN/LESSON STATE CADENCE AND PACKAGE GATES PASS

Preserve fixed-step simulation and show exact full-state equivalence at 20, 60, and 240 render Hz for launch placement, active terrain evolution, lesson updates, teacher handoff, raw-policy evaluation, preview resets, gait/contact evidence, and art transforms. Invalid or partial render deltas may not advance physics, terrain, curriculum, or policy clocks.

### WALK-RELEASE-368 — Publish and independently verify Runner v0.7.31
**Status:** LOCAL ART-CLEAN RELEASE COMPLETE - REMOTE PUBLICATION INTENTIONALLY PAUSED

Update versioned source, documentation, focused design notes, package/install lists, diagnostics, release workflow contracts, and cleanup. Require repository hygiene and git diff --check; Linux GCC 14 warnings-as-errors and all CTests; the complete Windows SDL3/Vulkan build and all tests; package, acceptance, camera, UI, art, rig-training, course, crouch-learning, terrain/contact, and cadence diagnostics; installed and independently extracted run.bat from an unrelated directory; ZIP checksum and per-file manifest audit; packaged Vulkan eye evidence; public-asset re-download and byte comparison; zero cleanup PRs; main-only remote state; mission-cache closeout; and removal of generated release garbage.

### WALK-EVOLVING-RIG-369 — Optimize edited rigs and evolve bounded morphology
**Status:** COMPLETED LOCALLY — TWO-MODE ROUTING, SIX-SEED RAW EVALUATION, COMPLEXITY COST, STATE ROUND-TRIP, AND PACKAGE/RIG-LAB EYE GATES PASS

Provide two explicit rig-optimization modes: maximize control and authored parameters for the current edited blueprint without changing its anatomy, and evolve morphology through bounded add, remove, split, mirror, resize, and reconnect mutations. Evolution must preserve finite connected anatomy, distinct valid supports, controllable parent-pivot-child chains, policy-dimension migration or intentional reset boundaries, rig-scoped persistence, complexity pressure, deterministic repeated-seed evaluation, adversarial invalid-candidate rejection, editor round-trip, and graph-derived art fit. Candidates must improve held-out locomotion and hazard performance without conveyor motion, teacher authority, hidden assists, or duplicate aliases, and every accepted evolved rig must remain editable, recognizable, package-safe, and frame independent.

The v0.7.32 audit found that deterministic morphology generation, neutral activation of new policy outputs, nursery adaptation, held-out six-seed evaluation, rollback, and editor-copy/save paths already exist, but `mutate_rig_locked()` is hardwired to the anatomy-immutable tuner and the UI claims anatomy can change only manually. Complete the mission by exposing explicit `CONTROL OPTIMIZE` and `MORPHOLOGY EVOLVE` modes, persisting the selected rig-scoped mode, routing only the latter through bounded morphology candidates, labeling generation/accept/reject telemetry truthfully, and proving the default control mode remains byte-for-byte anatomy immutable while morphology mode can generate and safely reject or accept structurally valid editable candidates.

The runtime now has explicit RigOptimizationMode state. Fresh or canonical rig boundaries default safely to CONTROL OPTIMIZE; MORPHOLOGY EVOLVE alone routes through the existing bounded topology candidate engine. The mode is command-queued, shown in status/accept/reject/rollback telemetry, written in autonomy-state version 17, restored per rig, and read-only compatible with version 16 as control mode. Focused tests cover control anatomy immutability, deterministic topology growth, invalid-mode fallback, and explicit naming; final compiler, persistence, UI, package, and eye gates subsequently passed as recorded below.

### WALK-SKIN-WRAP-370 - Assemble a connected body skin before fitting armor art
**Status:** COMPLETED LOCALLY - CONNECTED SKIN, SEVEN-RIG ART BUDGET, AND PACKAGE EYE QA PASS

Replace independent bone-strip sprites with one graph-derived, joint-overlapped body envelope and fitted armor layer. The skin must connect shoulders, torso, pelvis, limbs, joints, hands, and supports; preserve the physical pose without exposing a skeletal gap field; produce a broader armored humanoid silhouette rather than the current narrow torso and hips; remain topology-derived for every canonical rig; and keep rendering bounded, finite, deterministic, and frame independent. Add upright, crouched, fallen, mirrored, adversarial-scale, and all-rig visual/structural coverage.

### WALK-ARM-GAIT-371 - Give paired arms a true sagittal counter-swing
**Status:** COMPLETED LOCALLY - GRAPH-DISCOVERED PHASE-OPPOSED ARMS; FINAL ZERO-AUTHORITY PACKAGE PROOF PASSED

Drive complete two-link manipulator chains through bounded side-view targets opposite the paired support-chain phase. Arms must swing fore and aft with articulated elbows, may not spread into the current symmetric side/T pose, must remain fully policy-controlled after teacher handoff, and must derive roles and dimensions from authored topology rather than motor slot numbers. Prove zero-authority evaluation, absent/manipulator-extra topology behavior, finite targets, repeated terrain seeds, and render-cadence equivalence.

### WALK-CRAB-EYE-372 - Eliminate the retained humanoid's visible scissor/crab gait
**Status:** COMPLETED LOCALLY - PERSISTENT X-STEP REJECTION; FINAL PACKAGE PROOF PEAKED AT 0.183 S

The packaged screenshots outrank aggregate distance and crossing counters: the retained humanoid still holds a wide split and crosses its lower legs into an X. Correct the support-chain bend/lane geometry, distinguish sagittal foot passing from sustained shank scissoring, shape and reject the latter with the same contact-valid predicate used by retention, and expose the evidence in diagnostics. Require long zero-authority retained previews with forward distance, alternating valid contacts, bounded support span, no persistent lower-leg intersection, no teacher/conveyor motion, adversarial negatives, and repeated-seed/cadence coverage.

### WALK-TERRAIN-PRESSURE-373 - Preserve active sand without contact bubbles or trip lips
**Status:** COMPLETED LOCALLY - EXACT COLLISION-SAMPLED SURFACE; DPI-CORRECT PACKAGE COURSE EYE PASSED

Foot pressure must compact or shear the loaded surface while redistributing displaced material into a broad, smooth, volume-conserving berm away from immediate contact. It may not deposit into the adjacent cell and raise a spike beneath or directly beside the next footfall. Preserve visibly active dry and waterlogged sand, water coupling, holes, deterministic repeated seeds, launch stability, cadence independence, and physically matched collision/render heights; add repeated-footfall, alternating-foot, edge, saturated, non-finite, and long-run conservation tests.

The 2026-08-12 packaged runtime still shows terrain movement that the user identifies as visibly wrong. Inventory the sampled collision surface, top-surface stroke, substrate fill, sand/water overlays, cell interpolation, camera transform, pressure update, relaxation, and render snapshot ownership. Intended load-driven deformation must remain, but every visible moving boundary must be derived from the exact current collision samples with no independently animated lower edge, stale snapshot, cell seam, wrap, or one-frame lag.

### WALK-EYE-CRASH-374 - Make the packaged walk eye test complete reliably
**Status:** COMPLETED LOCALLY - PROOF RUNS BEFORE WINDOW; RESPONSIVE VULKAN EYE CAPTURED

The local packaged `--walk-eye-test` exits or crashes instead of producing durable eye evidence. Reproduce it without touching the user's live Runner process, isolate proof failure from renderer/runtime failure, emit actionable diagnostics on every rejection, and require build-tree, installed, and independently extracted runs from unrelated working directories to render and exit successfully before replacing the local release artifact.

Final extracted eye evidence isolated the remaining launch defect: the full 1,200-update proof runs synchronously after SDL creates the Vulkan window but before the event loop starts, so Windows marks the visible window not responding and the user correctly experiences it as another crash. Run the proof before creating the window, fail headlessly with printed evidence, then open an already-populated responsive Vulkan proof window. Do not relaunch the blocking path again.

### WALK-LIFETIME-ODOMETER-375 - Preserve lifetime totals and reject distance discontinuities
**Status:** COMPLETED LOCALLY - ODOMETER, INCOMPATIBLE-CURRENT, V0.7.30 FALLBACK, AND PRE-SAVE IMPORT GATES PASS

All-time statistics must survive preview, episode, lesson, retry, recalibration, same-rig, rig-switch, process, and compatible-version persistence boundaries according to their documented scope. Distance must integrate only finite accepted simulated displacement within one continuous episode and may never count spawn placement, camera movement, terrain motion, origin rebasing, preview teleport/reset, worker aggregation duplication, or unit conversion more than once. Add reset-matrix, rig-scope, persistence round-trip, discontinuity, adversarial non-finite, metric/imperial, repeated-step, and cadence tests, and expose separate episode, selected-rig lifetime, and true all-rig lifetime labels.

The user's v0.7.31 totals screenshot is direct contradictory evidence: THIS RIG shows 1,050 updates while ALL TIME shows only 1,054, despite the immediately preceding v0.7.30 run having roughly 8,000 updates. THIS SESSION and ALL TIME both show about 100.49 mi, proving the current session became the effective lifetime baseline instead of importing the compatible prior ledger. The distance-to-step ratio shown (100.46 mi / 212,346 steps, about 2.50 ft per simulated step) is physically plausible; the apparent tenth-mile jumps are batched background rollout aggregation, not one visible preview step. Before packaging, prove compatible v0.7.30 lifetime-ledger import occurs before any v0.7.31 autosave can replace it, preserve that ledger across startup/rig selection, and label batched agent-equivalent training distance separately from the visible preview distance.
## v0.7.31 local implementation and release-gate evidence

- Windows Visual Studio 2022 Release rebuilt the complete SDL3/Vulkan application and every test target. The final uninterrupted suite passed 29/29 in 489.25 seconds; the real five-rig cold learner passed in 380.88 seconds and cold raw Crouch passed in 80.08 seconds. After the packaged course diagnostic gained explicit `launch_contact`, `active_terrain`, and `delayed_objects` fields, its affected repository/course tests passed 3/3.
- A fresh WSL build compiled every CPU/core target with GCC 14.3 and `-Werror`. All 24 Linux CTests passed in 403.92 seconds; the cold learner passed in 302.31 seconds and Crouch in 74.28 seconds. The final active-terrain diagnostic update rebuilt warning-clean and its affected tests passed 2/2.
- Direct build-tree, installed `run.bat`, and independently extracted `run.bat` executions from `C:\Windows\Temp` passed version, SDL3/Vulkan, package, 24/24 acceptance, camera, visible UI, seven-rig plus fallen-pose art, five-rig zero-authority learning, and course diagnostics. The finalized course output explicitly reports `launch_contact=passed active_terrain=passed delayed_objects=passed` alongside material, water/hole, observation, delayed-pressure, climb, equipment, and frame-independence results.
- The cold diagnostic retained strict-valid zero-authority controllers with course motion disabled: biped 21.0267 m / 38.33 steps / 0 invalid seeds, humanoid 33.2620 m / 33.17 steps / 0 invalid, quadruped 8.6960 m / 54.50 support cycles / 0 invalid, crawler 15.4991 m / 54.67 / 0 invalid, and hexapod 26.4784 m / 51.67 / 0 invalid. Later exploratory regressions did not replace these retained controllers.
- `--diagnose-art` rendered the production course, all seven exposed rigs, and a horizontal fallen humanoid with a 218,415-vertex / 5,241,960-byte peak, below the 6,291,456-byte acceptance ceiling and 8 MiB hard limit.
- Hosted PR `#93` configured and built cleanly under GCC 14 and passed 23 of 24 CTests. The cold learner completed in 978 seconds rather than timing out, but its aggregate diagnostic returned failure even though every printed retained replay cleared the public distance, gait-cycle, invalid-motion, handoff, authority, course-motion, and preview-reset requirements; the quadruped retained 6.84968 m / 55 cycles / 0 invalid seeds. Preserve those gates and expose the exact per-rig failing predicate before changing trainer behavior or thresholds.
- The initial abbreviated log reading attributed the hosted failure only to the humanoid's non-strict historical quality bit; that diagnosis is rejected after reading the complete second-run output. The retained controller travelled 39.7983 m / 28.83 steps after the 900-update handoff and zero authority, but 2 of 6 fresh replay seeds carried lateral-crab rejection mask `1024`. Keep the stronger fresh-replay gate and fix trainer/retention behavior; do not lower crab, distance, gait, validity, handoff, authority, course-motion, or reset requirements.
- The corrected retained-policy release contract passed direct positive, negative, non-finite, assisted, pre-handoff, invalid-motion, conveyor, and reset-boundary cases under MSVC and GCC 14. Fresh full learners then passed on both platforms: Windows in 368.6 seconds and Linux in 336.3 seconds, with all five retained policies clearing the unchanged production replay gates. The reference/frame suites passed separately, and every remaining final-source CTest passed 27/27 on Windows and 22/22 on Linux, completing the 29-test and 24-test platform matrices without rerunning the same learner binaries twice.
- Hosted retry `31319306635` built cleanly and again passed every suite except the completed cold learner. Its 1,191-second result proves the unresolved cross-machine defect is the humanoid retained controller's 2-of-6 lateral-crab replay rejection, not timeout, quadruped distance, terrain, art, Crouch, or packaging. Reproduce the hosted worker topology and require a clean six-seed sagittal champion before merge.
- The first explicit two-worker GCC 14 learner with aligned crab shaping removed the hosted defect: humanoid retained replay changed to rejection mask `0`, 0 of 6 invalid seeds, and 21.1747 m. It is still rejected because its 13.5-step six-seed mean misses the unchanged 14-step mastery threshold by 0.5. Preserve that threshold and increase only the event reward for a physically verified sagittal crossing before repeating the low-core gate.
- A two-worker Windows replay with the crossing reward raised from 0.090 to 0.140 kept every retained candidate valid, but the humanoid retained candidate reached only 18.7364 m / 13.6667 mean steps (rejection 0, invalid 0) at update 1180. The red current-policy rows for biped, quadruped, crawler, and hexapod were not the retained release candidates; the authoritative retained rows remained valid. Reject the larger reward because it did not clear unchanged mastery and introduced noisier current-policy behavior. Revert to 0.090 and test a bounded 1,250-update run before changing any evidence threshold.
- The bounded two-worker Windows 1,250-update replay at the safer 0.090 crossing reward rejected the extra-time hypothesis: biped, quadruped, crawler, and hexapod retained strict-valid champions, but humanoid never retained a strict-valid candidate (25.8997 m / 16 steps was rejected on 5 of 6 seeds with mask 1033 and overspeed). Do not raise the update budget or weaken evidence. Restore the original 1,200-update budget and test a stronger reward that still requires an actual contact-validated sagittal crossing; if that fails cross-platform, refactor selection/training instead of continuing scalar tuning.
- The final scalar experiment, two-worker Windows at the original 1,200 updates with a 0.200 contact-validated crossing reward, retained a strict-valid 22.3585 m humanoid at update 900 with rejection 0 and invalid 0, but averaged 13.8333 steps: 83 total steps across six seeds, exactly one below the unchanged 84-step aggregate requirement. Biped and all multi-support rigs retained strict-valid champions. Scalar tuning is exhausted. The repeated best-update-at-handoff result identifies consolidation after zero-authority handoff—not evidence thresholds or elapsed updates—as the next structural target.
- Structural correction selected: after a strict zero-authority champion is first retained at the foundational Walk / Run handoff, allow a bounded 300-update consolidation window that softly anchors the exploratory policy toward that already-proven champion. It must never blend teacher actions, must apply only to the uneven foundational gait stage, and must expire exactly at the boundary. Add positive, negative, final-active-update, expiry, and non-Walk-stage tests before repeating the two-worker gate.
- The bounded consolidation replay improved or preserved strict champions for biped and every multi-support rig, but did not replace the humanoid update-900 champion (22.3585 m / 13.8333 steps, rejection 0, invalid 0). Crucially, the final exploratory humanoid reached 21.8007 m / 16.1667 steps and was rejected specifically by mask 1024 (lateral crab gait) on 5 of 6 seeds. Preserve consolidation, reward, and evidence thresholds; strengthen only the per-step crab penalty whose predicate is identical to the hard rejection gate, then repeat the two-worker test.
- The 0.060 hard-gate-aligned crab penalty replay is rejected: it suppressed humanoid learning without converting lateral transfers, retaining only 18.9846 m / 11.6667 steps. Restore 0.012. Across otherwise comparable low-core runs, the contact-validated crossing reward produced a monotonic retained sequence of 13.5 steps at 0.090, 13.6667 at 0.140, and 13.8333 at 0.200. Test one final bounded interpolation at 0.250 with consolidation retained; this signal is impossible without an actual sagittal crossing and does not reward crab motion.
- User eye evidence from the supplied v0.7.30 `creature.rig` supersedes the old canonical humanoid rest geometry. The file preserves the correct 13-node / 15-bone / 8-motor graph and semantic supports 4/6, but uses a 2.86188674 m pelvis, compact 3.85471725 m torso / 4.17547226 m head, forward close-profile articulated arms, and 0.105400003 m terminal support stubs. The live retained update 2745 reached 14 ft with 5 real steps / 4 crossings but visibly sat behind overextended forward feet and continued safety-rule retries. Promote the supplied graph geometry into the canonical humanoid, regenerate constraints and motor calibration through the existing topology-derived path, preserve strict gait evidence, and add exact calibration plus standing/teacher/adversarial tests. The old-geometry 0.250 replay was stopped because its result could not validate the new authored rig.
- Exact-file adversarial reference rejected the uncorrected uploaded stance: all 10 humanoid teacher seeds terminated on foot-node skating / rolling after 0–5 steps, while biped remained valid. The uploaded pelvis is 0.04918674 m higher than the previously valid calibration while knees and feet are unchanged, nearly locking both legs and pivoting the compact support stubs. Preserve the supplied compact torso, head, and forward articulated arms, but lower the pelvis to the proven 2.8127 m clearance before repeating the same ten-seed teacher and frame-cadence checks.
- Lowering only the pelvis removed foot skating and yielded 26–34 m, 25–35-step valid teacher walks on 8 of 10 humanoid seeds; seeds 29441 and 57344 still flipped. The supplied file places both complete articulated arm chains on the same forward side with only 0.110018037 m between shoulder pivots, duplicating forward mass and requiring the screenshot's backward body lean. Preserve the compact supplied arm lengths but mirror one complete chain around the torso x-axis position, rebuild its bones/motor neutrals, and require all ten terrain seeds to remain valid before learning.
- Mirroring the supplied compact arm chain improved deterministic humanoid teacher validity from 8/10 to 9/10 and produced 28–32 m valid walks on every passing seed; only seed 57344 flipped after 10.4472 m / 8 steps on firm ground. The remaining rigid-ground impulse still reaches nearly straight knees. Lower the pelvis a further 0.0327 m to 2.78 m for neutral knee compliance, without moving knees, feet, torso, head, or balanced arms, and repeat the identical seeds.
- The 2.78 m pelvis experiment is rejected: although valid seeds accelerated to 35–39 m, 4 of 10 seeds flipped or flew. Restore 2.8127 m, which held 9/10 and 28–32 m. With leg clearance isolated, the remaining compact-rig failure is upper-body control: the much shorter, lower-inertia arms still receive the old wide-arm swing. Reduce only manipulator arm swing during foundational gait, keep arms trainable, and require the same ten seeds plus strict cold replay.
- Final corrected supplied-rig reference passed: pelvis 2.8127 m, supplied compact torso/head, one compact arm chain mirrored around the torso, and foundational shoulder/elbow swing reduced from 0.38/0.12 to 0.20/0.06. All 10 humanoid terrain seeds survived 19.9999 s with no invalid motion, travelled 28.8495–31.3417 m, recorded 23–29 real steps and 13–21 sagittal crossings. Biped, quadruped, crawler, hexapod, repeated seeds, and exact 20/60/240 Hz full-state equivalence also passed.
- Direct v0.7.30 user-eye evidence then showed the same custom morphology's current exploratory policy at 257 ft, 6.1 mph, 75 real steps, and 72 leg crossings, while the main view still leaned behind the support midpoint. This disproves the old two-step plateau for that run and confirms the remaining defect is retained/posture quality, not reachability of repeated gait. The screenshot remained on Walk / Run with `EQUIP NONE / UNARMED`; pistol behavior belongs to later Equipment Targets / Combat Course and remains covered by package acceptance rather than being inferred from lesson 3. v0.7.31 exposes seven graph-distinct presets and keeps Scaffold internal, but shared graph-derived armor remains intentionally common until rig-specific variants are authored.
- First two-worker cold learner on the corrected compact rig found a 25.5607 m / 21.1667-step humanoid at update 1000, but fresh replay rejected mask 1025 (lateral crab plus invalid motion) on 2 of 6 seeds, so no strict champion existed. The deterministic teacher is 10/10 valid, yet guided teacher-action imitation currently becomes zero at the exact zero-authority handoff. Extend imitation only—not action blending—through the existing 300-update consolidation window with bounded decay to zero; evaluation authority must remain exactly zero and the unchanged six-seed release gate remains authoritative.
- Bounded post-handoff guided imitation also failed locally: the retained humanoid reached 19.325 m / 22.1667 steps at update 970, but fresh replay retained lateral-crab rejection mask `1024` with 1 of 6 invalid seeds. Biped, quadruped, crawler, and hexapod retained strict-valid champions. Do not publish this state. Preserve it locally while account access is restored, then remove or redesign the unsuccessful imitation experiment before rerunning Windows and GCC low-core gates.
- Local-only release correction selected: the foundational teacher must not choose an exaggerated 0.82 m stride and lift merely because a paired-leg rig also has arms. Derive the foundational step length, swing lift, and nominal leg height from the authored two-link support chains, keep the corrected compact arm counter-swing, and let the existing bounded zero-authority consolidation imitate only that anatomy-scaled gait. Positive biped/humanoid geometry tests, manipulator-independence, adversarial finite bounds, repeated teacher seeds, strict two-worker retained replay, frame-cadence equivalence, and the complete local package audit remain mandatory. No push, tag, PR mutation, or uploaded release is authorized; stop at locally verified ZIP/checksum/manifest artifacts.
- The corrected anatomy-scaled gait passed Windows core, v0.7.17 gait evidence, and all repeated v0.7.30 teacher/frame probes. The fresh two-worker five-rig learner then passed in 806.03 seconds: retained humanoid 19.7592 m / 18.8333 steps, rejection mask 0, zero invalid seeds, best update 1090, teacher authority zero; biped 24.4357 m / 34.5 steps, quadruped 10.669 m / 50.8333 cycles, crawler 15.1193 m / 54.3333 cycles, and hexapod 22.1891 m / 51.5 cycles also retained with mask 0 and zero invalid seeds. Continue with the unchanged Linux, full Windows, diagnostic, installed/extracted, manifest, checksum, and eye-evidence gates before producing the local ZIP.

- The user-reported lifetime reset and tenth-mile-per-step inflation were traced to two separate accounting defects. Canonical rig switches now clear policy/optimizer/best state without erasing the persisted all-rig ledger. Rollouts accumulate only finite accepted per-fixed-step forward displacement, reject backward/non-finite/over-50-km/h discontinuities, and divide worker totals once into agent-equivalent distance. Core tests cover teleport/backward/non-finite inputs, checkpoint round-trip, rig switching, and worker/time bounds.
- Active pressure still compacts and conserves sand, but displaced volume now forms a symmetric berm across cells two through six instead of raising either adjacent cell. Terrain, launch, repeated determinism, water/hole, delayed-pressure, and exact frame-independence diagnostics pass.
- Optional art now renders over a connected topology-derived skin with overlapping joints, pelvis, chest, and shoulder bridge. The torso width is bounded by the assembled envelope, all seven exposed rigs plus a fallen pose remain under the 75% vertex ceiling, and no motor-slot-indexed shoulder or terminal-hand presentation remains.
- Complete two-link arms now follow bounded fore/aft counter-swing targets. Paired gait keeps its functional opposing sagittal leg branches; normal ten-seed biped/humanoid passing peaks at only 0.150-0.167 seconds of projected shank intersection. Contiguous intersection above 0.34 seconds is shaped, rejected from incremental/final retention, and displayed in advanced and walk-proof telemetry. All 50 topology/terrain references and exact 20/60/240 Hz checks pass.
- The first headless learned proof honestly failed after a same-bend knee experiment (6.437 m / 6.5 steps, 3/6 invalid); that geometry was rejected rather than packaged. After restoring functional opposing leg branches and retuning the articulated arm reach, the exact 1,200-update Windows proof passed: retained update 1180, authority 0.000, mean 19.141 m / 18.83 steps, 0/6 invalid seeds, displayed seed 61443 at 20.083 m / 19 steps / 13 crossings. `--diagnose-walk-eye` now prints this actionable evidence and exits deterministically.
- The final complete Windows SDL3/Vulkan matrix passed 29/29 CTests in 880.30 seconds, including the 768.66-second five-rig cold learner and 82.58-second cold Crouch learner. The final Linux GCC 14 warnings-as-errors matrix passed 24/24 CTests in 701.08 seconds, including the 598.65-second cold learner and 74.95-second Crouch learner. The later startup-order change touches only `main.cpp`; the Windows executable rebuilt cleanly and the repository audit proves `prepare_walk_eye_test` precedes `SDL_CreateWindow`.
- Incompatible v0.7.31 policy state now imports only its validated cumulative lifetime ledger before starting a fresh policy, optimizer, best controller, and mastery state. When no v0.7.31 checkpoint exists, startup searches the same package directory for `runner-v0730-walk-autosave.eppo` and imports its compatible lifetime ledger before the first v0.7.31 autosave. Non-finite, incompatible-policy, exact fallback-before-save, rig-switch, and round-trip tests pass; UI copy distinguishes batched agent-equivalent training distance from the visible preview.
- The freshly installed and independently extracted final local package passed version, Vulkan, package, 24/24 live acceptance, camera, UI, art, course, and exact frame-independence diagnostics. Its independently extracted `run.bat --diagnose-package` passed from `C:\Windows\Temp`. The exact packaged `--diagnose-walk-eye` passed in 127.7 seconds with update 1180 retained, zero authority, 19.141 m / 18.83-step six-seed mean, 0/6 invalid seeds, and a 20.083 m / 19-step / 13-crossing display seed.
- The old visible-eye failure was a startup-order bug, not a Vulkan or learned-policy crash: the 1,200-update proof ran after window creation and before event pumping. Proof preparation now completes before any SDL window exists. A controlled final-binary run remained responsive while headless, opened an already-populated responsive Vulkan window, and produced `build/local-release-final/v0731-final-responsive-walk-eye.png`; that exact `Runner.exe` is byte-identical across the build tree, installed package, and independent extraction (SHA-256 `341cc6f3a18791737ea0b7bd7395e6cb3bba94bb0bc8f13d36d388710e611f9d`).
- The local-only release contains 46 independently hashed files. Installed and extracted trees match the per-file manifest exactly. Remote push, tag, PR, release upload, public-asset re-download, and branch cleanup are intentionally not performed because the user explicitly restricted this release to local artifacts.


The 2026-08-12 14:45:09 side-view screenshot makes the combined failure exact: grey/orange procedural capsules and joints remain exposed beneath the desired ivory armor; both hands and elbows occupy nearly mirrored forward/lateral poses instead of one arm leading and one trailing; the knees split excessively, lower legs converge into an X, and the feet overlap during an exaggerated high-knee step. Fix pose generation and retention predicates, not only draw order. Require phase-opposed shoulder targets tied to opposite support-chain phase, rear-arm occlusion/layering, bounded knee separation and lift, non-overlapping foot lanes outside the brief physical passing interval, and long raw-policy eye evidence.
### WALK-ART-LAYER-SCALE-376 - Remove legacy duplicate art and size the assembled armor correctly
**Status:** COMPLETED LOCALLY - REAL GRAPH BONES RESTORED; GENERATED BODY REMAINS REMOVED

The user's first v0.7.31 local-package eye test outranks the prior diagnostic budget pass: old unattractive bone-attached presentation is visible again underneath or beside the correctly assembled fitted armor, while the desired fitted pieces are too small on the physical body. The direct corrected-package window test then proved that recoloring and shrinking those capsules did not remove them: the neutral underwrap, joint caps, pelvis bridge, chest capsule, and shoulder bridge are still the same generated layer and must not render at all when modular art is enabled. Inventory every presentation pass and asset load, remove the legacy duplicate body/limb/foot/torso/helmet layer rather than hiding it by draw order, and derive one bounded visual scale from the assembled topology envelope. The fitted armor must cover the connected skin at the intended readable humanoid scale without changing node positions, collision, mass, motors, contacts, observations, policy dimensions, gait, camera, or frame timing. Apply the same assembly contract to every exposed rig, retain deliberate debug skeleton rendering only when explicitly requested, and add structural negative coverage proving the legacy body-art path cannot render concurrently plus upright, fallen, compact, large, all-rig, vertex-budget, and packaged Vulkan eye evidence.

The next direct package eye test corrected an overreach: removing generated capsule art must not erase the readable real-bone structure. Preserve the authoritative graph as thin bone links beneath the assembled armor in normal presentation, while keeping node circles, indices, and edit markers in the explicit debug overlay. Bone links are structural visualization, not the removed procedural body mass; they may not change physics or replace the connected authored silhouette.

### WALK-EARLY-CURRICULUM-LATENCY-377 - Turn passing first-goal evidence into timely mastery tests
**Status:** COMPLETED LOCALLY - 80/200 READINESS, RAW EVALUATION, INSTALLED/EXTRACTED QA PASS

The user's v0.7.31 MAX CPU screenshot at Static Crouch / Hold / Recover is direct performance evidence: session time is 8:40, training time 9:13, 704 simulated runs and 704 passed stage checks are reported, but lesson completion remains 0%, mastery tests remain 0/8, runs show 0/4, and only about 204-220 learning updates have accrued. Audit lesson-local clocks, readiness, evaluation cadence, worker aggregation, test scheduling, retry/reset ownership, and UI counter consistency. Passing foundational Stand and Crouch evidence must trigger bounded early mastery evaluation without lowering crouch depth, hold, recovery, balance, contact, repeated-seed, raw-policy, or zero-authority requirements. Add positive ready-sooner, negative insufficient-evidence, exact-boundary, retry, lesson-transition, checkpoint-resume, worker-count, and 20/60/240 Hz tests, and expose why a mastery test is not yet scheduled instead of leaving a zero counter for minutes.

### WALK-IMPERIAL-DEFAULT-378 - Start the UI in imperial units
**Status:** COMPLETED LOCALLY - FRESH IMPERIAL DEFAULT; INSTALLED/EXTRACTED QA PASS

Make imperial the default user-facing distance, speed, course-marker, and totals presentation on a fresh launch while preserving metric SI units internally for physics, terrain, learning, persistence, diagnostics, and deterministic tests. The existing Units control must still switch both ways without resetting training or changing any simulation state, and a persisted explicit user choice may override the fresh-install default if such preference persistence exists. Add default, toggle round-trip, formatting, and no-state-mutation coverage.

### WALK-CAMERA-RANGE-379 - Make manual zoom materially useful
**Status:** COMPLETED LOCALLY - 0.28X-3.60X / 12-150 PX/M; INSTALLED/EXTRACTED QA PASS

Expand manual zoom-in and zoom-out limits and step response enough to inspect fitted art, foot contact, posture, and terrain locally or frame a substantially longer course. Preserve smooth finite clamping, auto-view full-body/course behavior, PIP framing, resize and DPI handling, camera follow, unit switching, and exact separation from fixed-step simulation, terrain, curriculum, and policy state. Add repeated-button boundary, wheel/key, auto/manual transition, viewport-size, invalid-delta, and 20/60/240 Hz no-state-mutation coverage plus packaged eye evidence at both extremes.
## v0.7.31 corrected-local eye-test response evidence

- The renderer no longer traverses raw deformable-terrain fine cells. The continuous substrate and its material band share the exact pair of current ground_height_at samples used by collision on every segment; pressure-driven surface motion remains active, while the independently changing fine-cell underside visible in the user screenshot is gone.
- With modular art enabled, no generated capsule, joint cap, foot, pelvis, chest, shoulder, or malformed-part body fallback can render concurrently. Thin authoritative graph-bone links remain visible beneath authored limb sprites; bounded joint overlap and the enlarged torso plate fit the shoulder/hip envelope into one connected 1.24x-1.34x assembly across all seven exposed rigs. Debug skeleton mode strengthens those links and adds node markers/indices without owning normal bone visibility.
- Both the foundational arm teacher and bilateral assistance discover complete non-support shoulder/elbow chains from topology. The last slots-4-through-7 upper-body assumption was also removed from the crawl path. Strict mastery evaluation now steps only raw policy output; lesson assistance remains confined to training rollout and preview ownership.
- The stronger fore/aft hand target remains phase opposed, while an added persistent wide-knee plus overlapping-foot predicate rejects the eye-test X pose without rejecting a brief physical pass. Final reference evidence stayed valid across ten seeds per topology; humanoid/biped projected scissor maxima were 0.150-0.267 seconds below the unchanged 0.34-second rejection limit.
- Stand readiness is 80 lesson updates. Crouch fades from update 60 to exact zero authority at update 200, and its readiness boundary is the same update while retaining four completed runs, eight mastery tests, strict depth/hold/contact/recovery, and repeated-seed requirements. The corrected cold raw Crouch test passes after 80 Stand plus 320 Crouch updates.
- Fresh UI state is Imperial. Manual camera control now spans 0.28x-3.60x and 12-150 px/m; automatic fit keeps its separate 62 px/m ceiling. Repeated input, invalid values, toggle behavior, and presentation/simulation separation are covered.
- The exact final bones-visible Runner executable is SHA-256 `35c4f7d7c9040b7e5ad545caff3db50a158dde2ac3369de95bc7b2990523d6b8`. Its fresh 1,200-update package proof retained update 1190 at authority 0.000, averaged 20.787 m / 24.83 steps over six seeds with 0/6 invalid, displayed 21.646 m / 23 steps / 16 crossings, and peaked at 0.183 s scissor duration. The exact-source 27-test Windows non-learner matrix, prior unchanged long Windows learner/Crouch gates, final GCC 14 warnings-as-errors rebuild/art gates, installed short diagnostics, and DPI-correct orthographic/course Vulkan eyes pass. The prior five-rig learner retained biped 24.0765 m / 33.833 steps, humanoid 25.9546 m / 21.0 steps, quadruped 5.37336 m / 47.666 cycles, crawler 11.9068 m / 55.5 cycles, and hexapod 20.9148 m / 54.5 cycles with course motion disabled.
- WALK-EVOLVING-RIG-369 is complete locally as explicit control-locked and bounded morphology modes. Both compilers, asynchronous state round-trip and fallback, installed/extracted package, and selected-mode Rig Lab eye evidence pass.

### WALK-CROUCH-HINGE-380 - Prevent Static Crouch from stalling on hip-hinge retries
**Status:** COMPLETED LOCALLY - RAW-POLICY HANDOFF, EXACT BOUNDARY, REPEATED-SEED, AND PACKAGE EYE GATES PASS

The 2026-08-12 17:27:12 screenshot is authoritative: Static Crouch / Hold / Recover is stuck at 72% completion and 181/200 updates with 0/8 mastery passes, 64 preview restarts, and repeated `HIP HINGE - NOT A CROUCH` rejection. Audit authored crouch targets, torso/pelvis height, knee bend, support contact, raw-policy handoff, evaluation scheduling, retry ownership, and progress telemetry. A crouch must lower through both support chains while keeping the torso bounded, hold, recover, and schedule raw-policy mastery promptly; a forward fold must remain rejected. Add positive, hip-hinge negative, partial-depth, contact-loss, exact 200-update boundary, retry, checkpoint, repeated-seed, and 20/60/240 Hz coverage.

### WALK-FACING-GAIT-381 - Make the entire rig and gait direction-aware
**Status:** COMPLETED LOCALLY - BIDIRECTIONAL PHYSICS, WHOLE-RIG MIRRORING, AND ZERO-AUTHORITY WALK EYE PASS

The current side-view art is close but its arms, legs, boots, helmet, and gait targets do not agree on forward. Introduce one authoritative facing sign shared by locomotion intent, support/manipulator target construction, equipment aim, art mirroring, near/far layering, camera lookahead, observations, diagnostics, and retained-policy evaluation. Backward motion must use a bounded reverse gait without visually pretending the rig faces backward; after the turn phase the complete rig presentation must mirror and forward locomotion must proceed in the new facing direction. No fixed motor slots, biped-only render gate, observation-dimension drift, checkpoint corruption, or render-clock mutation is allowed. Cover both facings, reverse motion, turn boundaries, all canonical topologies, repeated seeds, and exact render-cadence equivalence.

Cross-platform reference replay found a release-blocking return edge: GCC 14 exposes a deterministic humanoid flip when repeated foot pressure destabilizes foundation sand, even when Windows survives. Strengthen the dry-sand foundation firmness enough to keep bounded support while retaining active pressure displacement, material variation, water, holes, and the original full-difficulty deformation scale; bound backing with a timeout; and require the exact adversarial seed to survive repeated full reverse traversals on both platforms. The subsequent cold gate found that a nonzero partial quality key could be retained as a champion when no strict candidate existed; forward-gait best snapshots and release eligibility must require the strict-valid bit, with partial walkers preserved only as learning signal, never publication state.

### WALK-HAND-ART-382 - Complete the assembled armor with side-view hands
**Status:** COMPLETED LOCALLY - SUPPLIED SIDE-VIEW HAND SOURCE, GRAPH FIT, HASH LOCK, AND PACKAGE EYE PASS

Extract the hand only from the strict `Side View` row of the user-supplied modular character sheet already represented by the repository art source; do not create, retain, or package a separately generated hand substitute. Build the bounded graphite/cyan open-glove side-elevation sprite present in that supplied row so it follows the terminal manipulator segment, mirrors with facing and chain side, overlaps the forearm, and remains presentation-only. Apply it graph-wise to every valid manipulator terminal, tolerate zero/one/many manipulators, preserve weapon mounting, and add source-lock, load/key/padding/color, translated/rotated/mirrored, missing/malformed, all-rig, vertex-budget, and package-eye coverage. Remove the rejected generated hand source and prove the installed package contains only the supplied-sheet derivative.

### WALK-SHUTTLE-COURSE-383 - Train on a compact bidirectional dynamic course
**Status:** COMPLETED LOCALLY - FINITE SHUTTLE, DELAYED DYNAMIC HAZARDS, CLEANUP, TERRAIN SYNC, AND PACKAGE EYE PASS

Replace the effectively unbounded one-way preview course with a finite shuttle arena. Each traversal starts with a short controlled reverse segment, transitions through a stable turn that flips facing, runs toward the opposite boundary, and repeats symmetrically. Spawn lesson-appropriate obstacles dynamically ahead of the active traversal only after readiness, retire their physics/render/observation state after the evaluation or lesson completes, and never let stale obstacles leak across retries, direction changes, rig switches, checkpoints, or mastery boundaries. Keep terrain pressure active and collision/render synchronized while bounding map length, camera framing, object count, policy observations, and memory. Add lifecycle, left/right symmetry, reverse-turn-forward sequencing, no-early-object, cleanup, repeated-seed, all-rig, checkpoint, and 20/60/240 Hz state-equivalence diagnostics plus installed Vulkan eye evidence.

Linux GCC 14 strict cold evidence remains release-blocking after the strict-retention correction: Windows retained all five subjects, while Linux retained biped, humanoid, quadruped, and crawler but produced no strict hexapod champion before the bounded gate; its terminal raw evaluations clustered on overspeed and flip despite a valid physical reference. Audit topology-scaled multi-support action authority, speed shaping, rollout schedule, evaluation cadence, and strict publication timing. The fix must improve the actual six-support learner rather than relaxing strict validity, accepting partial quality, or extending one platform with an arbitrary timeout. Require repeated-seed retained replay on both platforms before packaging.

The structural learner correction passed the unchanged bounded strict gate on both platforms: Windows retained all five rigs in 736.10 seconds, and Linux GCC 14 retained all five—including the previously missing hexapod—in 570.47 seconds. Validity, six-seed replay, zero authority, topology-specific distance/cycle requirements, and the 1,200-update budget were not relaxed. The final application relink and complete platform matrices remain required because the later optimization-mode work changes shared autonomy objects even though it does not change PPO behavior.

The explicit optimization implementation now compiles warning-clean under MSVC and GCC 14. Focused tests pass control anatomy immutability, deterministic bounded topology growth, invalid-mode fallback, explicit complexity pressure, asynchronous v17 morphology-mode save/load, v16 safe default, adversarial invalid persisted mode, and exact temporary-file cleanup. Publication evaluation is six deterministic raw-policy seeds with no lesson action blend; mode-specific UI/package eye evidence and the full matrices subsequently passed.

### WALK-RELEASE-384 - Build and audit the next local Runner release
**Status:** COMPLETED LOCALLY - V0.7.32 SOURCE, WINDOWS/LINUX MATRICES, PACKAGE, MANIFEST, AND VULKAN EYES PASS

Integrate missions 380-383 coherently, advance source/package identity, update missioncache, changelog, README, focused docs, install lists, workflow contracts, and tests, then run the complete Windows, Linux GCC 14 warnings-as-errors, diagnostic, installed/extracted launcher, manifest/checksum, and direct eye gates. Produce a clean local package and commit only. Do not push, tag, publish, or mutate remote state unless the user later authorizes it.

## v0.7.32 local release evidence

- The compact shuttle uses one signed facing/travel contract for reverse, turn, forward gait, support/manipulator targets, contacts, obstacle approach, observations, rewards, camera, equipment aim, sprite mirroring, and near/far ordering. Reverse and turn are bounded; dynamic obstacles remain absent before physical readiness and are retired across turns, retries, lesson changes, rig switches, and completion.
- Active pressure deformation remains enabled, but renderer geometry is derived from the same `ground_height_at` collision samples. Deterministic course diagnostics cover delayed pressure, sand, waterlogged sand, water, holes, synchronized contact, repeated seeds, and exact 20/60/240 Hz state equivalence.
- Authored foundational gait bends both knees to one sagittal side, swings topology-discovered arm chains in opposition, suppresses gait drive while turning, and consolidates strict post-handoff champions for every topology. No retained forward-gait champion may omit the strict-valid quality bit.
- Static Crouch now drives bilateral support-chain flexion instead of a hip fold, reaches exact zero teacher authority at lesson-local update 200, and retains the original depth, hold, support, recovery, repeated-seed, and forward-hinge rejection gates.
- The runtime hand is cropped only from the strict Side View row of `tools/art_sources/runner_user_modular_sheet.png` (source SHA-256 `D1DB49B2C376A87A060B9BB18402F8374C1F5730E132E51F5385C1DCC28F1195`). The installed `hand_side.ppm` SHA-256 is `BCD64BA364A4E6A9073CB11B4AE23F598426CB1258D97D0101CBBFE9CD625EDF`; the rejected generated substitute is absent from source, install, and manifest.
- Rig Lab exposes seven graph-distinct canonical side-view subjects rather than aliases. `CONTROL OPTIMIZE` keeps anatomy byte-stable; `MORPHOLOGY EVOLVE` alone applies bounded connected graph mutations, neutralizes new action slots, adapts, evaluates six held-out raw-policy seeds with explicit complexity cost, and accepts or rolls back. State v17 round-trips the rig-scoped mode; v16 and malformed values fall back safely to control mode.
- The final strict learner gates retained all five subjects without conveyor motion or teacher authority. Windows passed in 736.10 seconds; Linux GCC 14 passed in 570.47 seconds. Final retained replay was biped `23.6746 m / 31.50`, humanoid `23.7014 m / 27.17`, quadruped `12.4292 m / 46.00`, crawler `16.1524 m / 41.83`, and hexapod `22.5489 m / 56.17`, each with `0/6` invalid seeds.
- The complete Windows SDL3/Vulkan build and 30-test matrix pass, including the cold learner and corrected raw Crouch learner. The GCC 14 warnings-as-errors build and effective 25-test matrix pass; the sole stale UI-wording assertion exposed by the uninterrupted run was corrected to the structural no-Scaffold-button contract and passed focused re-execution without product-source change.
- Production diagnostics pass version, Vulkan, package, 24/24 acceptance, camera, five-layout DPI UI, modular-art vertex budget, synchronized course, five-rig training, and headless walk-eye. The fresh walk proof retained update 1030 at authority `0.000`, averaged `23.8 m / 21.5` real steps over six valid seeds, and displayed a direction-matched `23.8 m / 19-step` physical replay.
- The installed and independently extracted 48-file package each passed `run.bat` from an unrelated system temporary directory. The extracted tree matched the per-file SHA-256 manifest 48/48. The outer ZIP checksum is intentionally recorded after the embedded ledger is finalized so the archive does not depend on its own checksum.
- DPI-aware physical-screen Vulkan evidence supersedes invalid `PrintWindow` captures, which cannot capture the Vulkan swapchain. Accepted SHA-256 images are: course `2A365C7CF6BB1D3B94256FDE854B22858A9859CF341FD26E86E69A697F433046`, orthographic assembled art `7D1E74472EE4349F32C7BAAE910D50C0E93CBB7EC5BE95FB0CF15035C15E9CA5`, Humanoid plus selected Morphology Evolve Rig Lab `E2F347BEAC7E4ED1667306333E9AB925A395DB886303097B829741AEFF29B658`, and packaged retained walk `66F01914782340F2DE5C0105511C9D65047ED6C63AC19B5AF6B094EE429BC33D`.
- Authority remains local only. No tag, push, GitHub release, remote branch mutation, or published-asset operation was performed.
# Runner v0.7.33 full-limb facing and granular sky-hazard response

**Release state:** COMPLETE — LOCAL V0.7.33 AUDITED; REMOTE UNTOUCHED

The 2026-08-13 04:29:50 and 04:30:52 packaged screenshots outrank v0.7.32 automated claims. They show a left-facing helmet and torso beside limb/boot sprites that retain their prior local orientation, fingertip-sized terminal hands, and a deforming height band rather than the requested falling/stacking granular hazard simulation. Fix the shared transform and hazard systems; do not patch only the frozen screenshot pose.

### WALK-LIMB-FACING-385 — Mirror the complete articulated armor with rig facing
**Status:** COMPLETED LOCALLY - COMPLETE ARTICULATED MIRROR CONTRACT AND VULKAN EYE PASS

Use one whole-presentation reflection contract for upper arms, forearms, thighs, shins, boots, hands, graph bones, near/far layers, and equipment—not only torso and helmet. Preserve proximal/distal joint ownership while reflecting each sprite’s transverse axis, point terminal boots along signed facing even for vertical and fallen supports, and avoid double-reflection when the physical segment already points left. Cover both facings, arbitrary segment rotations, vertical limbs, fallen rigs, both branch sides, all seven canonical rigs, turn boundaries, and repeated 20/60/240 Hz poses with pixel/transform assertions plus live Vulkan eyes.

### WALK-HAND-SCALE-386 — Fit supplied hands at readable anatomical scale
**Status:** COMPLETED LOCALLY - SUPPLIED HAND FIT, BOUNDS, AND PACKAGE PASS

Keep the exact hash-locked Side View glove from the supplied modular sheet, but size it from the terminal forearm thickness and source aspect ratio so the opaque glove reads as a hand instead of fingertips. It must overlap the wrist without swallowing the forearm, stay bounded across compact/large/malformed rigs, mirror with facing and branch side, preserve equipment mounting, and remain presentation-only. Add opaque-bounds, minimum/maximum screen span, translated/rotated/mirrored, all-rig, and packaged close-up evidence.

### WALK-GRANULAR-SKY-387 — Simulate falling, settling, stacking terrain hazards
**Status:** COMPLETED LOCALLY - FIXED-STEP FALL, SETTLE, STACK, SAFETY, AND CLEANUP PASS

Replace height-band-only hazard behavior with deterministic granular material events that can spawn above the course, fall under fixed-step gravity, collide with the current terrain and rig-independent course obstacles, settle, stack, conserve bounded material, and alter the same collision/render surface. Seeded excavation may form traversable or dangerous holes. Moving blocks and active falling/rolling material are hazards that the controller observes and avoids while unsafe; after material and blocks remain below bounded motion/impact thresholds for a measured safe window, the resulting pile, depression, or block arrangement becomes terrain to traverse. Keep launch protection and early curriculum learnability, then scale rate, mass, height, hole depth, block motion, and combinations with lesson difficulty. Retire every particle/block/hole and its observation state across retries, turns, lesson transitions, rig switches, completion, and checkpoints. Add positive fall/settle/stack/hole/block cases, negative early-spawn and premature-traverse cases, adversarial bounds/non-finite inputs, repeated seeds, conservation, collision/render synchronization, all-rig observations/rewards, lifecycle cleanup, and exact 20/60/240 Hz state equivalence.

### WALK-RELEASE-388 — Build and audit corrected local Runner v0.7.33
**Status:** COMPLETED LOCALLY - V0.7.33 WINDOWS/LINUX MATRICES AND LOCAL PACKAGE PASS

Integrate missions 385–387 coherently, advance source/package/checkpoint identity, update cache/changelog/README/focused docs/install/workflow/tests, run complete Windows SDL3/Vulkan and GCC 14 warnings-as-errors matrices, long cold Walk and Crouch learners, every diagnostic, installed and independently extracted launchers, manifest/checksum audits, and DPI-aware Vulkan eyes showing both complete facings, readable hands, an active falling hazard, its settled traversable result, a hole, and a moving block. Produce a clean local commit and package only. Do not push, tag, publish, or mutate remote state without later authorization.
### WALK-MATERIAL-TRUTH-389 - Make authored surface labels, physics, and visuals agree
**Status:** COMPLETED LOCALLY - AUTHORITATIVE MATERIAL/COLLISION/RENDER CONTRACT PASS

The surface identified as `DRY DEFORMABLE SAND` must own the granular deformation, collision response, deposition, excavation, and render change. Firm dirt may support or border it but must not visibly deform while the labeled sand stays static. Derive labels, colors, collision height, particle deposition, hole geometry, and diagnostics from one authoritative material field so no visual band can impersonate another material. Add sand-impact, dirt-negative, boundary, deposition, excavation, render/collision synchronization, repeated-seed, and exact 20/60/240 Hz coverage.

### WALK-WEAPON-RUNTIME-390 - Restore visible, functional equipment
**Status:** COMPLETED LOCALLY - GRAPH-MOUNTED EQUIPMENT, PROJECTILES, HITS, AND CLEANUP PASS

Equipped weapons must mount to the graph-discovered manipulator terminal, render at readable side-view scale in both facings, follow the complete articulated transform, and remain visible in live preview, retained replay, and Rig Lab. A fire action must create a bounded fixed-step projectile or melee effect with deterministic aim, collision, hit telemetry, reward/penalty ownership, and cleanup; unarmed state must create none. Preserve gait and hand presentation, avoid fixed motor slots or humanoid-only assumptions, and clear equipment/projectiles across retry, turn, lesson, rig, and checkpoint boundaries. Add visible-mount, both-facing, arbitrary-rotation, fire/no-fire, collision/miss, all-rig, cleanup, repeated-seed, and 20/60/240 Hz tests plus packaged Vulkan evidence.
### WALK-PROPORTION-GAIT-391 - Correct assembled proportions and single-plane joint bends
**Status:** COMPLETED LOCALLY - GRAPH-DERIVED PROPORTIONS AND STRICT ZERO-AUTHORITY GAIT PASS

Bound visual and physical thigh, shin, torso, head, hand, and boot proportions from one graph-derived anatomy profile so the side-view rig no longer has extremely long legs, while preserving the current authored upper-arm and forearm reach exactly. Every paired support chain must bend its knees toward one anatomically consistent sagittal side, and every paired manipulator chain must bend its elbows consistently while retaining opposed arm swing; whole-rig facing reflection must reverse presentation without reversing joint ownership or creating X/scissor/crab gait. Preserve genuinely different topology configurations, morphology evolution bounds, contacts, masses, motor limits, observations, action semantics, retained policies, and art-only presentation separation. Add ratio bounds, both-facing bend-sign, arbitrary-pose, all-seven-rig, edited/evolved-rig, repeated-seed, no-X-gait, and packaged close-up/walk Vulkan evidence.
### WALK-RIGLAB-LAYOUT-392 - Remove Rig Lab diagnostic text overlap
**Status:** COMPLETED LOCALLY - MEASURED TEST ROWS AND DPI LAYOUT PASS

The Test page must allocate separate measured rows for joint selection, range actions, result/status labels, and manual-input label/value/slider at supported window sizes and DPI scales. No status string may render over buttons, no value may collide with its label, and wrapped equipment help must remain inside the panel. Preserve hit targets, enabled/selected states, keyboard/mouse control, panel clipping, and test semantics. Add minimum/recommended/large window, 100/125/150/200% DPI, long-status, all-tab, hit-test, and packaged Vulkan screenshot coverage.
### WALK-RIGLAB-WORLD-393 - Make the rig editor world readable and correctly framed
**Status:** COMPLETED LOCALLY - ACTIVE-GRAPH FIT, SUBDUED GHOSTS, AND LABEL CULLING PASS

Fit the complete edited/tested rig inside the world viewport with bounded margins and a visible ground reference instead of leaving it low and far right. The active rig must remain visually dominant: range/sweep/reference ghosts use bounded count and low alpha, graph lines stay legible, node radii are screen-bounded, labels are offset and culled to avoid one another and the body, and unsupported separator glyphs must not render as `?`. Preserve drag/add/connect/select hit testing, node semantics, motor range evidence, current-value markers, art/skeleton modes, resize, and DPI behavior. Add framing, label-overlap, ghost-budget, glyph, minimum/recommended/large viewport, 100/125/150/200% DPI, all-rig, and packaged Vulkan screenshot coverage. The 2026-08-13 04:46:05 screenshot further reopens this mission: the authored rig is reduced to an overlapping lower-right knot, range ghosts dominate the viewport, root/node disks obscure connected structure, labels collide with nodes and each other, most of the viewport is empty, the ground reference is detached from useful framing, and unsupported separators render as question marks. The readable active graph—not range ghosts—must determine fit and visual hierarchy, with the full authored topology centered above its ground reference.
The 2026-08-13 04:46:05 frame also proves the Test panel is still unusable at the shipped panel width: pattern buttons, traction result, and the manual-input value/slider occupy the same vertical band. The finished page must make the authored rest graph unmistakable, show only a subdued bounded test-pose overlay, enlarge the useful graph fit without clipping, and give every control/status row its own measured rectangle.

### WALK-FACING-IK-394 - Make physical bend orientation follow facing through shuttle reversal
**Status:** COMPLETED LOCALLY - STABLE SHUTTLE REVERSAL AND COMPLETE PRESENTATION FACING PASS

The v0.7.33 full matrix showed right-facing biped teacher probes falling during the boundary backup and every leftward traversal retaining the world-right knee/elbow solution. Drive two-link knee and elbow bend orientation from rig facing, not a constant world sign or signed travel intent: controlled backing retains the current facing anatomy, the completed turn mirrors physical bend direction, and forward travel in both directions preserves crossings without shank scissor. Replace the obsolete v0.7.18 normalized-command sign assertion with physical/target-space opposite-phase and same-facing-bend evidence. Require all ten deterministic teacher seeds for every retained topology, full 20-second survival, the existing distance/stride/crossing gates, reverse/turn symmetry, and 20/60/240 Hz equivalence.

### WALK-STRICT-HUMANOID-395 - Retain the corrected humanoid gait after zero-authority handoff
**Status:** COMPLETED LOCALLY - SIX-SEED ZERO-AUTHORITY HUMANOID RETENTION PASS

The first v0.7.33 cold matrix retained biped, quadruped, crawler, and hexapod, but the shortened humanoid reached 21.61 m and 40.83 stride events without a retained strict controller because all six raw seeds accumulated lateral-gait plus lower-leg-scissor rejection. Correct physical facing/bend and the topology-scaled gait learning signal so a raw humanoid policy produces real limb crossings without relaxing rejection thresholds, strict validity, six held-out seeds, zero teacher authority, or the 1,200-update budget. Re-run focused deterministic reference and learner gates before the complete Windows/Linux matrices.
## v0.7.33 final local release evidence

- One graph-derived presentation transform now owns full-limb, boot, supplied-sheet hand, torso, helmet, bone, near/far, and equipment facing. The procedural body fallback remains excluded while the real graph stays visible beneath the fitted armor. The accepted packaged Vulkan art frame is SHA-256 `577CC2344E86BF9E02A0D3B2DB9AA7DFC0C00CFBBD6EF8E741460E5360B57467`.
- Humanoid support proportions are shortened without changing authored arm reach. The 1.51 Hz foundational gait keeps the proven anatomy-scaled stride, uses a taller flexed leg target, suppresses walking drive during backing/turning, and excludes only the first 0.50 seconds of spawn settling from forward-gait quality telemetry. The sustained scissor rejection boundary remains 0.34 seconds; the packaged walk eye measured 0.250 seconds.
- Ten-seed physical reference replay passes for biped, humanoid, quadruped, crawler, and hexapod. The complete Windows cold learner retained strict raw zero-authority champions for all five rigs. The packaged replay retained biped `26.7038 m / 56.67`, humanoid `23.3650 m / 55.50`, quadruped `11.7019 m / 43.33`, crawler `9.6975 m / 27.50`, and hexapod `22.3869 m / 55.50`, each with zero invalid retained seeds and conveyor motion disabled.
- Falling sand/mud particles, excavation, deposition, stacking, moving blocks, unsafe-motion observation, measured safe dwell, and lifecycle cleanup run on the fixed simulation clock. Mutable sand/mud and structural dirt share one material field with collision, render, labels, and diagnostics. Course/equipment/terrain state is identical across 20/60/240 Hz render schedules. The accepted packaged course Vulkan frame is SHA-256 `1ADA7530E50EDD9A4F138C4D86AC5023C6235312E2677B2B068C5D36C1B4B652`.
- Weapons mount on topology-discovered manipulator terminals, follow facing and arbitrary segment rotation, render carried/dropped/Rig Lab states, fire bounded fixed-step projectiles, record deterministic hits, and clear across every retry/turn/lesson/rig/checkpoint boundary. Unarmed state cannot fire.
- Rig Lab Test controls use separate measured selection, range, pattern, status, and manual-slider rows. Active topology and ground determine editor fit; the test ghost is thin/low-alpha, node radii are bounded, labels use collision-aware placement/culling, and unsupported separator glyphs are absent. Five supported layouts at 150% DPI pass the production UI compositor diagnostic.
- Final Windows SDL3/Vulkan build and 30/30 CTest matrix pass in 888.24 seconds, including the 815.13-second five-rig learner and 44.02-second raw Crouch learner. Linux GCC 14 warnings-as-errors and 25/25 CTest matrix pass in 675.69 seconds, including the 606.43-second learner and 42.08-second Crouch gate.
- Installed and independently extracted launchers pass version, Vulkan, package, 24/24 acceptance, camera, five-layout UI, modular-art budget, active course/material/equipment/frame-independence, five-rig training, and walk-eye diagnostics from an unrelated working directory. The independently extracted tree matches the 49-entry per-file SHA-256 manifest exactly.
- The packaged walk-eye proof passes at update 1,200 with a champion retained at update 1,115, authority `0.000`, `24.586 m / 55.67` six-seed mean, `0/6` invalid seeds, and a displayed `25.277 m / 55-step / 17-crossing` physical replay.
- Training semantics are `0x0007'3302`, checkpoint magic is `EPPO33`, and v0.7.32/v0.7.31/v0.7.30 fallback imports lifetime totals only. Authority remains local: no push, tag, PR, release upload, or remote mutation was performed.

# Runner v0.7.34 curriculum-safe rig optimization recovery

**Release state:** COMPLETE — LOCAL V0.7.34 AUDITED; REMOTE UNTOUCHED

### WALK-CROUCH-RIG-RESET-396 — Stop accepted rig candidates from restarting the active lesson
**Status:** COMPLETE — LESSON AGE AND MASTERY STREAK SURVIVE CANDIDATE SCHEDULING

The user's actual extracted v0.7.33 state is authoritative. It is on Static Crouch with 113 rig candidates, 48 accepted control changes, and 65 rejected changes, while the binary checkpoint retains only 15 lesson updates. In v0.7.33, `attempt_rig_evolution_locked()` transferred only policy weights into a fresh four-update nursery; on acceptance it replaced the main blueprint and then applied that nursery checkpoint. This repeatedly erases Crouch's progress toward the exact update-200 zero-authority/mastery boundary. Defer control and morphology candidates until the current lesson has completed its measured work and any finite teacher handoff, and pause candidate publication whenever a valid mastery-confirmation streak is in progress so eight consecutive confirmations cannot be interrupted. Retarget a candidate checkpoint to the candidate rig while preserving the main lesson clock, optimizer, cumulative metrics, and training stage; invalidate only champion evidence that belongs to the previous physical rig. Positive accepted-candidate, rejected-candidate, rollback, exact Crouch boundary, Walk teacher boundary, morphology, checkpoint-resume, repeated-sequence, and 20/60/240 Hz coverage is required.

### WALK-RIG-LEDGER-397 — Preserve lifetime and stage totals through accepted candidate publication
**Status:** COMPLETE — RETARGETED CANDIDATES PRESERVE THE MAIN LEDGER AND OPTIMIZER

An accepted control or morphology candidate may add its bounded adaptation work, but it may not replace the main trainer's all-time updates, environment steps, episodes, resets, distance, time, evaluations, or current lesson age with the nursery's local four-update ledger. Candidate acceptance must be monotonic for every cumulative field, preserve finite odometry constraints, and publish honest rig-generation/accepted/rejected/rollback telemetry. Add adversarial high-ledger, repeated-acceptance, autosave round-trip, rig-signature, and no-candidate-before-readiness tests.

### WALK-RELEASE-398 — Build and audit corrected local Runner v0.7.34
**Status:** COMPLETE — LOCAL V0.7.34 PACKAGE AUDITED; NO REMOTE MUTATION

Advance source/checkpoint/state/package identity; import only validated v0.7.33 lifetime totals when no current state exists; update missioncache, changelog, README, focused docs, install/audit contracts, and deterministic tests; then run repository hygiene, Windows SDL3/Vulkan and Linux GCC 14 warnings-as-errors matrices, all production diagnostics, installed and independently extracted launchers, ZIP checksum/manifest audit, and direct packaged evidence. Produce a clean local package and commit only. Do not push, tag, publish, or mutate remote state.

### V0.7.34 completion evidence

- The user's extracted v0.7.33 state reproduced the failure at Static Crouch after 113 candidate attempts (48 accepted, 65 rejected) while the checkpoint retained only 15 lesson updates. Candidate nurseries had been publishing a four-update transfer-only checkpoint over the main trainer and clearing mastery. V0.7.34 admits no automatic rig candidate before the complete lesson/teacher boundary and admits none while a valid consecutive mastery streak is in progress.
- Candidate adaptation now retargets the complete compatible checkpoint, preserves policy parameters, Adam moments, optimizer step, lesson age, stage, difficulty, RNG, and every cumulative total, neutralizes newly activated action-slot parameters and both Adam moments, and invalidates only evaluation/champion evidence belonging to the old physical rig. Training semantics are `0x0007'3401`, checkpoint magic is `EPPO34`, and automatic files are isolated under `runner-v0734-curriculum-*`; v0.7.33 fallback imports lifetime totals only.
- Deterministic v0.7.34 coverage passes for the negative Stand case, incomplete Crouch work, exact Crouch update-200 boundary, active 1/8 mastery-streak exclusion, topology-specific Walk handoff, insufficient episodes/evaluations, 20/60/240 render cadence, high-ledger candidate retarget, parameter/Adam preservation, new-action moment neutralization, old-rig evidence invalidation, bounded nursery adaptation, rig-signature change, and checkpoint round-trip.
- Windows SDL3/Vulkan Release build and 31/31 CTest pass in 836.25 seconds, including the 763.75-second five-rig cold learner and 43.57-second raw Crouch learner. Linux GCC 14 warnings-as-errors build and 26/26 CTest pass in 716.12 seconds, including the 641.11-second learner and 47.64-second Crouch gate.
- Build-tree, installed, and independently extracted launchers pass version, Vulkan, package, 24/24 acceptance, camera, five-layout UI, modular-art budget, active course/material/equipment/frame-independence, five-rig training, and walk-eye diagnostics from an unrelated working directory. Retained raw-policy results remain biped `26.7038 m / 56.67`, humanoid `23.3650 m / 55.50`, quadruped `11.7019 m / 43.33`, crawler `9.6975 m / 27.50`, and hexapod `22.3869 m / 55.50`, with conveyor disabled.
- Walk-eye passes at update 1,200 with a champion retained at update 1,115, authority `0.000`, `24.586 m / 55.67` six-seed mean, `0/6` invalid seeds, and displayed `25.277 m / 55-step / 17-crossing` physical replay. The local archive has a 50-entry per-file SHA-256 manifest. Authority remained local: no push, tag, PR, release upload, or remote mutation was performed.

# Runner v0.7.35 proportional PIP, honest lineage telemetry, and anti-brace walking

**Release state:** COMPLETE — LOCAL V0.7.35 AUDITED; REMOTE UNTOUCHED

The packaged v0.7.34 eye test is authoritative. The Walk preview survives and produces real distance/steps, but the main replay leans backward as though resisting a push and the PIP compresses the same authored rig into a large-head, large-boot chibi silhouette. The user's extracted v0.7.34 autonomy state is already on Walk with zero control/morphology candidates and five rollbacks, so this is not the fixed v0.7.33 candidate-publication reset. The user may have explicitly started a fresh controller once; therefore TOTAL RIG UPDATES 630, POLICY AGE 38, and their difference 592 can legitimately describe prior policy lineage rather than discarded optimizer work. Presentation, qualification, reward, telemetry, persistence, diagnostics, and packaging must agree with that distinction.

### WALK-PIP-PROPORTIONS-399 — Preserve assembled anatomy at every preview scale
**Status:** COMPLETE — PROPORTIONAL ASSEMBLY, LOWER HELMET, SIDE REST, AND UPRIGHT FACING PASS

The follow-up eye test also shows an overlong/high helmet fit, arms whose authored rest pose does not begin at the sides, and the rear/left hand art rotating into a reversed or upside-down presentation. Shorten/lower the biped helmet envelope without rescaling the skeleton; derive arm rest/facing from semantic chains; and keep the supplied hand texture upright along the wrist-to-hand axis for both branch depth and both travel directions.

Remove fixed screen-pixel minima that make the helmet, boots, hands, torso, and limb thickness stop shrinking while the PIP skeleton continues shrinking. Derive every assembled-art dimension from one pixels-per-metre presentation transform so PIP, main preview, retained replay, Rig Lab, both facings, all canonical rigs, edited/evolved rigs, and malformed fallback graphs preserve the same bounded anatomical ratios. The supplied side-view modular assets remain authoritative; no procedural/legacy body art may return. Add main-versus-PIP normalized-ratio, small/large scale, both-facing, all-rig, arbitrary-pose, resize/DPI, and packaged Vulkan screenshot coverage.

### WALK-BACKWARD-BRACE-400 — Reject and train away sustained backward-braced gait
**Status:** COMPLETE — DIRECTIONAL SUSTAINED-BRACE REJECTION AND FRAME-INDEPENDENT TIMING PASS

Measure directional backward torso/head displacement relative to the authored root-to-torso axis and requested locomotion direction using fixed-step state. Allow bounded natural gait oscillation, recovery, backing before a turn, and non-upright body plans, but penalize sustained backward bracing during forward upright traversal and reject it from retained Walk/Run candidates before it can be labelled valid. Keep the existing distance, real-stride, crossing, support, crab/scissor, and zero-authority mastery gates strict. Expose the exact brace reason and duration in diagnostics. Add positive natural lean, negative sustained brace, adversarial facing/reversal/backing, non-biped, repeated-seed, and exact 20/60/240 Hz coverage.

### WALK-LINEAGE-TRUTH-401 — Distinguish fresh-policy resets from discarded work
**Status:** COMPLETE — PRIOR-LINEAGE TELEMETRY AND STAGE-SAFE RETENTION WORDING PASS

Replace the misleading derived DISCARDED = total rig updates - policy age label with neutral prior-lineage telemetry. A manual fresh-controller action may reset the active policy age while preserving rig/all-time totals, but the display must not infer that prior updates were silently rejected or claim one cause when legacy state does not persist it. Separate an incremental stage-safe retained candidate from final lesson mastery in the summary and PIP wording. The user clarified that update length/reset provenance was not the active defect, so v0.7.35 deliberately does not add a new persisted reset-cause subsystem; it closes this mission with underflow-safe lineage arithmetic, stage-safe language, legacy-import compatibility, and no false discard claim.

### WALK-RELEASE-402 — Build and audit corrected local Runner v0.7.35
**Status:** COMPLETE — LOCAL PACKAGE, MANIFEST, AND INDEPENDENT EXTRACTION AUDITED

Linux GCC 14 warnings-as-errors caught an implicit cadence integer-to-float conversion in the new v0.7.35 deterministic test while all production translation units compiled cleanly; the test bound must cast explicitly before the matrix can continue.

Advance source/checkpoint/state/package identity; update changelog, README, focused documentation, install/audit/release contracts, and deterministic tests. Run repository hygiene, Windows SDL3/Vulkan and Linux GCC 14 warnings-as-errors matrices, all feature diagnostics including camera and packaged art/gait eyes, installed and independently extracted launchers from unrelated directories, ZIP checksum/per-file manifest audits, and direct PIP/main comparison evidence. Produce a clean local package and commit only. Do not push, tag, publish, or mutate remote state.

### V0.7.35 completion evidence

- One proportional pixels-per-metre transform now sizes the assembled helmet, torso, limbs, boots, joints, supplied-sheet hands, and equipment at every preview scale. The helmet envelope is shorter and lowered, canonical humanoid hands rest beside the pelvis without changing segment reach, and whole-rig facing mirrors the supplied side-view limb/hand art without independently flipping the rear branch upside down. The accepted direct PIP/main eye frame is `runner-v0735-art-eye-window.png` in the local validation workspace.
- Walk qualification tracks sustained backward bracing only during supported forward upright traversal, after the gait grace period and outside backup/turn states. It is direction-aware, finite-safe, topology-safe, exposed as an exact rejection reason, and identical at 20, 60, and 240 Hz. Existing distance, real-step, crossing, crab/scissor, motion-validity, and zero-authority gates remain strict.
- The UI now reports `PRIOR LINEAGE` rather than deriving a misleading `DISCARDED` count. The user's clarification that update length was not the active defect is preserved: ordinary retries and candidate work do not become reset claims, while a legitimate fresh-controller action may leave current policy age below rig lifetime updates. Retained-candidate text is explicitly stage-safe and does not claim final lesson mastery.
- Windows Visual Studio 2022 Release rebuilt the complete SDL3/Vulkan product and passed 32/32 CTests in 747.16 seconds, including the 747.16-second cold learner and 51.86-second raw Crouch gate. Linux GCC 14.3 rebuilt with warnings-as-errors and passed 27/27 CTests in 627.32 seconds, including the 626.97-second cold learner and 47.50-second Crouch gate. The new test-only cadence conversion was made explicit and the focused v0.7.35 suite passed on both platforms.
- Build-tree, installed, and independently extracted launchers pass version, Vulkan, package, 24/24 acceptance, camera, five-layout UI, modular-art budget, active course/material/equipment/frame-independence, five-rig training, and walk-eye diagnostics. Independently extracted retained results are biped `25.2511 m / 56.17`, humanoid `20.2592 m / 56.17`, quadruped `11.7019 m / 43.33`, crawler `9.6975 m / 27.50`, and hexapod `22.3869 m / 55.50`, all with zero controller authority and conveyor motion disabled.
- The independently extracted walk-eye proof passes at update 1,200 with a champion retained at update 1,130, authority `0.000`, `22.723 m / 55.17 steps` over six seeds with `0/6` invalid, and a displayed `23.739 m / 57-step / 16-crossing` replay; sustained scissor duration is `0.233 s`. The course diagnostic passes launch contact, active terrain, delayed objects/pressure, material variation, water/holes, observations, climbing, equipment on/off, and frame independence.
- Training semantics are `0x0007'3501`, checkpoint magic is `EPPO35`, and compatible legacy checkpoints retain their validated lifetime totals. Authority remained local: no push, tag, PR, release upload, or remote mutation was performed.
# Runner post-v0.7.35 repository garbage cleanup

**Cleanup state:** COMPLETE — OBSOLETE ARTIFACTS AND GENERATED CACHES REMOVED; V0.7.35 AND V0.7.36 RETAINED

### WALK-REPOSITORY-CLEANUP-403 — Remove obsolete generated and superseded release artifacts
**Status:** COMPLETE — TRACKED SUPERSEDED ASSETS AND GENERATED CACHES REMOVED

Remove generated build trees, vcpkg install caches, stale runtime autosaves/rig outputs, obsolete validation captures, prior release-download staging, the ignored root v0.7.33 archive, and tracked release archives superseded by the audited v0.7.35 local release. Preserve all authored source, assets, tests, documentation, mission history, Git metadata, and the v0.7.35 ZIP/checksum/manifest. Verify every recursive target resolves inside this workspace, report recovered bytes, run repository hygiene and git diff --check, confirm the retained v0.7.35 checksum, and commit only the tracked cleanup locally without rewriting history or mutating any remote.
# Runner v0.7.36 authored gait, bounded engagements, staged shuttle, and stable active terrain

**Release state:** COMPLETE — LOCAL V0.7.36 AUDITED; REMOTE UNTOUCHED

The user's modified `creature.rig` from the extracted v0.7.35 package is the authoritative humanoid rest shape (SHA-256 `C7A6CBAB58E945BCC70ADA3ED59170B522B47CAEFBC2CBAEA149349862A46DAD`). The 2026-08-15 23:29:48, 23:32:37, and 23:32:42 screenshots outrank earlier automated claims: authored arms are replaced by an outward balancing target, equipment fires continuously, Walk owns an unrelated shuttle reversal, and ordinary retries visibly replace the terrain/material layout seconds apart.

### WALK-AUTHORED-GAIT-404 — Animate semantic limbs around the edited rig's rest pose
**Status:** COMPLETE — AUTHORED REST, SEMANTIC CHAINS, AND OPPOSED SWING ENFORCED

Promote the supplied humanoid graph exactly, including its lower head and compact layered arm rest. Discover paired support and manipulator chains from graph semantics rather than fixed action slots. At rest, both arm branches remain layered beside the torso in the same side-view convention as the paired legs; during locomotion, arms counter-swing and feet/legs swing in mirrored opposition around their authored rest vectors. Knee and elbow joints keep one anatomical bend direction per facing, backing preserves current facing, and completed turns reflect the entire physical/art rig without reversing proximal/distal ownership. Bound balance, motor-discovery, teacher, policy, and equipment contributions so none can fling manipulators outward or erase edited/evolved rest geometry. Preserve meaningful topology variation and make the contract reusable for enemy AI rigs. Add exact supplied-rig hash/coordinates, zero-swing rest, opposed swing, both-facing/backing/turn, policy-adversarial, edited/evolved/all-rig, and exact 20/60/240 Hz tests.

### WALK-ENGAGEMENT-FIRE-405 — Fire only inside authored engagement windows
**Status:** COMPLETE — CLASS RANGE, AIM, CADENCE, SAFETY, AND HIT-GOAL GATES ENFORCED

Give every equipment class a bounded minimum/maximum engagement range, aim tolerance, cadence, and per-encounter completion budget. Fire only while an active target is inside that window, the weapon is ready, aim is valid, and the current lesson/encounter still needs a hit; cease immediately after completion, outside range, during unsafe terrain motion, and across retry/turn/lesson/rig/checkpoint cleanup. Policy actions must not bypass the same simulation-side gate. Add boundary, too-near, too-far, cooldown, completed-goal, adversarial action, both-facing, all-equipment, repeated-seed, and 20/60/240 Hz tests.

### WALK-SHUTTLE-LESSON-406 — Make reverse/turn/return its own curriculum stage
**Status:** COMPLETE — DEDICATED LEGACY-COMPATIBLE SHUTTLE CURRICULUM STAGE

Keep foundational Walk/Run a forward locomotion lesson. Move short controlled backing, stable whole-rig turn, opposite-direction traversal, and symmetric return into a named dedicated lesson placed after Walk without renumbering persisted legacy stage identifiers. Give the shuttle its own work boundary, mastery tests, goals, telemetry, dynamic obstacle lifecycle, and strict forward/back/turn phase qualification. Preserve checkpoint compatibility and prevent ordinary Walk from reversing merely because it reached a preview boundary. Add stage-order, legacy-load, forward-only Walk, full shuttle sequence, both-direction, cleanup, repeated-seed, and exact 20/60/240 Hz tests.

### WALK-TERRAIN-LIFECYCLE-407 — Preserve one active material field through ordinary retries
**Status:** COMPLETE — COURSE LAYOUT PERSISTS THROUGH ORDINARY RETRIES

Separate episode dynamics from course-layout ownership. Construct or clear the sand/dirt/water/hole field only at an explicit lesson, evaluation, rig, or course boundary; ordinary controller retries must reset the rig without replacing the material patches or snapping settled/deformed terrain to a new seed. Fixed-step sand/dirt fall, stack, excavation, pressure, water, moving-block safety, collision, labels, and rendering continue evolving from the same authoritative field between those boundaries. Clear transient hazards exactly once when their test/training lifecycle completes. Add seconds-apart persistence, retry persistence, explicit-boundary reset, material identity, render/collision synchronization, adversarial reset storms, repeated-seed, and exact 20/60/240 Hz tests.

### WALK-RELEASE-408 — Build and audit corrected local Runner v0.7.36
**Status:** COMPLETE — LOCAL PACKAGE, MANIFEST, AND INDEPENDENT EXTRACTION AUDITED

Integrate missions 403–407 coherently; advance source/checkpoint/state/package identity; update changelog, README, focused docs, install/audit/workflow contracts, and deterministic tests. Run repository hygiene, complete Windows SDL3/Vulkan and Linux GCC 14 warnings-as-errors matrices, all production diagnostics, installed and independently extracted launchers from unrelated working directories, ZIP checksum/per-file manifest audits, and direct packaged eyes for rest/swing, bounded firing, staged shuttle, and stable active terrain. Remove superseded local artifacts only after v0.7.36 is independently audited. Produce a clean local commit and package only; do not push, tag, publish, or mutate remote state.
### V0.7.36 completion evidence

- The user-supplied v0.7.35 `creature.rig` (SHA-256 `C7A6CBAB58E945BCC70ADA3ED59170B522B47CAEFBC2CBAEA149349862A46DAD`) is the canonical humanoid rest graph. Semantic support and manipulator chains now derive authored endpoints and local segment vectors; manipulators remain layered beside the torso at rest, arms counter-swing while paired feet/legs swing in opposition, facing owns bend direction, and policy/balance clamps cannot replace authored rest with the old outward brace. The v0.7.36 suite covers the exact supplied coordinates, rest, opposed phases, adversarial policy actions, all canonical topologies, edited geometry, and fixed-step equivalence.
- Sidearm, carbine, and launcher use authoritative simulation-side engagement profiles with minimum/maximum range, aim tolerance, cadence, hazard readiness, active-target, and remaining-hit-goal checks. Sidearm is `2.5–10 m`, carbine `5–18 m`, and launcher `7–24 m`; a held or adversarial fire action cannot bypass completion, too-near, too-far, aim, unsafe-terrain, or cooldown gates.
- Foundational Walk is forward-only. `SHUTTLE / TURN / RETURN` is a dedicated curriculum stage appended to the persisted enum so legacy numeric stage identifiers do not change, while logical curriculum ordering places it after Walk. It owns backup, whole-rig turn, opposite traversal, symmetric return, tests, telemetry, and cleanup. Exact 20/60/240 Hz state-machine coverage passes.
- Course-layout ownership is separate from episode dynamics. Ordinary controller retries preserve the same seeded sand/dirt/water/hole field and its settled deformation while clearing transient falling/moving hazards; explicit course, lesson, evaluation, or rig boundaries replace it once. Collision, render, material labels, observation state, safety dwell, and fixed-step deformation remain synchronized and frame independent.
- Training semantics are `0x0007'3601`, checkpoint magic is `EPPO36`, autonomy state is `RUNAUTONOMY 18`, and automatic files are isolated under `runner-v0736-authored-*`. Valid older states keep compatible lifetime totals while legacy stage identifiers remain bounded to their original range.
- Windows Visual Studio 2022 Release rebuilt the complete SDL3/Vulkan application and passed 33/33 CTests in `821.28 s`, including the `821.28 s` cold learner and the v0.7.36 authored-runtime suite. Linux GCC 14.3 compiled every production/test translation unit with `-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror` and passed 28/28 CTests in `636.84 s`, including the `635.47 s` cold learner.
- The final build-tree diagnostic matrix passes version, SDL3/Vulkan, package layout, 24/24 live acceptance, camera, five-layout DPI UI, seven-rig/fallen art budget, course/material/equipment/frame independence, five-rig training, and walk-eye replay. Retained zero-authority probes are biped `52.2417 m / 57.00`, humanoid `49.9672 m / 57.00`, quadruped `2.1616 m / 44.33`, crawler `7.4164 m / 35.67`, and hexapod `26.2362 m / 58.17`, each with zero invalid retained seeds and conveyor motion disabled. The walk-eye proof retained update `1120`, averaged `47.564 m / 57.00` steps over six valid seeds, and displayed `48.933 m / 57 steps / 56 crossings` with `0.250 s` maximum scissor time.
- Installed and independently extracted launchers run from `C:\Windows\Temp`, match the fully tested executable byte-for-byte, pass their package/Vulkan/camera/course/walk diagnostics, and match the 52-entry per-file SHA-256 manifest. The outer ZIP checksum is recorded beside the archive after this embedded ledger is finalized, avoiding a self-referential archive hash.
- Cleanup removed the tracked v0.7.24, v0.7.25, and v0.7.34 release archives, v0.7.25 download staging, and obsolete v0.7.32 validation frames while preserving authored source/history and the audited v0.7.35/v0.7.36 assets. The initial cleanup pass reclaimed `5.136 GiB`; the final verified workspace-only build and dependency-cache removal reclaimed another `2,749,022,264` bytes (`2.56 GiB`).
- Authority remained local. No push, tag, PR, release upload, published-asset operation, or remote mutation was performed.

# Runner v0.7.37 forward posture, multi-support locomotion, and coherent granular terrain

**Release state:** COMPLETE — LOCAL V0.7.37 RUNTIME AND EXACT-COMMIT SOURCE AUDITED; REMOTE UNTOUCHED

The 2026-08-16 packaged v0.7.36 screenshots are authoritative. The retained humanoid still stabilizes by leaning materially backward during forward walking even after authored-rig adjustment. The canonical four-leg rig records many support events but travels only a few feet before collapsing into a short-range static support pose. The active material field presents hard alternating rectangular slabs, vertical material seams, isolated triangular spikes/cuts, duplicated labels, and seconds-apart surface discontinuities instead of one coherent granular sand/dirt/water/hole simulation. The v0.7.36 automated retained probes exposed the same release-gate weakness: quadruped acceptance passed at only `2.1616 m` and crawler acceptance at `7.4164 m` while paired rigs exceeded `49 m`.

### WALK-FORWARD-POSTURE-409 — Train and qualify directionally natural upright travel
**Status:** COMPLETE — DIRECTIONAL FORWARD POSTURE TRAINING AND RELEASE TRUTH PASS

Measure the signed root-to-torso and torso-to-head posture against requested ground-relative travel, authored rest geometry, support state, and fixed-step time. Allow bounded natural oscillation, deliberate braking/backing, turns, step recovery, slope response, and non-upright body plans, but train away and reject sustained backward-supported forward travel before retention. Reward useful forward center-of-mass placement over the current/next support instead of a rearward brace that merely survives. Apply the same observation, teacher, reward, qualification, retained replay, PIP, diagnostic, and checkpoint contract to canonical, edited, and evolved upright rigs. Add deterministic positive natural-lean, negative backward-brace, adversarial high-distance brace, reversal/backing, slope/recovery, repeated-seed, and exact 20/60/240 Hz coverage without relaxing existing gait truth.

### WALK-MULTISUPPORT-PROGRESS-410 — Make four-leg and other multi-support rigs produce sustained travel
**Status:** COMPLETE — SUSTAINED ZERO-AUTHORITY MULTI-SUPPORT LOCOMOTION PASS

Derive support groups, phase offsets, stance propulsion, swing clearance, and trunk balance from authored topology rather than paired-leg assumptions or motor index. Multi-support training must distinguish genuine ground-relative displacement from stationary foot cycling, skating, crouched support locking, or conveyor/course motion. Retention and mastery require sustained monotonic progress, bounded stall time, useful distance, and repeated-seed generalization appropriate to quadruped, crawler, hexapod, edited, and evolved configurations; a two-metre quadruped may not pass a release diagnostic. Preserve topology diversity and zero controller-assist authority. Add positive forward multi-support gait, negative static cycling/support lock, adversarial contact-count inflation, material transitions, all canonical multi-support rigs, edited topology, repeated seeds, and exact 20/60/240 Hz coverage.

The complete Windows and Linux 1,200-update cold-start matrices reproduced a policy-consolidation deadlock after the v0.7.37 topology fix. Zero-authority reference gaits travel more than 23 m for both quadruped and hexapod, while their raw policies never retain a candidate because forward-stage retention requires all six evaluation seeds to be strict-valid at once. With no retained stage-safe candidate, the existing self-imitation path has no source and PPO loses partial progress between evaluations. Permit only post-teacher-handoff, raw-policy, nonzero stage-safe candidates to seed immutable retention and self-imitation; they remain visibly non-mastered and may never satisfy strict release, lesson advancement, or acceptance. Pre-handoff partial candidates remain ineligible, and a strict candidate must always outrank every partial candidate.

The first corrected-retention rerun retained a real `4.30427 m / 57.67` quadruped partial at update 1190 but did not reach strict validity, while hexapod still produced no incremental snapshot. Audit then found a second shared root cause: v0.7.37's physical teachers ran the tall four-support and six-support gaits at `1.30 Hz` and `0.78 Hz`, but observations still encoded their older `1.08 Hz` and `0.96 Hz` authored clocks. The policy therefore received phase inputs that contradicted its demonstrations. One topology-derived authored cadence must drive teacher actions, observation sine/cosine, tests, edited/evolved rigs, and checkpoint semantics exactly.

The cadence-aligned complete Windows Visual Studio 2022 Release cold learner passes all five fresh 1,200-update subjects with zero action authority and zero invalid retained seeds. Strict retained probes are biped `57.2632 m / 57.00`, humanoid `47.6876 m / 56.67`, quadruped `27.2436 m / 50.33`, crawler `31.2895 m / 49.17`, and hexapod `29.6316 m / 51.00`; quadruped retained at update 1170 and hexapod at update 970, both after their update-900 teacher handoff. The complete Windows suite passes 35/35 CTests in `980.76 s`, including the `907.02 s` cold learner. Linux GCC 14.3 rebuilds all production/test translation units with warnings-as-errors and passes 29/29 CTests in `833.88 s`, including the `761.74 s` cold learner. Build-tree, installed, and independently extracted production diagnostics reproduce the same strict zero-authority locomotion proof.

### WALK-GRANULAR-COHERENCE-411 — Render and collide with one continuous active material field
**Status:** COMPLETE — CONTINUOUS GRANULAR FIELD, COLLISION, RENDER, AND LABEL PASS

Replace section-local slab presentation with one world-coordinate material field whose neighboring samples remain continuous across ordinary material boundaries. Sand and loose dirt may fall, settle, stack, erode, excavate, and form holes; water may occupy settled depressions; explicit holes, cliffs, or moving obstacles may create deliberate discontinuities. Ordinary sand/firm/water transitions may not create invisible vertical collision walls, one-sample spikes, rectangular underlayers that appear as terrain motion, or renderer/collider disagreement. Simulation, observations, contacts, labels, preview, PIP, retries, evaluation boundaries, and camera transforms consume the same authoritative samples. De-duplicate and cull labels so they identify coherent regions without overlap. Add positive settling/deposition/excavation, negative seam/spike/disconnected-field, adversarial reset storm and boundary crossing, renderer/collision parity, repeated-seed, and exact 20/60/240 Hz tests.

### WALK-HYBRID-RUNTIME-BRAIN-413 — Pair learned weights with deterministic live planning and recovery
**Status:** COMPLETE — DETERMINISTIC LIVE HAZARD, STUCK, AND ESCAPE BRAIN PASS

The 2026-08-16 follow-up screenshot is authoritative: a multi-support rig can enter a terrain depression, keep cycling supports, and remain trapped without choosing a viable escape. A frozen policy file is not a complete enemy-AI controller. Ship it with a deterministic, fixed-step runtime brain that continuously derives intent from current terrain clearance, support geometry, locomotion progress, balance reserve, free-space direction, incoming-object velocity/time-to-impact/density, and recovery history. The code layer owns immediate safety, threat prediction, stuck/hole detection, escape-direction selection, recovery escalation, action limits, and fallback behavior; the learned policy owns adaptive pose and motor control inside that safe intent. It must react to hazards that were never present in the exact training sequence without reading renderer state or applying nonphysical translation. Add positive live dodge and hole egress, negative harmless-object/no-false-recovery, adversarial blocked exits and repeated falling objects, multi-topology, policy-saturation, repeated-seed, and exact 20/60/240 Hz coverage.

Release review found that idle and turn holds could preempt an otherwise urgent incoming-object response. Imminent threats therefore outrank idle, turn, step-up, and ordinary recovery planning; deterministic coverage must prove the same dodge decision while stationary and while turning, not only during an already requested walking direction.

### WALK-RELEASE-412 — Build and audit corrected local Runner v0.7.37
**Status:** COMPLETE — LOCAL RUNTIME AND EXACT-COMMIT SOURCE ARCHIVES AUDITED

Integrate missions 409–411 and 413 coherently across physics, rigs, contacts, terrain, observations, curriculum, policy dimensions, persistence, editor, renderer, diagnostics, tests, packaging, and documentation. Advance source/checkpoint/state/package identity and isolate incompatible learned state. Run repository hygiene and `git diff --check`; Linux GCC 14 warnings-as-errors and all CTests; the complete Windows SDL3/Vulkan build and all tests; every package, acceptance, feature, camera, course, multi-rig, posture, terrain, hybrid-brain, and walk-eye diagnostic; installed and independently extracted `run.bat` from unrelated working directories; ZIP checksum and per-file manifest audit. Alongside the Windows runtime ZIP, produce an exact-commit source ZIP with its own checksum and tracked-file manifest, excluding release binaries, build caches, autosaves, and temporary dependency worktrees; independently extract and byte-audit it before delivery. Produce clean local commits/packages only unless the user later authorizes remote mutation. Re-read this cache and preserve every newly discovered consequence before closure.
### V0.7.37 source and build-tree validation evidence

- The source tree passes repository hygiene and `git diff --check`. Windows Visual Studio 2022 Release rebuilt the complete SDL3/Vulkan product and passes 35/35 CTests in `980.76 s`; Linux GCC 14.3 rebuilds with `-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror` and passes 29/29 CTests in `833.88 s`.
- The production five-rig diagnostic passes after 1,200 fresh updates per subject with strict zero-authority retained distances of biped `57.2632 m`, humanoid `47.6876 m`, quadruped `27.2436 m`, crawler `31.2895 m`, and hexapod `29.6316 m`. Conveyor/course translation remains disabled and every retained seed is motion-valid.
- The production walk-eye diagnostic passes with a `48.669 m / 56-step` six-seed mean, a displayed `49.910 m / 57-step / 57-crossing` replay, `0/6` invalid retained seeds, and `0.083 s` maximum sustained scissor time. Version, Vulkan, package, 24/24 acceptance, camera, five-layout UI, art-budget, course, and hybrid-brain diagnostics also pass from the build tree.
- Installed and independently extracted `run.bat` launchers execute from `C:\Windows\Temp`, match the fully tested executable byte-for-byte, and each pass version, Vulkan, package, 24/24 acceptance, camera, UI, art, fresh five-rig training, walk-eye, course, and hybrid-brain diagnostics. Both reproduce the same strict retained locomotion evidence and zero controller authority.
- Independent runtime extraction contains 53 files and matches every per-file SHA-256 manifest entry. Independent exact-commit source extraction contains all 128 tracked non-release files and no release binary, build cache, autosave, or dependency worktree; every source file matches its manifest. The final archives are regenerated from the ledger-closure commit, and their non-self-referential checksums/manifests are authoritative beside the ZIPs.
- Training semantics are `0x0007'3701`, checkpoint magic is `EPPO37`, autonomy state is `RUNAUTONOMY 19`, and automatic files are isolated under `runner-v0737-hybrid-*`. The runtime brain applies bounded physical motor safety authority only for imminent threats, unsafe active hazards, or detected entrapment; it never translates the rig or reads renderer state.
- Repository hygiene, final cache reread, workspace-only temporary build/dependency cleanup, and local branch status are verified after archive regeneration. Authority remained local: no push, tag, PR, upload, published-asset operation, or other remote mutation was performed.

# Runner v0.7.38 topology-complete shuttle locomotion and truthful scoped totals

**Release state:** COMPLETE — LOCAL V0.7.38 RUNTIME AND EXACT-COMMIT SOURCE AUDITED; REMOTE UNTOUCHED

The 2026-08-20 packaged v0.7.37 screenshots are authoritative. The humanoid walks cleanly through the foundational Walk lesson, then loses that posture during lesson 4 `BACK / TURN / RETURN` and braces or collapses near a turn marker. Monoped and chicken are visibly flung, flipped, disconnected, or motionless while their counters report no useful support gait. The default Totals page combines wall duration, parallel agent-simulated duration, selected-rig lifetime, session deltas, all-rig lifetime, distance, steps, falls, runs, and resets into relationships that can be impossible or misleading: a session distance may exceed all-time distance, selected-rig duration may exceed the current wall session by days, steps may remain zero while distance/falls advance, and reset totals nearly equal simulated-run totals without saying what either means.

### WALK-SHUTTLE-POSTURE-414 — Preserve learned physical gait through the turn lesson
- v0.7.37 eye-test consequence (2026-08-20): the human controller is visibly sound through forward walking and then regresses in lesson 4. The current observation exposes requested travel direction but not authored facing or shuttle phase, so backing and post-turn forward traversal can be observationally ambiguous. Shuttle candidates can also enter the retained lineage through the partial-retention path. v0.7.38 must add explicit facing/phase observations and require strict production-controller evidence before a shuttle candidate may replace the retained forward-walk controller.
**Status:** COMPLETE — PHYSICAL RETAINED TURN REPLAY AND STRICT SHUTTLE RETENTION PASS

The first optimized Linux GCC 14 all-seven release gate correctly stopped v0.7.38 at the learned humanoid shuttle replay: the retained controller completed a real turn with zero rejection, zero invalid seeds, and zero curriculum-teacher authority, but averaged only `17.0472 m / 96` support events against the unchanged `18 m / 14-event` gate. Windows had retained `19.6526 m / 79.67` under the same contract. Audit found the cross-platform boundary was deterministic candidate ordering, not an excuse to lower acceptance: shuttle quality inherited Walk's cadence-first key, so a busy 96-event candidate could outrank a controller that physically traversed farther. Shuttle retention must rank completed turns first, ground-relative distance second, real support cadence third, and elapsed evidence fourth; add an adversarial selector test proving extra stationary/busy events cannot defeat longer valid traversal, then rerun both complete platform gates.

Treat the shuttle boundary as a topology-aware physical skill rather than a sign flip applied to a forward policy. Preserve the selected rig's authored rest shape, support ordering, controller lineage, whole-rig facing, and direction-relative posture while backing, decelerating, turning, and returning. Do not retain or pass a shuttle controller that reaches a marker by sustained backward bracing, airborne flipping, collapsed dragging, or support-count inflation. Teacher actions, policy observations, directional reward, runtime-brain intent, preview/PIP facing, and mastery qualification must use the same turn phase and signed travel frame. Add positive clean humanoid forward-to-turn-to-return, negative backward brace/collapse, adversarial saturated actions at both markers, edited/evolved and non-upright rigs, repeated seeds, and exact 20/60/240 Hz coverage. The packaged diagnostic must replay the physical retained turn sequence, not merely validate state-machine transitions.

### WALK-SINGLE-AND-AVIAN-SUPPORT-415 — Train and render monoped and chicken from authored topology
- v0.7.37 eye-test consequence (2026-08-20): monoped and chicken preview failures show that raw-policy validation is not sufficient evidence for the shipped controller. Candidate evaluation and retention must exercise the same zero-teacher production controller used by preview, and fragile single-support/avian topologies must not use partial retention. The policy remains responsible for locomotion; the code brain may provide deterministic topology-aware safety/reaction constraints but cannot silently substitute a teacher trajectory.
**Status:** COMPLETE — ALL-SEVEN ZERO-TEACHER PRODUCTION CONTROLLER PASS

Extend semantic support discovery, balance, teacher cadence, phase observations, reward, retention, mastery, runtime recovery, contact truth, and assembled art to single-support and avian/two-support body plans. A monoped must use a real hop/plant/recover cycle whose progress cannot require alternating left/right feet. A chicken must preserve its authored trunk, head, tail/manipulator, leg ownership, joint limits, and support order instead of receiving humanoid or multi-support assumptions. Supplied modular art may be assembled only on compatible semantic roles; incompatible or missing pieces must use bounded topology-coherent presentation without duplicated/disconnected humanoid anatomy. Add strict zero-authority cold-start locomotion gates for monoped and chicken, malformed/edited/evolved variants, negative stationary cycling/flipping, adversarial contacts/actions, repeated seeds, both facings/turns, and exact 20/60/240 Hz coverage.

Release-gate review found that --diagnose-rig-training and the cold learner still enumerated only the historical five subjects; monoped and chicken were covered only by teacher/reference probes. Expand the production report and learner pass to all seven canonical rigs so neither omitted topology can pass the package audit without a strict zero-authority retained controller.

The first all-seven 1,200-update Windows learner audit correctly failed the release after the five historical rigs passed. The monoped retained `21.7379 m / 24.8333` real gait cycles but failed two of six seeds with `FOOT-NODE SKATING / ROLLING` and rejection mask `4105` (`INVALID MOTION + MISSING SKILL + BACKWARD BRACE`). Source inspection found the contact validator still feeds the authored heel and toe of the monoped into a double-support pivot test even though both plates are one logical support cluster. Make rolling/skating truth operate on independent semantic support clusters and accept a recent single-foot rocker/hop transfer only when real unload, clearance, plant, and root displacement evidence exists; stationary heel/toe rocking, sliding, and backward bracing remain rejected.

The same audit found the chicken's zero-authority retained controller at the teacher-handoff boundary (`update 900`) reached only `5.28676 m / 7.33333` cycles and flew in one of six seeds; its current post-handoff policy flew in all six. The deterministic avian teacher remains valid at roughly `12.9 m / 25` cycles, so the defect is policy transfer/retention rather than authored geometry. Train and retain the avian controller from topology-neutral gait-cycle evidence, penalize unsupported launch/flight before it can dominate policy score, and keep guided imitation sourced only from physically clean planted trajectories through a complete zero-authority consolidation window. Do not weaken the `10 m / 14-cycle`, zero-rejection, zero-invalid-seed release threshold.

The second all-seven Windows audit, after semantic support-cluster and unsupported-flight fixes, again passed the five historical rigs and correctly blocked both fragile subjects. Monoped retained roughly 9.63 m but produced only 2.67 real gait cycles and failed a fresh seed with ZERO MOVEMENT; chicken retained no controller and its zero-authority replay failed all six seeds at roughly 0.38 m / 0 cycles, while its clean teacher still reached 12.9239 m / 25 cycles. This isolates a shared post-handoff training defect: zero-authority imitation is currently sampled from learner rollouts after those rollouts have already collapsed off the valid support manifold. Build a deterministic clean teacher-trajectory prior for fragile authored topologies, use it throughout zero-authority consolidation without adding runtime teacher authority, and reject or avoid anchoring partial candidates that do not reproduce real topology-specific support cycles across fresh seeds.

The third all-seven Windows audit proves the deterministic clean-prior transfer fixes the avian subject without regressing the five historical rigs: chicken retained a zero-authority `13.0918 m / 20.33-cycle` controller with zero invalid probes; biped, humanoid, quadruped, crawler, and hexapod also retained strict-valid controllers. Monoped alone still blocks release: all six fresh probes saturated into `OVER 50 KM/H`, retained no controller, and restarted the preview 264 times. Preserve the clean-prior path and unchanged `5 m / 8-cycle` monoped threshold, but bound single-support policy authority and learned action slew/energy at the motor topology boundary so the policy cannot convert the compact heel/toe rocker into an explosive launch. Add an adversarial saturated-action and repeated-seed negative case that must remain finite, planted/recoverable, and below overspeed before repeating the learner gate.

The fourth 1,200-update all-seven audit (2026-08-20) measured the exact zero-curriculum-authority production controller and correctly rejected both fragile rigs after the historical five retained valid controllers. Monoped failed all six probes at `-2.36 m / 1.17 cycles` by flipping after overspeed; chicken failed all six at roughly `-8.03 m / 5.83 cycles` out of bounds. Their deterministic topology teachers remain clean (`9.55 m / 24` monoped cycles and `12.92 m / 25` chicken cycles), proving that unbounded learned residuals leave the known-safe support manifold after handoff. Implement the requested weights-plus-code-brain architecture as an explicit production reflex layer: authored topology supplies bounded support timing and recovery authority, learned weights supply the residual locomotion/control action, evaluation and preview execute that same composite, and telemetry/diagnostics continue to report zero curriculum-teacher authority. Do not release until both rigs retain and replay this composite across all fresh seeds at the unchanged strict thresholds.

The fifth 1,200-update all-seven Windows audit (2026-08-20) passed the unchanged strict retained-controller gates with the production reflex-plus-learned-residual action path used by evaluation and preview. Retained fresh-seed probes were biped `47.4764 m / 57` cycles, humanoid `47.2177 m / 57`, monoped `10.9858 m / 27.5`, chicken `12.7207 m / 25`, quadruped `25.108 m / 52.67`, crawler `25.9962 m / 46`, and hexapod `29.9726 m / 51.33`; every retained probe had zero rejection, zero invalid seeds, and `VALID` motion. Curriculum-teacher authority remained zero at evaluation. The focused monoped gate independently reproduced `10.9858 m / 27.5` valid evidence. Preserve these thresholds and the topology scope while completing the platform and package matrices.

### WALK-SCOPED-STATS-416 — Make selected-rig, session, and all-rig totals mathematically truthful
- v0.7.37 eye-test consequence (2026-08-20): passed/failed counters are already stage-qualified, so the defect is not their arithmetic name. The visible ambiguity is scope and clock semantics: selected-rig selection, app-session all-rig totals, agent-simulated time, and wall time are presented closely enough to look contradictory. v0.7.38 must keep stage-qualified counts, assert completed = passed + failed, label every scope/clock explicitly, and explain that one completed rollout produces one worker restart.
**Status:** COMPLETE — THREE-SCOPE CLOCKED TOTALS AND CONSISTENCY PASS

Define one accumulator contract for wall runtime, agent-simulated training time, optimizer updates, attempted episodes, completed simulated runs, environment restarts, passed/failed stage checks, agent-equivalent displacement, real support events, falls, collisions, features, and lesson progress. Persist lifetime baselines separately from current-session deltas and selected-rig scope; all-time values must include the current session exactly once and cannot be below the matching session quantity. Label wall time and parallel agent-simulated time distinctly. Do not present every worker episode boundary as an unexplained reset, do not count invalid teleport/restart displacement as distance, and do not show ordinary simulated-run distance as real walking steps. Legacy v0.7.37 state must import monotonically without inventing precision or resetting valid lifetime history. Add positive accumulation, restart/import/rig-switch, negative double-count/underflow/teleport, high-worker and zero-step adversarial cases, repeated-session persistence, unit conversion, and exact 20/60/240 Hz tests; exercise the actual default Totals rendering strings and ordering.

### WALK-RELEASE-417 — Build and audit corrected local Runner v0.7.38 runtime and source
**Status:** COMPLETE — LOCAL RUNTIME AND EXACT-COMMIT SOURCE ARCHIVES AUDITED

Integrate missions 414–416 coherently across physics, rigs, contacts, terrain, observations, curriculum, policy dimensions, persistence, editor, renderer, diagnostics, tests, packaging, branches, and release assets. Advance source/checkpoint/state/package identity and isolate incompatible learned state. Run repository hygiene and `git diff --check`; Linux GCC 14 warnings-as-errors and all CTests; the complete Windows SDL3/Vulkan build and all tests; package, acceptance, camera, course, terrain, hybrid-brain, physical-shuttle, seven-rig training, stats-truth, art, and eye diagnostics; installed and independently extracted `run.bat` from unrelated working directories; runtime ZIP checksum/per-file manifest audit; exact-commit source ZIP checksum/tracked-file manifest/independent extraction/byte audit. Produce clean local commits and packages containing source only; do not push, tag, publish, or mutate remote state unless the user later instructs otherwise.
### V0.7.38 build-tree validation evidence

- Windows Visual Studio 2022 Release rebuilt the complete SDL3/Vulkan product and passed 35/35 CTests in `1483.39 s`, including the `1405.13 s` topology-complete cold learner. Linux GCC 14.3 rebuilt every production and test translation unit with warnings-as-errors and passed 29/29 CTests in `1218.94 s`, including the `1143.86 s` cold learner. Repository hygiene and `git diff --check` pass.
- The direct optimized Windows executable passes version, Vulkan, package, 24/24 acceptance, camera, five-layout UI, modular-art, seven-rig training, walk-eye, course/material/equipment/frame-independence, and hybrid-brain diagnostics. The hybrid brain proves deterministic hole, falling-object, harmless-object, blocked-exit, topology, bounded-action, and fixed-step behavior.
- Strict zero-curriculum-authority retained probes are biped `47.4764 m / 57.00` support cycles, humanoid `47.2177 m / 57.00`, monoped `10.9858 m / 27.50`, chicken `12.7207 m / 25.00`, quadruped `25.1080 m / 52.67`, crawler `25.9962 m / 46.00`, and hexapod `29.9726 m / 51.33`. The walk-eye proof passes at 1,200 updates with retained update 1,050, authority `0.000`, a `48.649 m / 57.00` six-seed mean, `0/6` invalid seeds, and a displayed `52.050 m / 57-step / 57-crossing` replay.
- Installed and independently extracted `run.bat` launchers ran from `C:\Windows\Temp` and each passed the complete version, Vulkan, package, 24/24 acceptance, camera, UI, art, seven-rig training, walk-eye, course/frame-independence, and hybrid-brain matrix. Their retained metrics match the build-tree results exactly, and their `Runner.exe` bytes match the fully tested build at SHA-256 `96fde4ca4e69653303930dd616a86f38277edcbab9e9ba2b5d015b37273ca64e`.
- Independent runtime extraction contains 54 files and matches every per-file SHA-256 manifest entry. Independent exact-commit source extraction contains all 129 tracked non-release files; every extracted source file matches both its SHA-256 manifest entry and the exact closure commit's Git blob ID. Runtime and source archive checksums are stored beside their archives to avoid a self-referential ledger hash.
- Training semantics are `0x0007'3801`, checkpoint magic is `EPPO38`, autonomy state is `RUNAUTONOMY 20`, and automatic files are isolated under `runner-v0738-topology-*`. Final staging and temporary audit directories are removed after archive regeneration. Authority remained local: no push, tag, PR, release upload, published-asset operation, or other remote mutation was performed.

# Runner v0.7.40 physical-facing return closure

**Release state:** IN PROGRESS — PHYSICAL RETURN FIXED; FULL LOCAL RELEASE AUDIT REQUIRED

The 2026-08-20 packaged v0.7.38 screenshot is authoritative. The visible course still behaves like a continuously deforming height field even though the supplied SandHybrid integration contract defines authoritative static fine cells with immediate 8x8 macro-tile promotion/demotion. The only ordinary live mutation should come from explicitly spawned, dropped, deposited, or removed granular cells; standing and walking on authored ground must not erode, relax, ripple, compact, raise berms, or change collision underneath the rig. The same screenshot also shows the lesson-4 rig facing the requested return direction while leaning and resisting as if external course motion were dragging it backward. A correct turn must change the locomotion frame and produce physical support-driven travel in the direction the rig faces.

### WALK-IMMUTABLE-MACRO-TERRAIN-418 — Keep authored course cells static
**Status:** COMPLETE — IMMUTABLE AUTHORED CELLS AND EXPLICIT GRANULAR OVERLAY PASS

Use the pinned SandHybrid cell field as the sole terrain authority. Authored macro tiles and their canonical fine cells remain byte-identical across ordinary rig pressure, contact, stepping, retries, rendering, and fixed-step advancement. Disable pressure excavation, contact compaction, passive relaxation, procedural surface drift, and any renderer-only height animation for prebuilt terrain. Explicit falling/deposited granular cells may fall, collide, settle, stack, promote/demote macro occupancy, and be removed by an explicit authored event; those dynamic cells may not rewrite neighboring structural/course cells. Collision, terrain observations, support contacts, renderer batches, labels, PIP, preview, training workers, and diagnostics must sample the same static base plus dynamic-cell overlay. Add positive falling-cell settle/stack tests, negative foot-pressure mutation tests, adversarial repeated contacts and retry storms, macro promotion/single-cell demotion, renderer/collider parity, repeated seeds, and exact 20/60/240 Hz fingerprints.

### WALK-SELF-PROPELLED-RETURN-419 — Walk in the faced direction after turning
**Status:** IMPLEMENTED — FULL RELEASE AUDIT IN PROGRESS

The shuttle must brake, turn the whole rig, establish the new facing and direction-relative gait phase, then travel under physical motor/contact forces toward the opposite marker. No conveyor, course-progress translation, root translation, camera transform, terrain motion, or preview assist may provide apparent displacement. Backing before the turn is a short deliberate phase; after the flip, forward locomotion in the new facing must have positive signed progress, opposed support swing, useful cadence, bounded torso lean, and no sustained backward-brace posture. Observation direction/facing/phase, runtime planner intent, learned residual, topology reflex, teacher demonstration, reward, evidence, strict retention, main preview, and PIP must agree. Add positive both-direction retained replay, negative externally dragged/braced motion, adversarial sign/facing mismatch, marker overshoot, all canonical topologies, repeated seeds, and exact 20/60/240 Hz coverage.

The v0.7.39 post-handoff assertion proved only signed root displacement with zero course motion. Direct packaged-runtime eye testing shows the rig can still increase leftward distance while using a backpedaling support pattern on the return toward TURN A. Replace the distance-only proof with physical facing-relative evidence: motor-space reflection, forward foot placement and push-off, opposed support phase, root velocity aligned with the rendered/semantic facing, bounded COM/torso bracing, and an explicit negative case that rejects dragged or backward-braced leftward travel.

Implementation evidence (2026-08-20): the shared two-link reconstruction hard-coded one world-space knee bend side, so the post-turn constraint solve forced reflected legs back toward their outbound branch. v0.7.40 makes the knee solve facing-local, reflects the physical plant and equipment aim exactly once, localizes motor and observation channels, and renders the physical positions without a second presentation flip. The focused adversarial return travelled 8.73 m left with 9 real gait cycles, zero course translation, no sustained backward brace, and no invalid motion. The pure physical teacher completed 66.5 m / 5 turns / 86 gait cycles; the repeated-seed humanoid completed 70.2 m / 5 turns with an identical fingerprint. Positive mirror equivalence and negative backward-brace rejection pass with the original hybrid authority.

### WALK-RELEASE-420 — Build and audit corrected local Runner v0.7.40 runtime and source
**Status:** IN PROGRESS — V0.7.40 FULL LOCAL AUDIT REQUIRED

The first complete v0.7.39 platform pass found a cross-compiler monoped boundary before release: the focused MSVC reference accepted the 0.175-radian hip stroke, while GCC 14 initially produced two valid but sub-threshold 4.80-4.89 m seeds against the unchanged 5 m gate. The immutable read-only collision adapter then exposed an unsupported-launch boundary at the stronger provisional settings. The coherent final calibration keeps the 0.175-radian hip stroke, uses a 0.145-radian heel/toe rocker, reduces knee compression to 0.46, and retains 0.180 extension. Optimized MSVC and GCC 14 now both pass the all-seven deterministic reference suite at the unchanged 5 m, 8-cycle, and 20-second monoped gates. Preserve those gates while completing both full platform learner suites.

Validation evidence before packaging: repository audit and `git diff --check` pass; the complete Windows SDL3/Vulkan Release build passes all 35 CTest suites; Linux GCC 14 warnings-as-errors passes all 29 CTest suites; version, Vulkan, package, 24/24 acceptance, camera, UI, art, course/static-cell, hybrid-brain, walk-eye, and all-seven production rig-training diagnostics pass from the build tree. The strict retained seven-rig replay passes for biped, humanoid, monoped, chicken, quadruped, crawler4, and hexapod; current exploratory samples may still be rejected without invalidating the retained controller.

The installed and independently extracted launchers pass the complete diagnostic matrix from `C:\Windows\Temp`, including the seven-rig learner, walk-eye, static course/frame-independence, and hybrid-brain checks. The pre-closure runtime archive contains 55/55 manifest files; the exact-commit source archive contains 130/130 tracked Git blobs; and the build-tree, installed, and independently extracted `Runner.exe` bytes are identical at SHA-256 `77c4555150c00352d5e5c947b3f277b2d16a0cd59d8d8eb017ac2d08398b2693`. This ledger-closing commit changes mission evidence only. Final delivery remains conditional on regenerating both archives from this exact commit, independently extracting them, repeating the checksum/manifest/Git-blob/executable-identity audits, and removing only the scoped v0.7.39 audit directories.

The post-package return-direction eye test supersedes those automated passes: v0.7.39 is preserved as an audited historical artifact but is not the corrected final product. A successor local-only release must close WALK-SELF-PROPELLED-RETURN-419 with facing-relative gait evidence and repeat the complete runtime/source/package audit without pushing, tagging, publishing, or mutating remote state.

Inventory and reconcile physics, SandHybrid adapter ownership, macro/fine cells, contacts, terrain observations, curriculum events, policy observations and dimensions, persistence semantics, authored rigs, runtime brain, renderer/PIP, telemetry, diagnostics, tests, packaging, documentation, and release assets. Advance all incompatible source/checkpoint/state/package identities. Run repository hygiene and `git diff --check`; Linux GCC 14 warnings-as-errors and all CTest suites; the complete Windows SDL3/Vulkan build and all tests; package, acceptance, camera, UI, art, terrain/macro-cell, course, hybrid-brain, physical-shuttle, seven-rig training, stats-truth, and walk-eye diagnostics; installed and independently extracted `run.bat` from unrelated working directories; runtime ZIP checksum/per-file manifest audit; and exact-commit source ZIP checksum/tracked-file manifest/independent extraction/byte audit. Produce clean local commits and packages only. Do not push, tag, publish, or mutate remote state unless the user later instructs otherwise.