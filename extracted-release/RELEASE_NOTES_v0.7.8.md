# Runner v0.7.8

- Replaces sine-only uneven ground with a deterministic 224-cell, 56 m deformable sand heightfield shared by physics, observations, evaluation, replay, and both live/PIP rendering.
- Foot loading compacts loose cells, retains natural slip on soft support, displaces conserved volume into adjacent mounds, and relaxes over-steep slopes without creating or deleting terrain mass.
- Adds persistent falling sand, rocks, and debris; sand deposits into the terrain while rocks and debris bounce, roll, settle, and transfer impact velocity.
- Expands policy observations from 40 to 50 with firmness, looseness, burial depth, escape direction, incoming velocity, time-to-impact, density, obstruction mask, and surface slope.
- Adds burial/obstruction tracking, escape shaping, honest sustained no-escape termination, and PIP material telemetry.
- Adds seeded conservation, compaction, slope-collapse, material-spawn, observation-finiteness, and anti-tunneling regressions.
- Invalidates earlier policy/autonomy state with training semantics v0.7.8 and RUNAUTONOMY 13.
- Rebuilds the chicken around a vertical semantic torso and central load-bearing brace while retaining its horizontal bird body, raised head, beak, tail, leg-only motors, and separate feet; six seeded strict-balance runs guard the live 0/6 regression.
- Adds procedural biomechanical animation overlays to live rigs, the training PIP, and rig lab: semantic anatomy rings, neural-link pulses, motion-study ghosts, node halos, and a compact neural-chip motif with no new asset dependency.
- Expands material acceptance with deterministic repeated events, partial burial and escape-side detection, full no-escape burial termination, and direct/glancing impact anti-tunneling checks.
