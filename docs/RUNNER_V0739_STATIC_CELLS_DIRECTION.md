# Runner v0.7.39 immutable cells and self-propelled direction

Runner v0.7.39 closes two packaged v0.7.38 eye-test failures: authored terrain changed underneath the rig, and the reverse-direction lesson visually flipped the rig while a moving course frame dragged it backward.

## Immutable authored terrain

The course is authoritative fine-cell geometry with immediate macro-tile occupancy updates. Authored cells are tagged separately from runtime granular cells. Contact pressure is observation-only: standing, stepping, slipping, and landing cannot compact, erode, excavate, relax, ripple, or raise authored ground. The stored fine cells remain exact and immutable. Collision and rendering derive the same read-only, linearly connected surface between those cell centers, matching the visible cell boundary without introducing invisible stair-step ledges or changing any cell byte.

Only explicit material events create changing terrain. Dropped sand and dirt remain dynamic cells, fall under the fixed simulation clock, stack above authored cells, promote or demote macro tiles as occupancy changes, and may be removed by explicit granular operations. The authored base cannot be removed by runtime excavation. Repeated-seed tests compare every coarse field and every fine-cell material, flag, and fill byte after sustained pressure and stepping.

## Self-propelled reverse traversal

Back / Turn / Return is a static-course lesson even if generic course motion is enabled. Its course speed and course progress are exactly zero. The authored facing and locomotion direction change at the turn, while displacement must come from rig contacts and motor output. A deterministic regression test forces the global motion switch on and proves that the shuttle frame remains stationary before running physical and repeated-seed direction checks. The post-handoff production controller now retains a shuttle-only, topology-derived directional reflex, with learned weights providing bounded residual control, so a forward-specialized policy cannot brace against the faced return command.

## Cross-platform support calibration

The immutable collision adapter exposed a deterministic monoped handoff boundary rather than a reason to weaken acceptance. The final clean-prior gait keeps the 0.175-radian hip stroke, uses a 0.145-radian heel/toe rocker, and reduces knee compression to 0.46 while retaining 0.180 extension. The optimized MSVC and GCC 14 all-seven reference suites both pass the unchanged 5 m, 8-cycle, and 20-second monoped gates.

## Persistence and packaging

Training semantics are 0x0007'3901, checkpoint identity is EPPO39, autonomy state is RUNAUTONOMY 21, and automatic files use runner-v0739-static-cells-*. If no current save exists, an EPPO38 automatic checkpoint may contribute validated lifetime totals only; policy, optimizer, retained champion, rig, and mastery state start fresh.

The local release contains a Windows x64 runtime ZIP and an exact-commit source ZIP, each with an adjacent SHA-256 checksum and per-file manifest. Both archives are independently extracted and byte-audited before delivery.
