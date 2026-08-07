# Runner v0.7.21 readable training dashboard

Runner v0.7.21 makes the dashboard understandable and corrects the rig/gait defects visible in v0.7.20. Policy dimensions and terrain physics remain stable, but corrected anatomy and gait semantics use isolated v0.7.21 autosave state; older checkpoints remain explicit transfer inputs.

## The five questions the default dashboard answers

1. **Is training healthy?** The headline says whether the trainer is starting, training normally, testing, retaining an improvement, retrying after a rejected test, trying a fresh controller, paused, or finished with the lesson.
2. **How much work has this lesson received?** `LESSON PROGRESS` is the lowest completion percentage among required controller updates, completed simulation attempts, and repeat evaluations. This prevents one large counter from hiding a missing category.
3. **What happened in the latest test?** The dashboard says `PASSED`, `NOT YET PASSED`, or `WAITING FOR FIRST TEST`, then translates the rejection into ordinary language.
4. **What useful behavior has it demonstrated?** Stage-specific evidence replaces the arbitrary raw score: standing time, valid seeds, crouch/recovery, distance, stride events, jump landings, obstacle passes, or controlled flip landings.
5. **What must happen next?** The panel states the exact current mastery goal and whether more updates, attempts, or repeat tests are required before the behavior can be judged fairly.

## Meaning of the primary counters

- **Total updates:** every completed controller-learning cycle across the life of the saved trainer. A rejected attempt, episode reset, policy retry, or rollback does not erase this number.
- **Lesson updates:** controller-learning cycles completed since entering the current lesson.
- **Attempts:** completed simulated episodes. These include useful failures because PPO learns from them.
- **Valid attempts:** episodes that passed the current safety and skill gates.
- **Tests:** repeatable evaluation runs used to judge whether a controller should be retained or whether a lesson is mastered.
- **Retained champion:** the best validated controller saved for the current training lineage. A later regression can be rolled back to this controller.
- **Resets:** episode or weak-policy restarts. They do not mean all-time training was deleted.
- **Rollbacks:** the trainer restored a better retained controller after a regression.

## Color rules

- **Cyan:** information, startup, active training, or testing.
- **Yellow:** a current goal is not met yet, a test was rejected, or a fresh controller is being tried.
- **Green:** valid evidence, a retained improvement, or mastery.
- **Red:** reserved for broken rig state, invalid numerical/runtime state, packaging failure, or a presently terminal motion fault. Ordinary negative reward/score values are never treated as a user-facing emergency.

## Advanced diagnostics

The `ADVANCED` page preserves raw evaluation score, best score, quality key, rejection mask, policy/value loss, entropy, learning rate, optimizer state, environment steps, worker throughput, and pipeline details. Non-finite startup values are shown as `NOT AVAILABLE` rather than `-INF` or `INF`.

## Keyboard behavior

`T` cycles `SUMMARY`, `TOTALS`, and `ADVANCED`. The default after launch is `SUMMARY`.


## Fixed-anatomy training

Automatic curriculum refinement may adjust motor strength, joint range, and bone stiffness. It cannot move nodes, change limb length, widen feet, add or delete branches, or reassign supports. Anatomy changes are manual Rig Lab operations only.

## Side-view gait truth

A biped step is credited only when the swing support begins behind the stance support, leaves the ground, achieves useful clearance, passes ahead, and lands on the opposite contact phase. Permanent split stance, shuffling, treadmill-only progress, and a foot that remains ahead do not count.

## Preset and Rig Lab contract

Bipedal presets use compact side-view silhouettes. Quadruped and four-leg crawler presets have four independent articulated two-segment legs. The hexapod has six independent supports split into alternating tripod phases and no support-to-support brace. Rig Lab separates Presets, Structure, Motors, and Test controls, clips each page, and automatically centers/fits the complete rig in the viewport.
