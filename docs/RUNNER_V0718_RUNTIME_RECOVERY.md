# Runner v0.7.18 runtime recovery

This release is a packaged-runtime recovery pass driven by direct v0.7.17 eye testing. Terrain rendering, collision, pressure, deformable cells, treadmill transforms, and terrain coordinates are intentionally unchanged.

## Update-reset correction

PPO evaluates at updates 1, 5, and 10. v0.7.17 could automatically reset a no-champion policy on the third invalid evaluation, before the 120-update Stand fresh-work gate could complete. v0.7.18 requires an extended stage-aware nursery budget before an automatic restart and preserves cumulative totals.

## Visible progress

The trainer publishes cumulative updates plus current-stage fresh updates, episodes, evaluations, their thresholds, pipeline state, reset count, and throughput. The UI distinguishes all-time work from a resettable local policy counter.

## Reference markers

The physical course is unchanged. The live view restores a START reference and recurring 10 m metric / 50 ft imperial signs so motion can be judged immediately.

## Controls

- `Tab`: Live Autopilot / Rig Lab
- `Space`: run or pause background training
- `1`, `2`, `3`: Normal / Faster / Max CPU
- `T`: Training Results / Lifetime Totals
- `U`: Metric / Imperial
- `A`: optional body armor overlays
- `R`: reset live preview/camera
- Mouse wheel: live camera zoom

Optional torso, helmet, and weapon overlays default off. The forward foot sprites remain independent and visual-only.

## Walking recovery

Early Walk training receives stronger sustained sagittal bootstrap guidance. Existing crab-walk rejection, physical support requirements, sustained-distance requirements, and stage mastery gates remain strict.

## Release gate

The `v0.7.18` tag is created only after the merged `main` source passes Linux warnings-as-errors, the full Windows SDL3/Vulkan test matrix, installed and extracted package diagnostics, checksum/manifest generation, release upload, and release-asset re-download verification.
