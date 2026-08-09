# Runner v0.7.30 sustained-walk learning recovery

Runner v0.7.30 repairs the Walk / Run learning path exposed by the packaged eye test: 24,363 rig updates could leave a policy only 173 updates old, with one credited step, no retained best controller, and 1,693 preview restarts. The fix treats that as a failed optimizer/curriculum contract rather than a rendering or tuning problem.

## Root causes

Forward-gait policies were eligible for periodic no-champion nursery randomization. The resulting roughly 540-update cycle repeatedly erased partial walking skill while cumulative rig updates continued rising. Evaluation also reduced many physically different partial walkers to the same invalid score, and strict-test failure could immediately restore an older snapshot before the current network received enough accepted updates.

The foundational gait itself had two representation contradictions. A continuous 1.2 Hz walk transfers support about every 0.42 seconds, but Walk qualification required a 0.75-second static planted stance. Multi-support demonstrations also used topology-scaled oscillators while observations exposed only a different global phase. The same observed phase could therefore demand different motor targets on later cycles, which no deterministic policy could learn.

## Coherent controller and optimizer path

The foundational biped reference is an authored two-link inverse-kinematics foot path. It alternates planted backstroke and lifted swing trajectories, solves hip/knee targets from the real authored segment lengths, and applies only motor actions. The multi-support reference derives every driven motor from the authored support topology and uses an exact observable clock for tall four-support, compact four-support, and six-support rigs. Neither path translates the root, advances course coordinates, or enables conveyor motion.

Every rollout stores the pure guided target separately from the sampled/executed action. A dedicated supervised actor phase runs after PPO minibatches, so gait imitation is not numerically drowned by policy/value gradients. PPO continues to optimize reward and residual control; demonstration authority is a finite curriculum input, not permanent evaluation motion.

Teacher authority is topology scoped. The stripped four-motor biped holds full authority through update 300 and reaches zero at update 500. The arm-equipped humanoid holds through update 600 and reaches zero at update 900. Multi-support rigs hold through update 700 and reach zero at update 1200. At the exact boundary Runner preserves the learned network and Adam moments but clears assisted-era champion state. Any retained controller after the boundary must be republished under zero teacher authority.

Strict-valid evaluation quality occupies a reserved tier above partial-invalid quality. Incremental candidates can still be retained before mastery, but a later invalid candidate can never overwrite a strict-valid champion merely because it accumulated more strides or scalar reward. Forward-gait nursery replacement is disabled; same-rig training continues from partial skill while canonical rig changes still reset incompatible policy, optimizer, best, and rig-scoped totals.

## Motion truth and telemetry

Walk accepts either the original static support proof or sustained dynamic support. Paired rigs need repeated alternating events and genuine sagittal limb crossings; multi-support rigs need repeated authored gait cycles. The micro-motion accumulator now requires a high-energy low-progress window with zero new authored gait events; real support transfer clears it, while vibration without gait remains invalid. Body contact, invalid motion, crab gait, missing progress, structural failure, skating, and rolling remain rejection evidence. Final biped mastery requires an 18-metre distance and a 14-step six-seed average, with every step still requiring alternating sides, five-frame airtime, clearance, displacement, and strict motion validity. The packaged eye test separately requires an individual 18-metre / 16-step replay frame.

The Live view distinguishes all-time rig updates from current policy age and displays discarded-policy count, distance, real step/cycle evidence, preview restart count, and the retained invalid-motion reason. Rollout, evaluation, champion selection, preview, rig evaluation, and retained-trajectory sampling consume the same topology-scoped authority schedule and course-motion-disabled locomotion frame.

## Deterministic gates

`Runner.V0730ColdStart` is a real bounded learner, not a static helper test. From fresh state it runs rollout workers, PPO optimization, dedicated supervised actor updates, immutable publication, six-seed evaluation, best-policy retention, and the visible preview. Its default 1,200-update gate places the four-motor biped, eight-motor user-visible humanoid, quadruped, crawler, and hexapod beyond their teacher handoffs, then directly replays each retained strict-valid champion with zero teacher authority and zero invalid evaluation seeds. Both paired-leg subjects must reach at least 18 metres and a 14-step six-seed average; multi-support rigs must retain repeated physical contact cycles. Course motion remains disabled and preview resets remain bounded.

The same diagnostic runs ten deterministic 20-second physical reference probes for the stripped biped, arm-equipped humanoid, quadruped, crawler, and hexapod, including a repeated seed. It also compares complete preview particle state, prior positions, elapsed simulation time, distance, gait evidence, reset count, and reset reason at 20, 60, and 240 render Hz. Half-tick resets, negative frame deltas, and non-finite deltas cannot advance or contaminate physics state.

Current-source fresh Linux 1,200-update evidence before final release packaging:

- stripped biped: retained best update 810, 22.4835 m / 41.5 real strides / 0 of 6 rejected seeds;
- arm-equipped humanoid: retained best update 1085, 24.509 m / 17.1667 real lifted strides / 0 of 6 rejected seeds;
- quadruped: 7.44092 m / 52.3333 support cycles / 0 of 6 rejected seeds;
- crawler: 8.92786 m / 45 support cycles / 0 of 6 rejected seeds;
- hexapod: 29.0896 m / 57 support cycles / 0 of 6 rejected seeds.

`Runner.exe --walk-eye-test` is the packaged visual proof for the screenshot's arm-equipped humanoid. It performs 1,200 fresh updates, including 300 updates after the humanoid handoff, rejects a missing, partial-invalid, assisted-era, or pre-handoff champion, directly replays the retained policy across six deterministic seeds with teacher authority exactly zero, and opens a frozen production-renderer frame captured during a real lifted support transfer after 18 m / 16 steps. The frame labels the retained update, mean and displayed distance, lifted steps, sagittal crossings, replay seed, authority, and six-seed validity. It cannot fall back to the teacher or a canned pose.

These values are deterministic development gates, not a substitute for the final installed and extracted packaged eye test. Final release still requires GCC 14 warnings-as-errors and all Linux tests, the complete Windows SDL3/Vulkan build and tests, every diagnostic, installed and independently extracted launch from unrelated directories, ZIP checksum and per-file manifest audit, published-asset re-download and byte comparison, and a direct packaged sustained-walk eye test.

## State and compatibility

Training semantics are `0x0007'3001` and automatic state uses `runner-v0730-walk-*` paths. Older policy files may be used only through the existing explicit weight-transfer path; incompatible optimizer, best, mastery, and curriculum state cannot resume silently. Policy and observation dimensions remain unchanged in this release.