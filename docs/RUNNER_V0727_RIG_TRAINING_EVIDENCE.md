# Runner v0.7.27 rig-training evidence

v0.7.27 makes locomotion accounting follow authored support nodes from simulation through evaluation. Multi-legged rigs still use two controller phase groups, but a strike is now observed at each physical support seed. A quadruped, crawler, or hexapod can therefore retain grounded stability with some legs while other authored legs produce measurable swing and contact-transfer evidence.

The anti-skating gate remains strict when every support is planted. It now distinguishes that case from a recent physical multi-support transfer or a currently lifted authored support. This prevents the old aggregate-group model from rejecting legitimate planted phases between multi-legged steps.

Trainer rollout, champion evaluation, and the large preview no longer receive conveyor-derived locomotion distance. Terrain and hazards remain physical course features, but forward progress must come from rig displacement. This closes the mismatch where rollout distance could look healthy while a static preview moved backward or not at all.

All product simulation paths are render-frame independent. Training, evaluation, acceptance, and diagnostics already step at a fixed 60 Hz. The large Live preview now converts render elapsed time into bounded fixed 60 Hz ticks before evaluating the policy or advancing physics, contacts, terrain, and reset logic. Camera smoothing and UI clocks remain elapsed-time based. The deterministic regression compares complete preview physics state after equal wall time at 20, 60, and 240 Hz and verifies reset and invalid-delta behavior.

`Runner.exe --diagnose-rig-training` performs a deterministic 100-update comparison of biped, quadruped, crawler, and hexapod rigs. It reports rollout distance, static-course evaluation distance, stride evidence, invalid evaluation seeds, preview restart count/reason, and whether course motion leaked into rollout workers. The diagnostic requires every non-biped evaluation curve to reach the biped baseline within a small fixed margin and to produce physical stride events.

The fixed v0.7.27 seeds show that the three non-biped evaluation curves reach or exceed the current biped baseline after 100 updates. Their remaining failures are now honestly clustered and visible: micro-motion for quadruped/crawler and intermittent foot-pivot rejection for hexapod, rather than a topology-wide absence of gait evidence.
