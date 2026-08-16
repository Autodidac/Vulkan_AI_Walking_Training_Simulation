# Runner v0.7.35 PIP proportions and posture truth

Runner v0.7.35 closes the discrepancy visible in the v0.7.34 training screenshot: the full preview used acceptable modular-armor proportions while the live training PIP looked like a large-headed, large-footed miniature, and a stage-safe candidate could still advance with a sustained backward-braced torso.

## One proportional presentation contract

The modular armor remains assembled from the supplied side-view runtime pieces and fitted to the authored physical graph. All screen-space safety bounds now share the same pixels-per-meter presentation scale. At the 42 px/m reference view, the existing full-size appearance is unchanged. PIP, zoomed-out, zoomed-in, mirrored, and main views scale boots, limbs, hands, torso, helmet, weapon thickness, joint overlap, and fallback envelope limits together. Skeleton geometry and armor therefore keep the same normalized silhouette instead of letting fixed pixel minima dominate only the PIP. The helmet envelope is shorter and anchored down toward the torso. The humanoid rest graph keeps its accepted arm segment lengths while placing both hands beside the pelvis, and limb sprites receive only the whole-rig facing reflection; the rear branch no longer flips the supplied asymmetric hand and limb textures upside down.

## Direction-aware gait posture

Forward-gait stages measure torso lean relative to the rig's authored root-to-torso axis and the current commanded travel direction. Only supported paired-leg traversal after the existing settling grace accumulates evidence. Backing, braking, turning, airborne motion, unsupported collapse, and non-biped topologies do not fabricate brace time. A lean opposite travel above the 0.24 normalized threshold must persist for more than 0.80 simulation seconds before it rejects a candidate; normal stride oscillation clears the contiguous timer.

The same evidence now participates in reward shaping, incremental retention, strict stage qualification, evaluation aggregation, plain-language rejection telemetry, and Advanced diagnostics. The timing is accumulated from simulation `dt`, with deterministic 20/60/240 Hz boundary coverage.

## Controller-lineage truth

`RIG UPDATES` remains the selected rig's cumulative optimizer work. `POLICY AGE` remains work in the active controller lineage. The former `DISCARDED` label is now `PRIOR LINEAGE`: cumulative work retained in rig/all-time totals but no longer part of the active lineage after an explicit fresh-controller start, a rig boundary, or compatible lifetime import. Thus a manual fresh start can truthfully produce `630 total / 38 policy / 592 prior lineage` without implying that those totals vanished.

Candidate success text now says `STAGE-SAFE CANDIDATE` and `LATEST TEST: STAGE-SAFE`. These are incremental retention results, not full lesson mastery; the goal and repeated mastery confirmations remain authoritative.

## Persistence and compatibility

Training semantics are `0x0007'3501`, checkpoint identity is `EPPO35`, and automatic files are `runner-v0735-posture-*`. EPPO35 retains the established metrics byte layout, so v0.7.34 and older compatible checkpoint readers remain deterministic. A v0.7.34 automatic save contributes validated lifetime totals only when no current v0.7.35 save exists; policy, optimizer, rig champion, and mastery state start under the new posture semantics.

## Deterministic proof

`Runner.V0735PipPosture` contains normalized PIP/main/zoom armor ratio controls; invalid-scale adversarial fallbacks; both-direction forward-versus-backward lean controls; zero-axis and zero-direction negative controls; exact boundary and over-boundary retention qualification; 20/60/240 Hz contiguous-duration equivalence; manual fresh-controller lineage arithmetic and underflow protection; and packaged renderer/diagnostics source audits.