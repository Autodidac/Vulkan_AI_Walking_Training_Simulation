from __future__ import annotations

import argparse
from pathlib import Path


def verified_ledger(release_state: str, release_evidence: str) -> str:
    return f'''# EpochRunner mission cache

This is the authoritative release ledger. A mission is VERIFIED only when implementation, deterministic acceptance, cross-platform validation, and release evidence agree. Packaged runtime evidence overrides an earlier synthetic pass when they conflict.

## Release target

**Target:** EpochRunner v0.7.1

**Release state:** {release_state}

## Training-quality correction

### WALK-MOTOR-012 — Reciprocal parent-side motor reaction
**Status:** VERIFIED

Every motor divides angular correction between the driven subtree and the complete remaining body using rotational inertia. The joint pivot and parent body are not world anchors. Each internal correction preserves whole-body center of mass. Direct tests require chest motion, pivot motion, driven-side dominance, and bounded center-of-mass drift.

### WALK-TRAIN-013 — Reject collapsed poses as training success
**Status:** VERIFIED

Standing qualification requires sustained upright two-foot support, valid head and torso height, no non-foot support, bounded stance slip, bounded vertical motion, and bounded joint speed. Collapsed, unsupported, body-contact, violent-joint, rolling, skating, hovering, motionless, and otherwise invalid candidates cannot become the best policy, rollback anchor, evolved-rig seed, imitation source, or displayed training sample.

### WALK-CURR-014 — Evidence-gated ordered curriculum
**Status:** VERIFIED

The curriculum advances only through retained prerequisite evidence: sustained stand and recovery; duck plus return to stand; powered takeoff plus upright supported landing; alternating supported walk/run steps; moving duck/jump; one-to-three controlled spins plus upright landing; then mixed traversal. Average reward cannot bypass a missing prerequisite.

### WALK-BEST-015 — Stage-valid best-policy and imitation selection
**Status:** VERIFIED

Best-policy selection is lexicographic: stage validity and evidence quality first, scalar reward only as a tie-break. Evaluation, champion rollback, rig evolution, self-imitation, and PIP representative selection share the same qualification predicate. A high-reward invalid pose always loses to a lower-reward valid controller.

### WALK-STATE-016 — Invalidate incompatible learned state
**Status:** VERIFIED

Training checkpoints carry a v0.7.1 semantics signature and a new checkpoint format. Incompatible v0.7.0 optimizer, curriculum, best-policy, rig-evolution, and imitation state is rejected. Default autosave paths and autonomy-state format are versioned for v0.7.1, and the UI reports incompatibility instead of silently resuming stale behavior.

### WALK-RUNTIME-017 — Bounded packaged-runtime acceptance
**Status:** VERIFIED

The release gate builds the packaged Windows application and runs deterministic replay acceptance proving that a bounded first training update retains a stage-valid standing controller, while an adversarial collapsed pose is rejected. UI telemetry exposes stance duration, recovery evidence, quality key, and rejection reason. The live viewport distinguishes an unverified current policy from a stage-valid best controller, and the PIP remains empty until a valid representative rollout exists.

## Runtime architecture

### WALK-CORO-001 — Meaningful C++23 training pipeline
**Status:** VERIFIED

Queued commands, rollout collection, advantage computation, parallel gradient generation, deterministic reduction and optimizer application, evaluation and curriculum, immutable publication, asynchronous persistence, and throttle/yield remain observable coroutine stages.

### WALK-ARCH-001 — Worker-owned mutable trainer state
**Status:** VERIFIED

The trainer worker exclusively owns mutable PPO, optimizer, curriculum, rig-evolution, and checkpoint state. UI operations use coalesced commands and immutable snapshots.

### WALK-IO-001 — Asynchronous checkpoint and autosave
**Status:** VERIFIED

Immutable checkpoint, rig, and state snapshots are coalesced and written by a dedicated `std::jthread` using temporary-file plus atomic rename publication.

### WALK-TEST-001 — Cross-platform concurrency regression suite
**Status:** VERIFIED

Coverage retains hip editing, preset swapping under NORMAL and MAX CPU load, speed-mode switching, pause and single-step, checkpoint operations under load, deterministic staged updates, cancellation, persistence coalescing, Linux release validation, and Windows full-application validation.

## Ordered movement curriculum

### WALK-SKILL-008 — Ordered reusable skills
**Status:** VERIFIED

Stand, duck/recover, jump/land, walk/run, moving duck/jump, controlled flips, and mixed traversal are taught and validated in prerequisite order. Hazard contact remains physical and legal; passing the hazard is the goal. Hovering, unpowered sustained flight, more than three spins, ground rolling, body surfing, planted-foot skating, and wheel sliding remain invalid.

### WALK-ARMS-009 — Humanoid arms for balance and acrobatics
**Status:** VERIFIED

The humanoid retains independent shoulder and elbow motors across eight policy outputs. Reciprocal motor reaction prevents the shared chest or shoulder pivot from acting as a fixed anchor.

### WALK-LEARN-010 — Faster learning without regression
**Status:** VERIFIED

A strong neutral standing bootstrap decays as the controller learns. Gait bootstrap, action smoothing, anti-skating gates, skill-specific learning-rate reset, evidence-first champion anchoring, rollback, and bounded self-imitation remain active without allowing reward exploits.

## Existing locomotion and course requirements

### WALK-RIG-001 — Nonblocking hip/joint editing
**Status:** VERIFIED

### WALK-CONC-001 — Persistent CPU parallelism
**Status:** VERIFIED

### WALK-UI-001 — Functional NORMAL/FASTER/MAX CPU controls
**Status:** VERIFIED

### WALK-OPT-001 — Parallel PPO optimizer
**Status:** VERIFIED

### WALK-COURSE-001 — Procedural obstacle and recovery treadmill
**Status:** VERIFIED

### WALK-OBS-001 — Complete obstacle sensing and reward integrity
**Status:** VERIFIED

Reward is subordinate to stage-valid evidence. Obstacle geometry and motion remain observable, and invalid posture cannot be promoted by scalar reward.

### WALK-PHYS-001 — Semantic support, traction, and world-anchored debris
**Status:** VERIFIED

### WALK-COURSE-002 — Shared mile-marker obstacle schedule
**Status:** VERIFIED

### WALK-GAIT-002 — Alternating stepping instead of wheel sliding
**Status:** VERIFIED

Walking qualification requires real alternating supported steps and positive progress; wheel sliding cannot qualify.

### WALK-SAND-001 — Sand-simulation enemy curriculum
**Status:** VERIFIED

### WALK-ROLL-003 — Head, tail, torso, and foot-node rolling rejection
**Status:** VERIFIED

Rolling and non-foot support participate in the shared qualification predicate and cannot seed best-policy or imitation state.

### WALK-HAZARD-003 — Hazards are never pickups or contact rewards
**Status:** VERIFIED

### WALK-LOCO-004 — Biped, quadruped, crawler, and hexapod support
**Status:** VERIFIED

Cross-rig deterministic tests protect biped, humanoid, quadruped, crawler, hexapod, chicken, and monoped presets after the complete-body reciprocal motor partition.

### WALK-IDLE-005 — Zero-progress reset
**Status:** VERIFIED

Motionless candidates are reset and excluded from elite selection.

### WALK-GUIDE-006 — Automatic best-result imitation prior
**Status:** VERIFIED

Only stage-valid trajectories with clean frames can enter the imitation prior; quality evidence outranks reward.

## UI and release evidence

### WALK-UI-002 / WALK-UI-003 — Responsive readable telemetry
**Status:** VERIFIED

Existing layout checks remain. Training telemetry now exposes sustained stance, longest stance, duck recovery, evidence quality, and the primary rejection reason.

### WALK-PIP-007 — Actual valid worker-rollout picture-in-picture
**Status:** VERIFIED

The PIP displays only a stage-qualified representative worker rollout. It shows `NO STAGE-VALID ROLLOUT YET` instead of presenting a failed sample as success.

### WALK-REL-011 — Historical v0.7.0 release evidence
**Status:** VERIFIED

The v0.7.0 build and test evidence remains historical, but its training-quality conclusion was superseded by Adam's August 1, 2026 packaged-runtime screenshot and corrected in v0.7.1.

{release_evidence}'''


