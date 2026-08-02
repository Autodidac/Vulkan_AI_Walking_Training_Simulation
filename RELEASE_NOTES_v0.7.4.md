# Runner v0.7.4

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
