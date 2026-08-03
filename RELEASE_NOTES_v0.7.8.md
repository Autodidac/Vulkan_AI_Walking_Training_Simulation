# Runner v0.7.8

- Replaces sine-only uneven ground with a deterministic 224-cell, 56 m deformable sand heightfield shared by physics, observations, evaluation, replay, and both live/PIP rendering.
- Foot loading compacts loose cells, retains natural slip on soft support, displaces conserved volume into adjacent mounds, and relaxes over-steep slopes without creating or deleting terrain mass.
- Adds persistent falling sand, rocks, and debris; sand deposits into the terrain while rocks and debris bounce, roll, settle, and transfer impact velocity.
- Expands policy observations from 40 to 50 with firmness, looseness, burial depth, escape direction, incoming velocity, time-to-impact, density, obstruction mask, and surface slope.
- Adds burial/obstruction tracking, escape shaping, honest sustained no-escape termination, and PIP material telemetry.
- Adds seeded conservation, compaction, slope-collapse, material-spawn, observation-finiteness, and anti-tunneling regressions.
- Invalidates earlier policy/autonomy state with training semantics v0.7.8 and RUNAUTONOMY 13.
