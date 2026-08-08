# Runner v0.7.24 structural integrity, truthful telemetry, and screenshot icon

Runner v0.7.24 closes three packaged eye-test failures: compressible legs, misleading training totals, and an application icon that did not use the requested gameplay screenshot.

## Rigid structural contract

Every `DistanceConstraint` is a fixed-length bone. The `stiffness` field remains serialized only for compatibility and is normalized to `1.0` when a blueprint is authored, loaded, or installed into an environment. Automatic controller tuning may alter motor strength or joint range only. It cannot change nodes, radii, contact roles, topology, rest lengths, or bone stiffness.

Motors, articulated toes, crouch guidance, ground contact, terrain, course collisions, and support pressure may move joints during an iteration. A final structural projection runs after those operations so the displayed and scored pose finishes with authored segment lengths. Excessive residual length error is an explicit invalid-motion reason rather than a learnable shortcut.

## Stage-qualified accounting

A completed parallel rollout is not called a pass merely because it avoided a terminal physics fault. `PASSED STAGE CHECKS` means the completed environment satisfies both body-integrity and current-stage qualification. Every other completed rollout is counted under `FAILED STAGE CHECKS`.

`SIMULATED RUNS` describes the high-volume parallel PPO workload. These are not human-visible mastery tests. `TESTS` are the deterministic evaluation cycles used to confirm the current controller.

## Honest lesson completion

The dashboard separates:

- **Training work:** required updates, completed simulation runs, and evaluations.
- **Mastery passes:** consecutive repeat evaluations that satisfy the final lesson target.
- **Lesson completion:** 80% training work plus 20% mastery evidence.

A lesson with zero mastery confirmations cannot display 100% completion.

## Screenshot-derived application icon

`assets/ui/runner_icon_source.png` is the canonical square crop selected from the supplied gameplay screenshot. The icon generator verifies its SHA-256, copies the exact source into generated/package assets, and derives the 256×256 PNG, 512×512 PNG, SDL BMP, and multi-resolution Windows ICO only by pixel resampling. It does not redraw or replace the screenshot content.

## State isolation

The training semantics version and autosave names are advanced to v0.7.24. Older policies remain explicit transfer inputs, but old compliant or compressible rig state cannot silently resume as the current structural contract.
