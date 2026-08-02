# Runner v0.7.5

- Replaces static duck folding with a foot-only crouch-walk lesson.
- Adds uneven crouch terrain, low-bar avoidance, small ground hazards, and useful reaction distance.
- Invalidates any duck attempt where knees, hands, torso, head, tail, or other non-foot nodes touch terrain.
- Rebuilds the training PIP around a current valid moving crouch, the full connected rig, nearby terrain, and the next obstacle.
- Invalidates v0.7.4 duck checkpoints and autosaves so the failed 10,000-update policy cannot masquerade as progress.
- Preserves the current working chicken preset.
