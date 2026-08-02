from pathlib import Path

root = Path(__file__).resolve().parents[1]
path = root / 'missioncache.md'
text = path.read_text(encoding='utf-8')

section = r'''

## Full conversation reconciliation for v0.7.4

This section is the explicit carry-forward audit of the complete Runner conversation trail. Earlier broad mission names remain valid, but the requirements below are no longer allowed to hide behind shorthand. Every item must be implemented, deterministically tested where practical, exercised by the full Windows package, and preserved in release evidence. Contradictory packaged-runtime screenshots reopen the exact affected mission.

### WALK-AUDIT-051 — No silent omissions across project conversations
**Status:** IN PROGRESS

The release pass must reconcile every prior request about rigs, feet, joints, curriculum, hazards, learning, concurrency, PIP, UI, statistics, persistence, launch, package contents, branches, pull requests, and release assets. Anything not completed remains explicitly open rather than disappearing during cleanup.

### WALK-RIGLAB-052 — Safe and complete rig editing
**Status:** IN PROGRESS

The rig lab exposes node, bone, motor, semantic root/torso/head, and left/right support meaning. Joint limits and strength remain inspectable and testable per joint or coordinated group. Selecting or dragging the hip cannot lock the application. Required default structural joints cannot be deleted into an invalid rig; invalid edits are rejected without blocking the training worker.

### WALK-PRESETS-053 — Distinct stable preset anatomy
**Status:** IN PROGRESS

Humanoid, biped, chicken, quadruped, four-leg crawler, six-leg rig, and monoped presets remain structurally distinct, finite, connected, grounded, and visually recognisable. Every legged preset has explicit semantic feet or support contacts. Stable quadruped-derived physical limits may guide other defaults without turning different rigs into the same body.

### WALK-PIPELINE-054 — Continuous background training without mode stalls
**Status:** IN PROGRESS

Training continues on worker-owned state while the live simulation renders immutable publications. C++23 coroutine stages, persistent rollout workers, parallel gradient workers, asynchronous persistence, and NORMAL/FASTER/MAX controls remain functional. Switching between live and rig-lab views does not stop learning, reset the controller, or require reduced visualisation modes.

### WALK-COURSE-055 — Long readable locomotion course
**Status:** IN PROGRESS

The course includes clear ground and road reference lines, long flat preparation areas, later ramps, inclines, declines, hills, and uneven terrain. Metric and Imperial modes use quarter-kilometre or quarter-mile reference markers without changing the denser internal obstacle schedule.

### WALK-HAZARDS-056 — Learnable hazards that must be passed
**Status:** IN PROGRESS

Rocks, hurdles, low bars, moving hazards, thrown objects, and mixed terrain are world-anchored curriculum obstacles rather than pickups or actor-attached debris. Contact is permitted when physically appropriate, but the goal remains passing or recovering from the hazard. Observation includes type, distance, dimensions, motion, and enough approach time to perform a meaningful movement.

### WALK-CURRICULUM-057 — Ordered reusable movement skills
**Status:** IN PROGRESS

The evidence-gated order is standing, compression duck and recovery, powered jump and landing, real alternating walk/run, moving low-bar or hurdle avoidance, controlled landed flips, then mixed traversal combining movement with ducking, jumping, or flipping. Scalar reward alone cannot skip prerequisites.

### WALK-GATES-058 — Anti-exploit locomotion rules
**Status:** IN PROGRESS

More than three airborne rotations, flipping at or above 50 km/h, unpowered sustained flight, out-of-bounds motion, micro-movement, zero progress, wheel sliding, body rolling, foot-node rolling, head dragging, collapsed support, hazard quivering, and knee/body-first obstacle shoving cannot qualify or seed elite state. Early harmless settling retains bounded grace.

### WALK-AIRTIME-059 — Powered but bounded aerial ability
**Status:** IN PROGRESS

A rig may jump or briefly fly only when joint power produces a recognised launch in an allowed lesson. The allowance is stage-bounded, never substitutes for walking, and ends in a controlled landing. Generic rotation remains diagnostic and penalised outside the dedicated flip lesson.

### WALK-CONTROL-060 — Coordinated joints with feet-first authority
**Status:** IN PROGRESS

Hips, knees, shoulders, and elbows may use light stage-aware coordination while PPO keeps residual per-joint control. Feet and leg chains establish support before arms gain authority. Ducking uses hips and knees rather than a robot-like torso or main-shoulder-axis swing; arms remain available later for balance and acrobatics.

### WALK-PIPUI-061 — Full-body PIP and readable responsive UI
**Status:** IN PROGRESS

The training PIP shows only a current connected full rig and automatically fits all particles; it never zooms into detached feet or publishes stale collapsed posture. Text, panels, telemetry, controls, and the full-width DPI-safe background remain readable without overlap at supported window sizes.

### WALK-STATS-062 — Complete rig, session, and persisted totals
**Status:** IN PROGRESS

Per-rig lifetime statistics include age, updates, environment steps, episodes, valid and invalid episodes, distance, alternating steps, falls or invalidations, collisions, powered jumps, landed jumps, landed flips, obstacles passed, accepted/rejected rig changes, and best stage reached. Session totals and persisted all-time totals expose the same relevant counters plus resets and rollbacks. Counter baselines change only with the rig signature, and incompatible older state cannot silently corrupt totals.

### WALK-LEARNING-063 — Best-result imitation without regression
**Status:** IN PROGRESS

Only current stage-valid trajectories may become champions, rollback anchors, evolved-rig seeds, imitation samples, or PIP representatives. Robust perturbed evaluation, quality-before-reward ranking, regression rollback, bounded imitation weight, and fresh-policy semantics remain shared across training, evaluation, preview, and live execution.

### WALK-LAUNCH-064 — Clean executable-relative package launch
**Status:** IN PROGRESS

The source launcher and installed launcher select the current adjacent or Release executable, find shaders/assets relative to that executable, and work from an unrelated current directory. Stale root executables, stale learned state, missing DLLs, or source-tree assumptions are release blockers.

### WALK-RELEASE-065 — Tidy audited release repository
**Status:** IN PROGRESS

A release is not complete until Linux and Windows tests pass, the full Vulkan application builds, the installed and independently extracted launchers pass diagnostics, the archive manifest and SHA-256 verify after re-download, the release ledger records exact evidence, temporary applicators/workflows are removed, no cleanup pull requests remain open, and only `main` remains unless an explicitly retained development branch is documented.

### WALK-LIVE-066 — Screenshot-level packaged-runtime acceptance
**Status:** IN PROGRESS

Automated metrics cannot overrule visible failures. Fused or detached feet, body collapse, arm-first movement, uncontrolled heads or tails, clipped hazards, unavoidable obstacles, incorrect preset anatomy, stale PIP frames, unreadable UI, or a controller repeatedly exploiting one body axis reopen the matching mission and block release closure.
'''

if '### WALK-AUDIT-051' not in text:
    anchor = text.index('## v0.7.3 live-runtime correction')
    text = text[:anchor] + section + '\n' + text[anchor:]

# Strengthen the prior narrow statistics mission so it cannot be mistaken for the
# complete lifetime/session/all-time request.
old = '''### WALK-STATS-038 — Rig lifetime and cumulative runtime totals
**Status:** PACKAGE VERIFIED

Display current rig age, rig update delta, rig environment-step delta, session runtime, total updates, and total environment steps. Counter deltas are saturating and reset only when the rig signature changes.'''
new = '''### WALK-STATS-038 — Rig lifetime and cumulative runtime totals
**Status:** REOPENED BY FULL CONVERSATION AUDIT

The earlier v0.7.3 display covered only age, updates, environment steps, and session runtime. WALK-STATS-062 now carries the complete requested per-rig, session, and persisted all-time counters, including episodes, distance, steps, falls/invalidations, collisions, jumps, flips, obstacles, rig acceptance/rejection, resets, and rollbacks.'''
text = text.replace(old, new)

path.write_text(text, encoding='utf-8', newline='\n')
Path(__file__).unlink()
print('reconciled complete Runner conversation trail into missioncache.md')
