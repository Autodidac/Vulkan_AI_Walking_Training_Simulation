# Runner v0.7.17 eye-test correction

The v0.7.16 released package was reopened by direct runtime review. Automated
acceptance did not overrule the visible failures.

## Physical feet

Bipedal presets now use one short physical support stub per leg. A side-view
boot sprite is anchored to the stub but is not collision geometry. This avoids
the old heel/ball/toe triangle bridging and snagging across SandHybrid cells.

## Gait truth

Walk qualification requires sustained distance, fresh stage work, repeated
alternating cycles, and signed leg crossing. A widened lateral stance or
forward motion without sagittal crossing is reported as crab walking and cannot
become a champion, imitation source, evolved-rig seed, or PIP-valid result.

## Quadruped press

Horizontal body plans receive a shallower, slower press target. Four-chain
compression and authored-pose extension are bounded, and recovery must be held
stably after retraction before the stage can complete.

## Optional user art

All four supplied concept sheets are represented by compact derived P3 references under
`assets/optional/runner_armor_concepts/source/`, with original and packaged SHA-256 values recorded in `PROVENANCE.md`. Derived P3 runtime sprites live
under `runtime/` for feet, helmet, torso, and a Rig Lab-only fictional weapon
preview. Optional art is visual only and can be disabled independently from the
debug skeleton. Missing or malformed optional assets fall back to procedural
rendering without changing physics or training.
