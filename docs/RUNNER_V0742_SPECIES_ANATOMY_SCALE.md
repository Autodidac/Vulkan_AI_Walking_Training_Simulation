# Runner v0.7.42 species anatomy, scale, and natural control

Runner v0.7.42 closes the gap between a topology that can technically move and a production creature that reads correctly in the side-view game. The production catalog remains exactly Human, Chicken, Dog, and Hexapod. Generic Biped, duplicate Quadruped, and unstable Monoped remain available only for compatible legacy rig import and low-level regression coverage; they are not selectable production subjects.

## Presentation and physical scale

Human remains the immutable visual and camera reference. Its authored torso scale is not reduced to make another rig fit. Camera framing uses the Human reference height, so a compact Chicken stays visibly small, Dog remains below Human shoulder height, and Hexapod keeps its authored multi-support footprint instead of every subject being normalized to the same screen height. `--art-eye-test=<human|chicken|dog|hexapod>` freezes any production subject at that same scale for packaged comparison; retired or unknown names are rejected.

Chicken, Dog, and Hexapod each ship a dedicated orthographic modular atlas and six runtime modules: body, head, upper leg, lower leg, foot, and tail. The renderer selects modules from the actual production species and does not reuse Human armor as animal anatomy. Each module is fitted to the authored segment envelope, follows segment rotation and facing, and keeps the debug skeleton available underneath the presentation.

## Authored anatomy and locomotion

Chicken uses a compact paired avian leg chain and a species-sized contact foot. It no longer inherits the Human passive-foot enlargement that made a 0.61 m body attempt a 0.54 m planted stance. Its stance target and support-cycle evidence are derived from the actual avian chain.

Dog is the canonical four-support subject. The duplicate Quadruped label is retired. Its front and rear branches keep their authored roles, alternating support phases, dog scale, and dog presentation.

Hexapod has six articulated knee-and-foot support branches. The outer four branches are the eight policy-controlled motors and use the wider locomotion range; the middle pair is coupled at a bounded support range so it stabilizes rather than dragging or locking the plant. Release evidence requires real six-support progress, not torso translation with idle legs.

Human uses bounded casual-gait evidence based on authored leg length, physical stride separation, swing clearance, cadence, upright travel, and backward-brace ratio. Arm stabilization is deliberately weaker and shorter-travel than leg drive so the hands rest near the sides and swing oppositely without flying outward to mask a bad center of mass.

## Shared environment truth

Authored fine cells and macro tiles are immutable. Ordinary contacts cannot excavate, raise, ripple, or reclassify the course. Only explicit granular overlay cells may fall, stack, settle, or be removed. Rendering, collision, observations, diagnostics, and curriculum read the same authored surface plus the same explicit overlay.

Back / Turn / Return physically reflects the articulated plant and evaluates controller inputs in facing-local space. The return leg must self-propel toward Turn A; course translation and backward dragging are not accepted as locomotion.

## Persistence boundary

Training semantics are 0x0007'4202, checkpoint magic is EPPO42, and autonomy state is RUNAUTONOMY 24. Human, Chicken, Dog, and Hexapod each own `{species}.rig`, `runner-v0742-{species}-autosave.eppo`, `runner-v0742-{species}-evolved.rig`, and `runner-v0742-{species}-autonomy.state`. A cross-species rig or checkpoint signature is rejected before live state changes. A v0.7.41 autosave may contribute only finite, monotonic lifetime totals; its controller, optimizer, champion, curriculum mastery, and rig-scoped active state do not resume across this anatomy and control boundary.

## Release contract

The local release gate covers repository hygiene, MSVC and GCC warnings-as-errors, the complete CTest matrix, package/acceptance/camera/UI/art/course/hybrid-brain/rig-training/walk-eye diagnostics, installed and independently extracted launch checks, exact runtime and source manifests, and archive byte audits. Direct packaged eye testing remains authoritative for species scale, art ownership, stance, gait, facing, terrain synchronization, PIP framing, and UI readability.
