# Runner v0.7.41 four-rig natural gait

Runner v0.7.41 makes the shipped subject catalog match the intended game-facing set: Human, Chicken, Dog, and Hexapod. Generic Biped, duplicate Quadruped, and unstable Monoped are retired from all visible/runtime/release matrices. Their constructors remain solely for compatible legacy rig import and low-level regression tests.

## Physical rig contracts

Human retains the eight-motor authored plant and now receives a physical casual-stride score normalized by authored leg length. Useful credit requires moderate opposed foot separation, bounded swing clearance, a relaxed cadence, and low backward brace. Tiny shuffling, high marching, overstride, frantic or stalled cadence, and rearward resistance are explicit negative cases.

Chicken is a compact side-elevation bird with a forward beak, rear tail, and two independent hip-knee-foot chains. Dog uses the distinct Crawler4 four-support topology instead of a duplicate generic quadruped. Hexapod has six knee/foot chains: four fully powered outer chains and two passive articulated middle knees, with support phases derived from authored topology.

Armor is presentation geometry wrapped around the physical plant. Helmet and boot sizing follow physical dimensions, nonhuman heads remain world-upright in side view, and the four production rigs share the art pipeline without changing their physics.

## Terrain, telemetry, and persistence

Authored terrain fine cells and macro tiles remain immutable. Contact does not excavate or raise the course. Only explicitly dropped granular overlay cells can fall, stack, settle, or be removed.

The top panel reports TRAINING LEVEL rather than an unexplained difficulty percentage. Before final checks are eligible, it reports training work and FINAL CHECKS WAITING. During and after checks it reports running, passed, or complete states, avoiding a false zero-of-eight failure signal.

Training semantics are 0x0007'4102, checkpoint magic is EPPO41, autonomy state is RUNAUTONOMY 23, and automatic files use runner-v0741-natural-gait-*. v0.7.40 can contribute validated lifetime totals only; incompatible controller, optimizer, champion, and mastery state start fresh.

## Release evidence

Release acceptance covers positive, negative, adversarial, repeated-seed, and 20/60/240 Hz cases. The production matrix cold-trains and directly replays Human, Chicken, Dog, and Hexapod at zero teacher authority. The retained physical distances are 38.4640 m, 31.4363 m, 30.2172 m, and 17.9177 m respectively, with zero invalid retained seeds and no conveyor/course translation.

The full Windows SDL3/Vulkan build passes 35/35 CTest suites and Linux GCC 14 warnings-as-errors passes 29/29. Version, Vulkan, package, 16/16 acceptance, camera, UI, art, rig-training, walk-eye, immutable-course, and hybrid-brain diagnostics pass from the build tree and through the installed launcher from an unrelated working directory. The local release audit independently extracts both archives, verifies runtime and tracked-source manifests and checksums, and byte-audits the source tree against the exact commit. No remote publication is part of this local-only release.
