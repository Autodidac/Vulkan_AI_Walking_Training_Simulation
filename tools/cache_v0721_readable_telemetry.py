#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
path = root / "missioncache.md"
text = path.read_text(encoding="utf-8")

heading = "# Runner v0.7.21 plain-language training dashboard\n"
if heading in text:
    raise SystemExit("v0.7.21 telemetry missions are already cached")

section = r'''# Runner v0.7.21 plain-language training dashboard

**Release state:** CACHED BEFORE IMPLEMENTATION — RELEASE BLOCKING.

The application now exposes enough internal training data to debug the trainer, but the default interface still requires reinforcement-learning knowledge. A normal failed evaluation can show a huge red negative score, `-INF`, hexadecimal quality keys, abbreviated counters, and rejection terminology without explaining whether training is healthy, what improved, what failed, or what must happen next. The default dashboard must answer five ordinary questions: Is it still learning? Is it getting better? What just happened? What is it trying to learn now? What specifically must improve before the next lesson?

### WALK-HUMAN-STATUS-263 — Summarize learning health in ordinary language
**Status:** OPEN — RELEASE BLOCKING

Derive one stable headline from trainer state: `STARTING`, `TRAINING NORMALLY`, `TESTING CURRENT POLICY`, `VALID ATTEMPT FOUND`, `IMPROVING BEST RESULT`, `RETRYING AFTER FAILED TEST`, `TRYING A FRESH POLICY`, `PAUSED`, or `LESSON MASTERED`. The headline must not infer failure merely because an internal score is negative.

### WALK-LESSON-PROGRESS-264 — Show understandable lesson completion
**Status:** OPEN — RELEASE BLOCKING

Display total updates prominently, then show current-lesson progress as a percentage and a simple bar. Progress is the conservative minimum of required update, episode, and evaluation work, capped at 100%; mastery confirmation is shown separately. Labels use full words such as `UPDATES`, `ATTEMPTS`, and `TESTS`, not `UPD`, `EPS`, or `EVAL`.

### WALK-LATEST-TEST-265 — Explain the latest evaluation result
**Status:** OPEN — RELEASE BLOCKING

The default results page says `LATEST TEST: PASSED` or `LATEST TEST: NOT YET PASSED`, followed by one plain-English reason such as `body touched the ground`, `needs more alternating steps`, `did not travel far enough`, `could not hold balance long enough`, or `joint motion was too violent`. A rejected test is presented as useful feedback, not as proof that learning is permanently broken.

### WALK-BEST-RESULT-266 — Report useful accomplishments instead of arbitrary score
**Status:** OPEN — RELEASE BLOCKING

The default page reports stage-relevant best/current evidence: standing time and valid seeds; crouch/hold/recovery; walking distance, steps, and survival; jump landings; obstacles passed; controlled flip landings; or mixed-course survival. Raw evaluation score and packed quality key are hidden from the default page.

### WALK-NEXT-GOAL-267 — State exactly what advances the lesson
**Status:** OPEN — RELEASE BLOCKING

Show a concise stage-specific next goal. Examples: stay upright for six seconds across test seeds; crouch, hold, and stand back up; take real alternating steps and cover the required distance; land a powered jump; clear a hurdle; land a controlled flip; or survive the mixed course. When the required training budget is incomplete, say that more training/test samples are needed before mastery can be judged.

### WALK-COLOR-SEMANTICS-268 — Reserve red for actionable current failure
**Status:** OPEN — RELEASE BLOCKING

Negative score magnitude, uninitialized best score, ordinary rejection, and incomplete work do not paint the entire card red. Cyan indicates information/work in progress, yellow indicates an unmet current goal or retry, green indicates valid evidence/mastery, and red is limited to broken rig, invalid numerical state, package/runtime failure, or a presently terminal motion fault.

### WALK-ADVANCED-269 — Preserve expert diagnostics behind an explicit page
**Status:** OPEN — RELEASE BLOCKING

Add an `ADVANCED DIAGNOSTICS` page containing raw evaluation score, best score/update, quality key, rejection mask/name, policy/value loss, entropy, learning rate, environment steps, worker throughput, optimizer state, and pipeline details. The default page remains understandable without opening it.

### WALK-TOTALS-PLAIN-270 — Translate lifetime totals
**Status:** OPEN — RELEASE BLOCKING

The totals page groups data under `THIS RIG`, `THIS SESSION`, and `ALL TIME` with full labels. It explains that `ATTEMPTS` are completed simulated episodes, `VALID` means the motion passed safety/skill gates, `RESETS` are policy/episode restarts rather than lost all-time progress, and `ROLLBACKS` mean the trainer restored a better retained controller.

### WALK-INLINE-HELP-271 — Make unfamiliar terms self-explanatory
**Status:** OPEN — RELEASE BLOCKING

Provide a compact on-screen legend or explanatory lines for total updates, lesson progress, retained champion, latest test, attempts, valid attempts, resets, and rollbacks. The UI must not require README knowledge to interpret its primary state.

### WALK-TELEMETRY-TEST-272 — Deterministically test every human-facing state
**Status:** OPEN — RELEASE BLOCKING

Add pure C++23 tests covering no-evaluation startup, active training, valid evaluation, invalid evaluation, uninitialized infinities, fresh-policy retry, paused state, mastery, conservative progress calculation, stage-specific goal text, rejection translation, color severity, and advanced raw-value availability. Existing locomotion, UI, Windows, and package gates remain mandatory.

### WALK-COMPAT-273 — Preserve learned-state compatibility
**Status:** OPEN — RELEASE BLOCKING

This release changes presentation only. Do not change policy dimensions, checkpoint format, training semantics, terrain behavior, curriculum thresholds, retained champion parameters, or v0.7.20 autosave paths. Existing v0.7.20 learned state must resume directly.

### WALK-DOC-274 — Document the readable dashboard contract
**Status:** OPEN — RELEASE BLOCKING

Update README, CHANGELOG, one focused v0.7.21 document, repository audit, package contents, and this single mission cache. Document every default label and its precise meaning without creating duplicate ledgers.

### WALK-RELEASE-275 — Publish audited Runner v0.7.21
**Status:** OPEN — RELEASE BLOCKING

Require Linux GCC 14 warnings-as-errors, full Windows SDL3/Vulkan build, complete deterministic/live/UI suites, readable-dashboard diagnostics, installed/extracted execution, ZIP/checksum/manifest, published-asset re-download byte verification, and cleanup of temporary branches/workflows. The release includes every v0.7.20 locomotion, terrain, preview, DPI, clipping, and icon correction.

'''

marker = "# Carried open work\n"
if marker not in text:
    raise SystemExit("carried-work marker not found")
text = text.replace(marker, section + marker, 1)
text = text.replace(
    "# Runner v0.7.20 equipment, carry, and target curriculum\n\n"
    "**Release state:** CACHED AND OPEN — intentionally separated from the v0.7.19 general-locomotion release because equipment changes policy dimensions and checkpoint compatibility.",
    "# Runner v0.7.22 equipment, carry, and target curriculum\n\n"
    "**Release state:** CACHED AND OPEN — intentionally separated from the v0.7.21 presentation-only release because equipment changes policy dimensions and checkpoint compatibility.",
    1)
path.write_text(text, encoding="utf-8", newline="\n")
print("Runner v0.7.21 readable telemetry missions cached")
