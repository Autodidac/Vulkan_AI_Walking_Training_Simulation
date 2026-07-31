# EpochRunner

EpochRunner is a C++23 Vulkan locomotion laboratory built with SDL3, EpochGui, vcpkg manifest mode, and a compact PPO controller. Version 0.6.2 restores real biped foot support and traction, prevents head/tail/body contacts from pinning the actor, and keeps rocks and other course debris anchored to the moving course instead of the creature.

## Biped traction and world anchoring hotfix

Passive heel/toe triangles now count as the left or right support cluster used by observations, gait validation, airborne checks, rewards, and recovery. Only those designated foot clusters receive strong traction; incidental head, tail, or torso contact slides instead of acting like an unintended brake. Procedural rocks and hazards use stable sequence/world coordinates and no longer inherit root translation. Version-specific autosaves prevent incompatible v0.6.1 controllers from silently resuming under the corrected contact model.

The v0.6.2 interface also uses larger typography, wider responsive side panels, wrapped trainer messages, grouped runtime/result cards, and split world telemetry so labels remain readable instead of overlapping.

Course markers and obstacles now share one eight-metre schedule. Each lesson starts with three clear markers of safe runway, then advanced training cycles rocks, hurdles, overhead bars, moving hazards, and thrown projectiles at consecutive markers. The virtual course moves quickly enough to reach those events in practical training time while every feature remains anchored to course coordinates rather than following the actor.

Walking reward now requires foot-led gait evidence. Sliding forward with both feet planted receives no startup progress credit, grounded foot slip is penalized, sustained wheel-like motion is a hard invalid gate, and a knee crossing a rock or hurdle before its corresponding foot receives an explicit penalty and visible fault count.

Pausing background training now clears queued single-update backlog immediately, and the unused PPO constant that blocked strict Linux warning builds has been removed.


## Procedural obstacle and recovery treadmill

Version 0.6.0 continuously advances a bounded training course even when the creature does not translate. The course cycles through flat road, inclines, plateaus, declines, hills, uneven terrain, rocks, hurdles, low bars, moving hazards, and thrown projectiles. Road dashes and numbered metre/mile markers expose movement clearly in the live view.

Obstacle impacts and large balance errors start a timed recovery objective. Policies receive extra reward for restoring upright supported balance and a penalty when recovery times out.

## Default workflow

Version 0.6.0 visibly identifies itself in the title bar and UI, uses uniquely named release assets and clean autosaves, rejects synchronized hopping as walking, applies motors to the full driven subtree, and keeps persistent rollout workers instead of recreating threads every update.

The application starts in **Live Autopilot**. The foreground renders one current best verified controller while background CPU workers train 64 non-rendered environments. Vulkan remains dedicated to presentation instead of drawing every training agent.

The trainer automatically:

- starts by learning to stand and balance;
- advances to flat walking only after three deterministic mastery checks;
- introduces ramps, uneven terrain, hurdles, overhead bars, and moving hazards in order;
- saves full checkpoints and the current evolved rig whenever a new verified best is found;
- restores the best verified controller after repeated degradation;
- tests tiny symmetric rig changes and keeps only changes that improve deterministic valid-walking score;
- resumes its curriculum, optimizer, metrics, controller, and evolved rig on restart.

The **Rig Lab** remains available for inspecting joints, testing individual motors or groups, selecting A/Pivot/C, changing safe travel limits, and manually correcting geometry.

## Walking validity gates

A controller or rig candidate cannot become the published best when any deterministic evaluation run:

- flips upside down;
- exceeds 50 km/h;
- leaves the bounded course;
- remains airborne long enough to exploit flying;
- produces repeated high-energy micro-movement without meaningful displacement;
- fails to produce alternating foot contacts after the balance lesson.

Invalid episodes terminate immediately and receive a large penalty. These are hard gates, not merely weak reward preferences.


### Quadruped-stable bodies and real feet

Every built-in body now derives its motor defaults from the stable quadruped profile. Hips/shoulders use a symmetric 22-degree envelope and knees use 30 degrees. Power is normalized by driven-limb length, so a longer humanoid leg receives the same effective endpoint correction as the quadruped instead of being launched by the same raw strength.

Bipeds, humanoids, chickens, and monopeds now have passive heel/toe triangles rather than balancing on one circular contact point. Every episode begins with a short no-control settling period and a smooth motor ramp, while fresh policies start with lower output weights and exploration. This preserves each body shape while copying the quadruped's stable startup behavior.

## Human-calibrated defaults

The humanoid proportions and asymmetric hip/knee ranges are based on a user-trained six-node rig that was physically closer to a human body. Its motor powers were reduced by roughly ten percent, a separate head node was restored, and the learned flip/fly policy itself was deliberately not imported.

## Performance model

- 64 training environments by default.
- NORMAL, FASTER, and MAX CPU select increasing persistent worker budgets and duty cycles while reserving capacity for Vulkan presentation.
- A C++23 coroutine supervisor stages command application, parallel PPO work, curriculum handling, immutable publication, and speed throttling.
- Persistent workers now handle rollout simulation, PPO minibatch gradients, and deterministic policy evaluation.
- Rig edits are coalesced through a non-blocking command queue and never wait on a training update.
- Only the live best controller is rendered.
- The policy contains only a few thousand parameters; keeping its optimizer on CPU avoids the synchronization and transfer overhead of dispatching this tiny network to the GPU. Vulkan is used where it is efficient: smooth real-time presentation.

## Controls

- `1`: Live Autopilot
- `2` or `3`: Rig Lab
- `Space`: Pause or resume background training
- `R`: Reset the live preview
- `S`: Save rig in Rig Lab
- `L`: Load rig in Rig Lab
- `Delete`: Remove the selected node in Rig Lab
- `Shift + click`: Add a node
- `Ctrl + click`: Connect the selected node to another node

## Build

Windows:

```powershell
cmake --preset windows-release --fresh
cmake --build --preset windows-release
ctest --preset windows-release --output-on-failure
```

Dependencies are resolved through `vcpkg.json`, including `sdl3[vulkan]`, shaderc, and the Vulkan loader.
