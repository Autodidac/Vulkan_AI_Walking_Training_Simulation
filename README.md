# EpochRunner

EpochRunner is a C++23 Vulkan locomotion laboratory built with SDL3, EpochGui, vcpkg manifest mode, and a compact PPO controller. Version 0.5.0 replaces manual train/run switching with a continuously operating autonomous curriculum.

## Default workflow

Version 0.5.0 visibly identifies itself in the title bar and UI, uses uniquely named release assets and clean autosaves, rejects synchronized hopping as walking, applies motors to the full driven subtree, and keeps persistent rollout workers instead of recreating threads every update.

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