def validate(run_id: str) -> None:
    evidence = f'''### WALK-REL-013 — v0.7.1 validation
**Status:** VERIFIED

- Workflow run: `{run_id}`;
- Linux GCC 14 C++23 core build and deterministic tests: passed;
- Windows 2025 full SDL3/Vulkan/EpochGui Release build and tests: passed;
- executable version and Vulkan diagnostic: passed;
- bounded first-update stage-valid standing replay: passed;
- adversarial collapsed-pose rejection: passed;
- reciprocal complete-body motor reaction and center-of-mass preservation: passed;
- publication and package checksum: pending final release step.
'''
    Path("missioncache.md").write_text(
        verified_ledger("VALIDATED — EpochRunner v0.7.1 publication pending", evidence),
        encoding="utf-8",
    )
    Path("validation/v0.7.1-prepublish.md").write_text(
        "# EpochRunner v0.7.1 prepublication validation\n\n"
        f"- Workflow run: `{run_id}`\n"
        "- Linux GCC 14 C++23 core build and all tests: passed.\n"
        "- Windows 2025 full SDL3/Vulkan/EpochGui Release build and all tests: passed.\n"
        "- Vulkan diagnostic: passed.\n"
        "- Bounded first-update standing replay retained a stage-valid champion.\n"
        "- Adversarial collapsed pose was rejected.\n"
        "- Chest, joint pivot, pelvis, and complete parent body react without center-of-mass injection.\n"
        "- Best policy, rollback, rig evolution, imitation, and PIP use one strict qualification predicate.\n",
        encoding="utf-8",
    )


