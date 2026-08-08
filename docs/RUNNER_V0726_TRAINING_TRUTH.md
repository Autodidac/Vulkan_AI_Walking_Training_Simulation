# Runner v0.7.26 training truth

## Root causes fixed

The locomotion controller previously treated action slots 0-3 as support legs and action slots 4+ as upper-body. That mapping only matches the humanoid family. Quadruped and crawler rigs use all eight action slots for legs, and hexapod uses six. Several lesson assists therefore damped or zeroed real front/support legs while telemetry still reported normal learning.

Motor role is now derived from the authored rig graph. A motor is a support motor when its driven branch reaches a semantic support seed. The same rule is used by balance, crouch, walking, and effective-policy blending.

## Rig switching

`TOTAL RIG UPDATES` is scoped to the selected training subject. Switching to a different canonical rig clears the new rig's cumulative counters and starts Stand with fresh policy/optimizer/best state. Episode failures, nursery policy retries, and same-rig recalibration keep that rig's totals.

Lesson-entry baselines are captured immediately after the command is applied, so progress cannot inherit an old rig's update/evaluation baseline.

## Preview

The large live preview disables the moving-course conveyor. It must generate its own world displacement. Training workers retain their curriculum pressure independently.

Automatic preview termination records the invalid-motion reason before reset. The live overlay displays restart count and the last reason, so a fall/overspeed/collapse restart is visible instead of looking like an unexplained teleport.

## Art

The packaged runtime `foot_side.ppm`, `helmet_side.ppm`, `torso_side.ppm`, and `weapon_side.ppm` are enabled automatically when present. Torso art is bounded to the real root/torso span; the renderer never draws an entire concept sheet over the rig.
