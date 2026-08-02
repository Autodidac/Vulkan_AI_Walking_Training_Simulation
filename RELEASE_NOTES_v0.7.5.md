# Runner v0.7.5

- Carries falling sand/debris avoidance, impact recovery, burial escape, and continuation training into the next release mission ledger.
- Carries full deformable sand-cell terrain integration into the next release mission ledger rather than delaying this correction package.
- Splits static crouching from crouch walking: stand and static crouch require no movement, walk/run must be mastered next, and moving crouch comes afterward.
- Restores monoped progression by counting real forward single-leg landing cycles instead of demanding alternating biped footfalls.
- Keeps training strictly staged: eight consecutive strict successes lock the best controller, then the next lesson builds on it without random mixed replay.
- Allows controlled somersaulting without requiring a separate powered-launch flag, permits forward-facing prone recovery outside crouch lessons, and retains the hard three-rotation limit.
- Reorders the curriculum to stand, walk/run, then crouch-walk and duck obstacles, so the chicken and other rigs must learn ordinary gait before combining gait with ducking.
- Replaces static duck folding with a foot-only crouch-walk lesson.
- Adds uneven crouch terrain, low-bar avoidance, small ground hazards, and useful reaction distance.
- Invalidates any duck attempt where knees, hands, torso, head, tail, or other non-foot nodes touch terrain.
- Rebuilds the training PIP as an honest live training view: the complete rig stays large, nearby terrain and obstacles remain visible, distant obstacles get a distance label instead of shrinking the rig, and failed attempts stay visible with the exact rejection reason.
- Shows update number, crouch time, crouch distance, alternating steps, and passed obstacles directly in the PIP.
- Invalidates v0.7.4 duck checkpoints and autosaves so the failed 10,000-update policy cannot masquerade as progress.
- Preserves the current working chicken preset.
