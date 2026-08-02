# Runner

## Ordered skill curriculum

Training now advances through stand, duck/recover, jump/land, walk/run, moving duck/jump, controlled flips, and a mixed goal course. Hazard contact is legal: collision applies physics and a bounded event penalty, while passing the obstacle earns progress. Joint-powered launches receive bounded airtime, controlled airborne flips may reach three spins, and a fourth spin, ground rolling, hovering, or unpowered sustained flight remains invalid.

Walking convergence now uses phase observations, temporally smoothed actions, and a decaying early gait guide. A step counts only after real swing airtime and clearance, so foot-contact wiggles and double-supported skating cannot masquerade as gait. PPO updates are less destructive, exploration is bounded, and every substantial or invalid regression restores the best valid champion immediately instead of allowing late training to degrade beyond it.


Runner is a C++23 Vulkan locomotion laboratory built with SDL3, RunnerGui, vcpkg manifest mode, and a compact PPO controller. Version 0.6.5 completes the guided sand-simulation enemy pass: true four-leg and six-leg support semantics, longer safe training runway, startup-only rolling grace, strict mature rolling rejection, zero-motion episode reset, automatic best-result self-imitation, and a real worker-rollout picture-in-picture.

## Guided multi-leg training release

Version 0.6.5 replaces the old two-contact quadruped approximation with a true four-leg body and gives the four-leg crawler and six-leg hexapod explicit multi-foot support groups. Near and far legs are staggered for readable side-view alignment, while every semantic foot receives the same flat-sole traction and anti-pivot-rolling treatment.

The first obstacle is now held beyond a forty-metre safe runway so a fresh policy can establish balance and gait before debris arrives. Rolling gates allow a brief startup settling window, then become strict. Episodes that produce no translation, no new gait step, and no useful foot lift reset promptly instead of consuming most of a rollout.

The trainer automatically records clean frames from its best valid stepped result and uses them as a small decaying actor-only imitation prior. Invalid body-contact and orange-foot rolling frames are excluded. The live view also includes a compact upper-right picture-in-picture showing an actual exploratory worker rollout rather than a duplicate deterministic replay.

The former foot-before-knee restriction is no longer a rigid ordering rule. Natural bent-knee lead and useful raised-foot clearance are allowed; only an obvious low-foot body-first shove receives a mild shaping penalty.

## Autonomous locomotion hotfix

The trainer now starts with spawn stance and flat sand patrol, then introduces isolated sand mounds and loose/deformed terrain before debris. Early rocks, hurdles, and low bars are generated only on flat zones. Terrain-plus-hazard combinations remain locked to the later combat-traversal lesson at higher difficulty.

Head, tail, torso, and other non-foot ground contacts no longer provide a locomotion path. Sustained body rolling is a hard invalid gate, body-ground motion receives no gait multiplier, head/body contact receives an immediate penalty, and new versioned autosaves prevent the old rolling controller from resuming.

Course features remain physical hazards after contact and are retained until they pass behind the actor instead of disappearing at approach distance like collectible upgrades. Obstacle contact no longer opens a positive recovery-reward opportunity; collisions are strictly costly while recovery remains a survival state.

The bitmap UI now uses a substantially larger global scale, larger minimum fitted text, larger marker signs and hazard labels, wider panels, a larger default window, and sand-sim-specific labels.

## Biped traction and world anchoring hotfix

Passive heel/toe triangles now count as the left or right support cluster used by observations, gait validation, airborne checks, rewards, and recovery. Only those designated foot clusters receive strong traction; incidental head, tail, or torso contact slides instead of acting like an unintended brake. Procedural rocks and hazards use stable sequence/world coordinates and no longer inherit root translation. Version-specific autosaves prevent incompatible v0.6.1 controllers from silently resuming under the corrected contact model.

The v0.6.2 interface also uses larger typography, wider responsive side panels, wrapped trainer messages, grouped runtime/result cards, and split world telemetry so labels remain readable instead of overlapping.

Course markers and obstacles now share one eight-metre schedule. Each lesson starts with five clear markers forming a forty-metre safe runway, then advanced training cycles rocks, hurdles, overhead bars, moving hazards, and thrown projectiles at consecutive markers. The virtual course moves quickly enough to reach those events in practical training time while every feature remains anchored to course coordinates rather than following the actor.

Walking reward now requires foot-led gait evidence. Sliding forward with both feet planted receives no startup progress credit, grounded foot slip is penalized, sustained wheel-like motion is a hard invalid gate, while only an egregious low-foot body-first obstacle shove receives a mild shaping penalty and visible fault count.

Pausing background training now clears queued single-update backlog immediately, and the unused PPO constant that blocked strict Linux warning builds has been removed.


## Procedural obstacle and recovery treadmill

Version 0.6.0 continuously advances a bounded training course even when the creature does not translate. The course cycles through flat road, inclines, plateaus, declines, hills, uneven terrain, rocks, hurdles, low bars, moving hazards, and thrown projectiles. Road dashes and numbered metre/mile markers expose movement clearly in the live view.

Large balance errors start a timed recovery state. Obstacle impacts are penalized and never grant a recovery bonus; failed recovery still receives an additional penalty.

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

The **Rig Lab** remains available for inspecting joints, testing individual motors or groups, selecting A/Pivot/C, changing safe travel limits, and manually correcting geometry. Built-in enemy bodies now include the humanoid and basic bipeds, a true four-legged quadruped, a separate four-legged crawler, and a six-legged hexapod.

## Walking validity gates

A controller or rig candidate cannot become the published best when any deterministic evaluation run:

- flips upside down;
- exceeds 50 km/h;
- leaves the bounded course;
- remains airborne long enough to exploit flying;
- produces repeated high-energy micro-movement without meaningful displacement;
- fails to produce alternating foot contacts after the balance lesson;
- travels on its head, tail, torso, knees, or other non-foot body contacts.

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


### One-click Windows launcher

Double-click `run.bat`. In an extracted release it launches the packaged executable. In a source checkout it uses an existing Release build or configures and builds `windows-release` before launching. Command-line arguments are forwarded, including `run.bat --diagnose-package`.


Windows:

```powershell
cmake --preset windows-release --fresh
cmake --build --preset windows-release
ctest --preset windows-release --output-on-failure
```

Dependencies are resolved through `vcpkg.json`, including `sdl3[vulkan]`, shaderc, and the Vulkan loader.
