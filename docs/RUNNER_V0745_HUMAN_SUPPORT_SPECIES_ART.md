# Runner v0.7.45 Human support gait and species art fit

Runner v0.7.45 closes two packaged eye-test regressions that automated v0.7.44 checks missed: chronic Human knee flexion during the BACK / TURN / RETURN lesson, and incorrect Chicken/Dog head and tail presentation.

## Human support gait

- The saved `human.rig` remains the authoritative neutral pose.
- Human gait now reaches a near-straight planted leg at mid-stance while keeping bounded flexion during support exchange and forward-only swing-knee flexion.
- Human cadence, stride, swing clearance, and torso correction are tuned together; the preview still receives no conveyor or course-motion authority.
- The physical shuttle regression samples grounded hip-to-foot reach and requires repeated near-straight support evidence in addition to real turns, distance, gait transfers, strict integrity, and zero backward bracing.

## Species art fit

- Chicken and Dog now own explicit head scale, head attachment, tail scale, and tail attachment profiles.
- The Dog tail corrects the source module's transverse orientation independently of travel-direction mirroring.
- Chicken retains its authored tail orientation while using larger, connected head and tail modules.
- Human world scale, physics topology, policy dimensions, and contact semantics are unchanged.

## Persistence boundary

Training semantics are `0x0007'4501`, and species state paths use the `runner-v0745-` prefix. A matching v0.7.44 state may contribute lifetime totals only; policies, optimizer state, and retained champions restart at the corrected gait/art boundary. The unchanged on-disk formats remain `EPPO44` and `RUNAUTONOMY 26`.

## Release proof

The release gate includes deterministic positive, negative, adversarial, endpoint, both-facing, repeated-seed, and grounded support-extension checks; complete Windows SDL3/Vulkan and Linux GCC 14 warnings-as-errors matrices; runtime diagnostics; installed and independently extracted package execution; and exact-commit runtime/source archive audits.