# Runner v0.7.37 hybrid locomotion and coherent terrain

Runner v0.7.37 closes the packaged v0.7.36 failures visible in the 2026-08-16 screenshots: backward-braced forward walking, multi-support rigs that cycle contacts without travelling, fragmented slab-like terrain presentation, and a rig that can remain trapped in a depression without selecting an exit.

## Runtime brain and learned policy

The saved weights are the adaptive motor layer, not the whole enemy-AI controller. A deterministic fixed-step planner runs from current physics observations on every simulation step. It predicts urgent falling-material threats from density, velocity, and time to impact; holds while an active granular hazard is unsafe; detects stalled progress in constrained terrain; selects free space or the lower exit wall; escalates to escape or crawl; and bounds saturated policy output with topology-aware physical teacher motion. The planner never reads renderer state and never translates the rig.

`Runner.exe --diagnose-hybrid-brain` verifies hole escape, an unseen falling-object dodge, harmless-object rejection, blocked-exit fallback, policy safety authority, quadruped/crawler/hexapod physical travel, and exact 20/60/240 Hz intent equivalence.

## Physical gait truth

Upright forward balance now targets ground-relative travel instead of preserving a backward-authored torso axis. Multi-support branches are discovered from authored contact topology. Four-support rigs use two-link endpoint paths with diagonal support groups; the one-link hexapod uses opposing tripod phases and constant stance preload. Release qualification rejects contact-count inflation and requires useful ground-relative distance, survival, and stride evidence at zero conveyor motion.

The repeated-seed reference matrix requires at least 10 m / 20 events for four-support rigs and 14 m / 24 events for six-support rigs. The implementation pass produced worst-case 20-second reference distances of 22.34 m for quadruped, 21.34 m for crawler, and 19.30 m for hexapod across ten seeds.

## Granular field and presentation

Physics remains the pinned SandHybrid field. Sand and loose material can fall, settle, deposit, excavate, bury, and form holes; water occupies depressions. The course tail now uses long seeded regions with boundary fading instead of short alternating slabs. Rendering samples the same authoritative field, blends material color across adjacent samples, draws a shallow material band rather than a moving rectangular underlayer, and labels only coherent non-overlapping runs. Tests retain deposition, burial/escape, direct and glancing falling impacts, renderer/collider source contracts, repeated seeds, boundary continuity, and 20/60/240 Hz granular-state equivalence.

## State compatibility

Training semantics are `0x0007'3701`, checkpoint identity is `EPPO37`, autonomy state is `RUNAUTONOMY 19`, and automatic files are `runner-v0737-hybrid-*`. An EPPO36 automatic checkpoint may contribute validated lifetime totals when no current save exists. Incompatible policy, optimizer, retained champion, rig, and mastery state start fresh.

## Local release artifacts

The audited local release contains `Runner-v0.7.37-windows-x64.zip` and `Runner-v0.7.37-source.zip`. Each archive has an adjacent SHA-256 checksum and per-file SHA-256 manifest. The source ZIP is made from the final release commit, excludes `release-assets/` and all untracked build/dependency staging, and must pass an independent extraction, tracked-file-set, and byte audit before delivery.