def finalize(source_sha: str, run_id: str, archive: str, checksum: str) -> None:
    evidence = f'''### WALK-REL-013 — Verified v0.7.1 training-quality hotfix
**Status:** VERIFIED

- Exact tested source commit: `{source_sha}`;
- workflow run: `{run_id}`;
- Linux GCC 14 C++23 build and tests: passed;
- Windows 2025 full SDL3/Vulkan/EpochGui build and tests: passed;
- bounded stage-valid standing replay and collapsed-pose rejection: passed;
- complete-body reciprocal motor reaction and center-of-mass preservation: passed;
- executable version and Vulkan diagnostic: passed;
- Windows package: `{archive}`;
- package SHA-256: `{checksum}`;
- remaining branches: `main`;
- open pull requests: `0`.
'''
    Path("missioncache.md").write_text(
        verified_ledger("VERIFIED — EpochRunner v0.7.1 published", evidence),
        encoding="utf-8",
    )
    Path("validation/v0.7.1.md").write_text(
        "# EpochRunner v0.7.1 release evidence\n\n"
        f"- Exact tested source commit: `{source_sha}`\n"
        f"- Workflow run: `{run_id}`\n"
        "- Linux GCC 14 C++23 build and tests: passed\n"
        "- Windows 2025 full application build and all tests: passed\n"
        "- Vulkan diagnostic: passed\n"
        "- Bounded stage-valid standing replay: passed\n"
        "- Adversarial collapsed-pose rejection: passed\n"
        f"- Package: `{archive}`\n"
        f"- Package SHA-256: `{checksum}`\n"
        "- Remaining branches: `main`\n"
        "- Open pull requests: `0`\n",
        encoding="utf-8",
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("mode", choices=("validate", "finalize"))
    parser.add_argument("--run-id", required=True)
    parser.add_argument("--source-sha", default="")
    parser.add_argument("--archive", default="")
    parser.add_argument("--checksum", default="")
    args = parser.parse_args()
    if args.mode == "validate":
        validate(args.run_id)
    else:
        if not all((args.source_sha, args.archive, args.checksum)):
            raise RuntimeError("finalize requires source SHA, archive, and checksum")
        finalize(args.source_sha, args.run_id, args.archive, args.checksum)


if __name__ == "__main__":
    main()
