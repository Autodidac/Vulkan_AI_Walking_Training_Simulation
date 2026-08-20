# Runner v0.7.38 turn topology and scoped statistics

Runner v0.7.38 closes the v0.7.37 packaged-runtime failures visible in the 2026-08-20 screenshots: the clean humanoid gait broke at the shuttle turn, the monoped and chicken were trained through incompatible support assumptions, and the default Totals page mixed selected-rig, app-session, and lifetime scopes.

## Physical shuttle direction

A shuttle boundary no longer reflects articulated particle positions or changes a knee/elbow inverse-kinematics branch. It preserves the physical pose, clears only stale Verlet and angular velocity, changes the direction-relative phase clock, and derives limb bend direction from the authored chain geometry. The same signed travel frame now drives gait targets, facing, posture, evidence, and retention.

Strict shuttle retention orders evidence by completed physical turns, then ground-relative traversal distance, then real support cadence, then elapsed evidence. This prevents a high-frequency in-place or short-range gait from outranking a controller that safely covers the authored shuttle, while leaving the 18 m / 14-event multi-seed release threshold unchanged.

## Authored single and avian support

The monoped starts from a bent, non-singular side-view chain and earns support gait cycles from verified plant/air/clearance/root-travel transfers rather than an impossible left/right alternation. Its teacher uses bounded rest-centred hip, compression, and rocker targets. The chicken keeps its authored root-driven leg graph; contradictory rigid root braces are removed, its two support legs remain phase-opposed, and body stabilization is support-relative.

The production cold-start diagnostic and learner enumerate all seven canonical rigs. Monoped and chicken therefore cannot be omitted behind teacher-only probes: each must retain a strict-valid, zero-curriculum-authority controller and pass its topology-specific distance and physical gait-cycle threshold.

Fragile authored body plans also ship a small deterministic code brain. Monoped support timing/recovery and avian plant stability are blended with the learned residual action only in Walk and Back / Turn / Return; human, paired-leg, four-support, six-support, and non-walking stages receive zero topology-reflex authority. Candidate evaluation and the visible preview execute this exact same production composite, so a teacher-only or raw-policy-only pass cannot hide a broken shipped controller.

## Truthful totals

The default page has three explicit scopes in fixed order: SELECTED RIG - THIS SELECTION, THIS APP SESSION - ALL RIGS, and SELECTED RIG - LIFETIME. Wall time is distinct from parallel agent-simulated time. Distance remains agent-equivalent displacement, while SUPPORT GAIT CYCLES is topology-neutral and does not claim that a monoped hop or avian plant is a humanoid step. Session deltas saturate at zero across fresh-controller resets and are accumulated once across rig switches.

## State and release

Training semantics are 0x0007'3801, checkpoint identity is EPPO38, autonomy state is RUNAUTONOMY 20, and automatic files are runner-v0738-topology-*. An EPPO37 automatic checkpoint may contribute validated lifetime totals when no current save exists; incompatible policy, optimizer, retained champion, rig, and mastery state start fresh.

The local release contains Runner-v0.7.38-windows-x64.zip and Runner-v0.7.38-source.zip with adjacent SHA-256 checksums and per-file manifests. The source archive is exported from the exact audited release commit, excludes release-assets/ and build/dependency staging, and is independently extracted and byte-audited before delivery.
