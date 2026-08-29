# Runner v0.7.47 species rigs, full Rig Lab control, and retained mastery

Runner v0.7.47 replaces the held v0.7.46 package. The v0.7.46 website asset remains unpublished.

## Species-owned rig integrity

Canonical Human, Chicken, Dog, and Hexapod preset buttons now restore factory blueprints only. Disk files are loaded only through the explicit load action. Live morphology commits must preserve the selected species identity: support nodes are unique terminal ground contacts, never root, torso, or head roles; each support has an independent parent branch; Dog has exactly four support branches, Chicken two, and Hexapod six with actuated distal and proximal joints. Invalid or cross-species edits are rejected before they can mutate training state.

## Species art

Chicken and Dog body, head, leg, foot, and tail modules use species-specific scale and attachment profiles. Dog tail orientation is corrected. Generated PPM sprites use a binary alpha mask and the renderer tolerates only near-key saturated magenta, removing extraction fringe without erasing authored pink pixels. Human world scale remains the reference.

## Full control during Rig Lab

Rig Lab keeps the complete Autonomous Rig Trainer panel active beside the preset/motor controls and blueprint editor. Autopilot, lesson status, training speed, camera controls, units, Summary/Totals/Advanced telemetry, and diagnostics remain visible and clickable. On wide displays the live world is also retained as a fourth pane; narrower supported windows preserve every control and a usable editor, and the full live world returns automatically when all four panes fit.

## Retained mastery confirmations

After a stage-safe controller earns its first mastery confirmation, Runner freezes optimizer updates, restores the retained best controller, and runs the remaining confirmation tests against independent deterministic evaluation seeds. A failed confirmation resets the streak; a successful streak advances normally. This prevents a good retained controller from being changed between confirmations and remaining indefinitely at 80 percent.

## Persistence and release

Training semantics are `0x0007'4701` and species-owned state uses `runner-v0747-*`. Older v0.7.46 checkpoints may contribute lifetime totals only; policy, optimizer, retained-best, mastery, rig, and autonomy state do not cross the semantic boundary. The release workflow requires the built Windows ZIP, sidecar checksum, per-file manifest, exact-commit source ZIP, independent extraction, launcher diagnostics, and published-asset byte verification.
