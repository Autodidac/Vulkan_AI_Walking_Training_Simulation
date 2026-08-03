# Runner v0.7.7

- Restores rig-specific standing: quadrupeds and other multi-support presets no longer receive biped-only hip/knee corrections.
- Tests every motor alone in both directions, synchronized groups, and alternating patterns during dedicated early discovery lanes.
- Reduces compounded teacher dominance so PPO retains meaningful residual exploration and can branch beyond one pose.
- Replaces global left/right support pushing with authored-order non-overlap separation for every semantic support node.
- Rejects humanoid and biped jumping-jack support spans during standing and static crouch qualification.
- Changes static crouch to knee-first or authored compact-support control, then rewards the hold and recovery rather than accidental post-platen walking.
- Invalidates v0.7.6 checkpoints and autonomy state so the failed standing/crouch policies cannot be reused as valid progress.
- Keeps deformable sand terrain and falling-material/burial recovery explicitly carried in `missioncache.md` for the subsequent terrain release.
- Allows natural foot sliding during crouch and locomotion; only no-step planted-foot friction shuffling loses gait credit and receives a mild shaping penalty.
- Preserves cumulative evaluation accounting across recalibration and autosave state so evaluations, mastery, PPO updates, and the PIP remain synchronized.
