# Runner v0.7.5

- Replaces static duck folding with a foot-only crouch-walk lesson.
- Adds uneven crouch terrain, low-bar avoidance, small ground hazards, and useful reaction distance.
- Invalidates any duck attempt where knees, hands, torso, head, tail, or other non-foot nodes touch terrain.
- Rebuilds the training PIP as an honest live training view: the complete rig stays large, nearby terrain and obstacles remain visible, distant obstacles get a distance label instead of shrinking the rig, and failed attempts stay visible with the exact rejection reason.
- Shows update number, crouch time, crouch distance, alternating steps, and passed obstacles directly in the PIP.
- Invalidates v0.7.4 duck checkpoints and autosaves so the failed 10,000-update policy cannot masquerade as progress.
- Preserves the current working chicken preset.
