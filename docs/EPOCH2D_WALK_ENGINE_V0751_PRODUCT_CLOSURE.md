# Epoch2DWalkEngine v0.7.51 product-closure contract

## October 23 source-only standing review

At the user's request, the October 23 pass ran no build, simulation, training,
benchmark or runtime test while another project was under heavy load. That change
makes Stand's authored posture helper honor physical guidance authority: root,
torso, head and any other nodes moved by that helper receive proportionally scaled
position and history corrections, and zero authority skips it. Its posture-guide
report uses the same scalar. Tests are authored for four species and four seeds,
including partial/zero authority, nonfinite input, repeatability and Raw exclusion.
Subsequent focused Windows authority checks ran this change; they do not establish
an unaided Stand-to-Walk handoff.

This fixes one authority defect. The current Stand lesson still requests full
authority, blends its balance teacher and evaluates Assisted. It has no complete
handoff to unaided standing. Before that curriculum can be changed, extend actual
Raw-origin collection and actor attribution, retire assisted episode/best state,
qualify a retained standing policy, and preserve the separate Walk provenance
requirements. Reducing just the posture helper does not establish Raw identity for
other appendage/contact/actuator paths. Bounded physical tests are needed before
connecting this change to automatic training or claiming stand/walk/recovery.

## October 21 physical/preview audit

Interactive launch was paused for the October audit. Current Live automation
starts Eco training on fresh startup or after applying a valid autosave. The
interactive main replay does not auto-retry. Eco uses one active
training worker and idles for three times each completed update's duration;
Balanced uses up to two and idles for one update-duration. Maximum explicitly
removes that duty throttle, with a default pool ceiling of four. The library may
opt into a larger ceiling. These are training budgets, not promises about total
process CPU: rendering, live physics and persistence are separate. Pause finishes
an already-started atomic update but starts no further updates. Commands, mode
changes and shutdown interrupt budget waits. Paused workers service commands and
persistence without continuously polling at 125 Hz.

Training semantics `0x00075115` supersede `0x00075114`: prior support assumptions
cannot count as current mastery. An incompatible autosave keeps authored rigs and
lifetime totals, explicitly replaces policy/optimizer state, clears Director
evidence, and returns to Stand. Interactive startup enables Eco after migration;
callers that did not opt into startup training remain paused. Existing checkpoint
files are not deleted.
Explicit unverified weight transfer remains distinct from validated resume. This
is necessary retraining hygiene, not evidence that the unsupported body has
already learned to balance or walk.

The interactive inset shows a sampled worker rollout, a labelled recorded
outcome when no current rollout can be shown, or a waiting state. It is not a
second camera on the main view. The main view runs a separate fixed-step replay
with one controller and course. A new worker task or policy is staged until R;
a committed rig change replaces the incompatible replay. A terminal frame stays
visible for inspection without stopping background training. R starts a new
trial, not a worker reset. A failed terminal frame is not successful recovery.

Human support exchange now requires the newly landed foot to carry at least half
the measured foot load for two consecutive clean receipts. Flat contact alone is
not weight transfer. `RunnerV0748ContactArtTests --support-transfer-only` exercises
the focused contracts; it is deliberately not a substitute for the default full
landing and sustained-gait test. Strict Raw forward/reverse walking and recovery
remain blocking. Rejected capture/hip/ankle controller experiments were removed;
their outcomes remain in `missioncache.md`. No v0.7.51 publication is authorized by
these partial checks.

v0.7.51 is the direct visual-acceptance pass opened after packaged v0.7.50 Human motion, extremity art, course scale, zoom, and Rig Lab presentation were rejected by operator screenshots. It is an active release candidate, not a published release. A passing metric or version string is insufficient evidence: the rendered packaged application and its physical telemetry must agree.

## Current candidate state

The November 12 Windows build passed replay ownership, package/camera/UI
diagnostics, and the 16-case live acceptance matrix. These are not unaided gait
evidence. Raw Human forward/reverse trials recorded zero alternating steps,
and the Human lake trial fell before reaching the water. Both gates remain red.
The full Windows and Linux matrices and release promotion are not complete.

The candidate implements the canonical deep material map, authenticated Director/challenge boundary, physical finite-ammunition equipment sequence, integration API 7/action-layout schema 2/canonical snapshot schema 5, and their focused deterministic coverage. Raw retained-network inference remains release-blocking until its verified receipt and physical diagnostic pass. Complete Windows and Linux matrices, installed and independently extracted package audits, direct multi-phase bitmap acceptance, exact-commit archives, checksums/manifests, commit, tag, publication, public re-download, and cleanup are not claimed here.

## Product identity and workspace contract

The product is `Epoch2DWalkEngine`. `Epoch2DWalkEngine::Epoch2DWalkEngine` is the reusable cross-platform C++23 library, and `Runner.exe` is the named SDL3/Vulkan example that demonstrates the complete trainer, Live Autopilot, Rig Lab, renderer, terrain, and diagnostics. User-facing product, window, package, SDK, and current documentation identity must lead with Epoch2DWalkEngine; `Runner` remains only where it truthfully names the example executable or historical releases.

The interactive workspace is a production tool, not a permanent wall of equally weighted controls. Live Autopilot keeps current task, physical/safety state, progress, primary training controls, and the large simulation view prominent. Rig Lab keeps the same physical subject and full trainer capability, but compact windows expose one named inspector page at a time; wide windows may show trainer and rig inspectors together. Summary, totals, advanced evidence, and long explanation are distinct reachable pages with bounded clipping and scrolling. Switching pages or modes never changes simulation authority or hides a terminal failure.

Live and Rig Lab own independent presentation-art switches. A valid interactive session begins with Live art enabled and Rig Lab art disabled. The Live toggle affects only Live/PIP rendering, while the Rig Lab toggle affects only the editor preview; neither changes particles, contacts, observations, rewards, training, saved policy, or the other mode. Live camera zoom and Rig Lab editor zoom likewise remain independent. The Rig Lab fit control reports the actual bounded zoom while retaining its click-to-fit action.

## Canonical material world

The authored 640x360 footprint is a real canonical cell cross-section rather than a shallow surface strip over a decorative renderer fill. Its 0.140625-m cells place the surface near the middle of the grid, with `world_bottom = -25.3125 m` and `world_top = +25.3125 m`. The normal viewport therefore reads populated cell state instead of the former opaque fallback slab.

Deep columns use the pinned EpochSimEngine v2.5.29 `resident_ground_boundary_offset`, `resident_transition_material`, and `resident_ground_sample` contracts to create deterministic dithered sand/dirt/silt/stone layers, resources, and cavities. Exposed firm dirt carries a canonical Grass cell veneer while sand, mud, water, and structural challenge decks retain their own material identity; the veneer preserves the exact cell fill, height, aux state, and collision geometry. The same cell field retains the real surface profile, lake depth, structural step/ramp overlays, sampling, collision, and course coordinates. Render geometry may visualize those cells but cannot substitute a second collision surface.

The 640-cell horizontal period is 90 metres. One shared course-geometry contract supplies its extent and 45-metre centre to terrain and camera code. Placards remain tethered presentation only.

## Director and challenge authority

Integration API 7 carries selected graph index and control profile with the stable task and challenge identity. A task command is accepted only when its index, stable ID, task/challenge pair, prerequisites, evidence, and profile reproduce `director::select()` for the supplied graph and control request. Task nodes bind the stable ID of their expected physical challenge. A caller-provided support assertion or scalar mastery value cannot grant physical task authority. Schema-5 snapshots also carry action-layout schema 2's versioned 15-slot action-role mask and exact motor-to-node identities so an external host can reject commands for inactive anatomy or equipment before physics advances.

Supported nodes resolve to actual material intervals, lake/shore/gap boundaries, deterministic step/ramp or low-clearance features, or the active aim-range target. The fixed range is described as static; it is not advertised as a moving target. Recovery, construction, and tool traversal remain disabled until those distinct mechanics exist. The Director may choose a bounded task and explain the choice, but it cannot move a root, invent a contact, aim a joint, steer a projectile, or grant completion. Director state schema 2 persists a per-node physical receipt bound to one trial, task, challenge, and ordered tick interval. Water receipts require approach, entry, required aquatic phase, measured depth/submersion, shore exit, and a later far-shore dry sample. Stop/plant/stance/aim/fire-and-correct receipts require timed two-foot plant and settled physical aim, a real miss, a finite opposite-signed correction no larger than 0.22 radians, and a later lower-error physical hit. Wrong-ID, replayed, out-of-order, shallow, non-finite, hit-only, and scalar-only evidence is rejected without changing Director state.

Autonomous-state format 29 carries those receipts; scalar-only format 28 is intentionally non-migratable. The current PPO evaluation aggregate lacks the ordered water and physical shot/correction sequence, so automatic mastery of these nodes remains fail-closed rather than inferring success from distance, stance, transitions, shots, or hits. This is an explicit open producer obligation, not a waived release gate.

## Physical equipment sequence

The default Human begins with a finite loaded carbine in safe carry. Assisted equipment behavior preserves the locomotion controller while the physical body brakes, establishes a timed two-foot low-slip plant, enters gun stance, and drives the actual shoulder/elbow/hand chain through acquire and settle before READY.

The simulation rejects a trigger when body motion, plant duration, stance, joint settle, target/goal, cooldown/recoil, ammunition/reload, range, footing, visibility, or the measured two-hand grip is invalid. A projectile has finite flight and lifetime; terrain or obstacle collision is resolved before target overlap. A real miss supplies bounded observed vertical error to the next aim request. Completing the fixed hit goal returns the rig to safe carry and locomotion; an empty magazine follows the finite reload state instead of manufacturing a shot. API 7/schema-5 snapshots expose magazine and round counts, plant/reload state, fire-block reason, projectile result, target visibility, action roles, active slots, and motor node identities. The canonical RUNRIG7 Human persists 19 nodes with separately commanded left/right ankle and distal-toe motors; its observation vector is 84 values, including each sole's physical contact motion, two toe angles, two toe rates, and four finite support-phase-coordinator values. The fixed public action vector is 12 topology-masked anatomy slots plus 3 separate equipment slots. Equipment remains transient trial state. Its physical grip frame and historical 76/13 policy ABI first entered under `0x0007'5109`; `0x0007'5110` is the historical foundational-input/mixed-origin boundary, `0x0007'5111` is the whole-composite authority/plant boundary, `0x0007'5112` is the Human Uneven Walk support-feedback observation boundary, and `0x0007'5113` is the CrouchWalk authority boundary. Current `0x0007'5115` binds RUNRIG7's expanded 84/15 tensor and explicit toe-control contract. Explicit guidance keeps every non-target Raw scope phase-scrubbed and every non-target Assisted scope byte-compatible; retained integration uses the artifact's recorded guidance rather than ambient Environment mode. Checksummed `EPPO51` state may resume only through an opaque trainer- or exact-loader-issued artifact after full provenance/context/metric validation and exact EOF; `0x0007'5109`, `0x0007'5110`, `0x0007'5111`, `0x0007'5112`, and `0x0007'5113` are unverified transfer-only and cannot silently resume under `5115`. The unkeyed checksum detects corruption, not malicious-producer authenticity.

Automatic learned/evolved species state uses product-owned `epoch2dwalk-v0751-*` paths: `epoch2dwalk-v0751-{species}-autosave.eppo`, `epoch2dwalk-v0751-{species}-evolved.rig`, and `epoch2dwalk-v0751-{species}-autonomy.state`. At the v0.7.50 rename boundary, `runner-v0750-{species}-autosave.eppo` is lifetime-ledger-only compatibility input; its policy, optimizer, retained best, evolved rig, autonomy state, and mastery do not resume, while authored `{species}.rig` remains separately compatible.

Human Autopilot starts its new Stand lineage in `epoch2dwalk-v0751-raw-stand-human-*`, without writing the previous Human files. PPO collects Stand transitions directly from its joint policy on the unsupported plant. After the usual Stand training quota, the exact active candidate must pass three independent batches of six seeds, each sustaining six seconds of qualifying contact and posture, with no posture, root, torso, or course assistance. Optimization pauses during confirmations; the weights that pass are the weights that enter Walk. Human state schema 30 binds Director credit to the exact checksummed checkpoint and rig. Old assisted credit and standalone Walk checkpoints cannot bypass the prerequisite. Raw Stand does not certify recovery, a walking cycle, or a lake crossing.

Foundational Human Walk policy input now has one authority-independent shape: private teacher gait phase is scrubbed before every Assisted or Raw network evaluation, transition, retained replay, diagnostic, and self-imitation capture. Across the existing 300-update handoff ramp, Raw-origin environments execute only their sampled policy action and are the only PPO actor samples; complementary Assisted environments retain finite demonstrations and are the only inputs to the separately labeled decaying imitation pass. Self-imitation and retained-best anchoring are absent from updates containing Raw actor samples, and no Raw receipt is issued until the whole cohort is Raw. The existing single-support ankle balance target is Assisted training composition only, returns zero in double-support/airborne states, and cannot alter Raw/live effective actions. No posture projector, body hold, or burial escape is present in Raw. The Director `recover` task and burial-recovery mastery remain disabled until an unaided retained network produces repeated physical receipts.

The two-hand teacher is physical rather than a display pose: it discovers both articulated arm chains, binds the rear grip to the actual equipment-mount hand, binds the foregrip to the other real hand, and drives the authored joints through a finite equipment-only envelope. The authored rest arms permit only `62` degrees at each shoulder and `76` degrees at each elbow; equipment may opt into a symmetric `176`-degree range without changing the stored neutral or unarmed gait range. The completed grip-frame correction solves each real hand toward body-local shoulder-pocket/rear-grip and separated foregrip endpoints, selects continuous articulated branches, and uses that one measured frame for rendering, aim/rate, muzzle origin, visibility, miss correction, and finite two-point recoil. Invalid, crossed, coincident, non-finite, unseated, or incomplete grips fail closed before READY and cannot fire. The accepted deterministic `2048x1152` equipment proof is planted and READY at `0.005 rad` aim error and `0.010 rad/s` aim rate with no shot; it shows the carbine on a real two-hand upper-chest line. Focused physical-equipment and modular-art suites pass without a renderer-only weapon shift, root/contact write, hidden aiming state, synthetic hit, or relaxed settle criterion. The older waist-level and singular-reacquisition experiments remain rejected historical evidence. The combined V0749 combat/terrain suite is still blocked earlier by the separate Speed Walk locomotion gate, so this equipment completion is not evidence that the release-wide physical gait gate has passed.

## Raw inference gate — open

`RAW LEARNED` means the retained network owns every powered locomotion command. Raw execution may retain gravity, collision, finite material traction, passive distance constraints, and motor limits. It may not receive a lesson teacher, scripted gait replacement, topology locomotion reflex, posture or swing projection, contact-timed toe command, bootstrap/history action transform, static world-foot rail, root drive or translation, rigid-body pose projection, manipulator guide, or course motion.

Assisted remains an explicitly labeled hybrid/teacher mode and useful development evidence, but it cannot satisfy this release gate. The latest authoritative Raw audit produced network-directed forward stepping and then failed repeated-seed stability once optional guidance was removed. Consequently, no stable Raw retained-controller claim is made. The separate `--diagnose-raw-inference` gate writes the trainer-issued state to a real temporary `EPPO51` artifact, reloads it through the exact-EOF verifier, and must pass that immutable post-handoff retained network in forward and reverse across the declared seeds with stationary terrain, physical alternating support, bounded foot slip and root motion, valid body integrity, and zero optional locomotion authority. It reports every network joint command beside the effective powered motor command, plus per-joint raw/effective/actuator/motor ranges and mismatch counts, deterministic command and contact traces, contact transitions, stance slip, root displacement, and injected displacement/work. `--diagnose-walk-eye` remains a distinct physical replay and visual-capture gate; one cannot substitute for the other.

## Visual and Rig Lab acceptance

### Final UI source reconciliation receipt — September 6, 2026

The production interface now uses one Live `MISSION / VIEW / DETAILS` hierarchy and one Rig Lab command bar with mission, `START/PAUSE TRAIN`, `SETUP`, `TRAINER`, and `INSPECTOR`. Full training choices remain reachable in the contextual `TRAIN SETUP` drawer, narrow gait choices use full-width targets, and the Rig Lab rail stays within 420–720 logical pixels while retaining more than 900 logical preview pixels at 1368x799. Live Art and Rig Art keep isolated state and their intended defaults; Details owns detailed telemetry without duplicating the View overlay; and the Rig preview keeps persistent `ZOOM OUT / FIT / ZOOM IN` controls. A separate preview-title lane prevents those controls from covering Art, Bone, Motor, or Test headings, while physical-test detail starts below the toolbar. F11 toggles SDL fullscreen and Escape returns to windowed mode before it can quit.

MSVC 17.14 Release `/W4 /WX` builds and executions pass for `RunnerViewCameraTests`, `RunnerV0729ModularArtTests`, and `RunnerV0723GrayFrameTests`; `RunnerVisualCapture` also builds cleanly. The final deterministic software-raster matrix is `build/ui-final-reconcile-v0751-titlelane/manifest.txt`: 62 frames plus the manifest at 1368x799, 2048x1152, and true 3840x2160. Reviewed frames confirm readable compact Live/View/Details, non-overlapping 1368 Rig Art title/zoom controls, and the intentionally sparse fitted 4K preview. This is a source/layout/pointer/software-raster receipt only. Native Vulkan, packaged-runtime, and package-authentic eye acceptance remain open and cannot be inferred from it.

Live Replay and Trainer Snapshot render their own physical poses, terrain and
equipment state through the same production renderer and authored art layout.
Rig Lab's editor and parity view share the editor's physical pose. Human
heel/ball/toe progression, connected hands/forearms/ankles/boots, relaxed opposite
arm/leg phases, and bounded pelvis motion must be readable. Chicken, Dog, and
Hexapod must preserve connected species silhouettes through multiple contact
phases, lake entry/exit, and editor preview. Normal art cannot hide a broken chain
or define collision.

Rig Lab must retain the full trainer and statistics while Presets, Structure, Motors, Art, and Test operate on the selected production subject. Art anchors, pivots, scale, rotation, flip, layer, skeleton/contact overlay, gait scrub, zoom, contrast, undo/redo, species-owned save/load, and live commit must remain coherent with the live renderer. Visual-proof sessions must not read or overwrite interactive training state.

Direct packaged review uses the exact 60-capture `epoch2dwalk-native-capture-v2` matrix below. A successful launch, process exit, or generic capture count is not visual acceptance. Every required frame must have readable unclipped primary and persistent text, unobscured controls, correct selected-page state, and a usable subject scale. This applies at the exact 1368x768 or 1368x799 compact client size as well as default and ultrawide layouts. A visible scale, posture, contact, terrain, framing, overlap, clipping, or readability defect reopens the matching mission even when an automated diagnostic passes.

- Live: `live-compact`; compact top and bottom frames for the visible Mission, View, and Details routes; `live-default`, `live-ultrawide`, and `live-ultrawide-pip`. Retired internal Status, Totals, and Advanced labels are not accepted as substitutes for those operator-facing routes.
- World and Human water sequence: `course-overview`, `lake-approach`, `lake-immersion`, and `lake-exit`.
- Human foot phases: `human-heel-strike`, `human-flat-support`, and `human-toe-off`.
- Chicken, Dog, and Hexapod: `contact-loaded`, `contact-transfer`, `lake-entry`, and `lake-exit` for each named species.
- Equipment: `equipment-plant`, `equipment-aim`, `equipment-fire`, and `equipment-recovery`.
- Rig Lab: compact top and bottom frames for Presets, Structure, Motors, Art, and Test; normal and zoom frames for Presets, Structure, Motors, and Test; plus normal and zoom Art frames for Human, Chicken, Dog, and Hexapod.
- Failure truth: `terminal-state`, retaining the actual terminal pose and visible failure state.

Each matrix entry records exact `group`, `subject`, `phase`, and `viewport_class` values plus logical width/height, Vulkan drawable width/height, native DPI, file bytes, and SHA-256. The decoded PNG must equal the declared drawable size, while drawable dimensions must be the exact logical dimensions scaled from 96 DPI. Accepted logical client profiles are deliberately finite: compact 1368x768 or 1368x799, default 2048x1152 or 2560x1392, and ultrawide 3440x1440 or 3840x1600. Native DPI is one of 96, 120, 144, 168, or 192. The verifier rejects a missing, extra, reordered, semantically duplicated, byte-duplicated, wrongly sized, wrongly scaled, or malformed entry; an exact compact frame is valid evidence and is not rejected merely for being smaller than 2048x1152.

The packaged `--diagnose-camera` complements that eye test by exercising the actual Application route rather than only view formulas: real wheel input in the Live viewport must change both reported projection and emitted world geometry at 2048x1152 and 3840x2160, while real Rig Lab editor zoom must leave the saved Live camera unchanged across a tab round trip. Native fullscreen and button inspection remain required separately.

The September 5 native 1368x799 inspection reopened this gate. It proved that Live zoom input does change the world projection, but Rig Lab displayed a misleading fixed `FIT 1.00X` label after editor zoom. More importantly, an active Human Walk-Casual trial folded nearly horizontal around world x=81.95 m while reporting `MOTION BONE LENGTH ERROR` and both semantic feet airborne. Rig Lab reproduced that same failed physical pose until the canonical Human preset was deliberately reselected. This is honest shared-pose behavior, but it is an unacceptable kinematic/controller result; no renderer or camera correction may cosmetically upright it.

The same inspection rejected the dense three-column compact layout and underweight Human arms. The arm correction may add transverse authored-pixel mass and clearer cuff/hand separation only at the unchanged shoulder/elbow/wrist anchors. It may not add a generated capsule, ribbon, fake bone, or weapon-aligned duplicate limb.

## Terrain construction authority

The visual reference is the real connected-world bitmap produced by the operator's EpochSimEngine permanent worktree, not the Site's decorative artwork. It shows the authored 640x360 material footprint as a continuous cell cross-section: natural stratified soil/sand/stone, a grass surface, an open water shaft and reservoir, structural underground rooms, stairs, conveyor/material machinery, and a whole-world map view. That bitmap is layout evidence only; it is not parsed back into physics and it cannot override canonical material IDs.

Epoch2DWalkEngine therefore owns one canonical `SceneCell`-compatible field for rendering, surface collision, material observations, terrain editing, and external-host snapshots. Screen-space slabs, a second collision-only height field, or floating labels cannot impersonate that world. Surface challenges must remain physically traversable and include firm calibration ground, loose cells, waterlogged approach, shallow water with reachable shores, a recoverable depression, a bounded step, and a bounded ramp. Labels are optional presentation attached to true world coordinates; they may be omitted when they cannot be placed without overlap, and Rig Lab hides course placards by default.

## Implementation evidence and rejected approaches

The mission ledger is the authoritative chronological record. The following condensed results are the continuation boundary for this pass:

- Working: a stationary authored course, fixed 60 Hz simulation, canonical 640x360 material cells, deep stratified substrate, real water/rooms/caverns, species-owned rigs/art, separate Assisted and Raw authority labels, finite contact budgets, physical equipment/projectiles, external-host snapshots, and production-vertex visual capture.
- Working but still under final acceptance: shared Live/Rig Lab physical projection, exact species preset commits, irregular source-pixel silhouettes, explicit Human heel/ball/toe chains, separate manipulator/weapon layers, constructed-world rendering, and the responsive Trainer/Rig Inspector workspace. Ordinary fullscreen content through 4095 logical pixels shows one independently scrollable inspector beside a stable preview; simultaneous inspector panes begin only on larger canvases and the complete four-pane view is reserved for at least 5120 logical pixels. Live and Rig Lab own independent art defaults/toggles, the editor displays its real bounded zoom, and default course annotations are sparse while Advanced Challenge Details exposes the complete set.
- Rejected: root or terrain conveyors, root/COM translation, infinite or material-agnostic friction, art-only contact offsets, synthetic limb/boot slabs, acceptance-threshold relaxation, screen-space terrain decoration, interpreting a Site graphic or screenshot pixels as simulation state, and claiming Assisted output as unaided learned locomotion.
- Rejected lake experiments: broad low-firmness gait changes, enlarged static-capture anchors, stored positional correction, and generic history damping. Each either regressed dry gait, stored recoil, or left the deterministic lake failure unchanged.
- Open physical blockers: first-cause Human bone-length failure, stable bilateral Raw support transfer, finite stop/deceleration under player-guided Raw control, full four-species lake traversal, casual/transition/run/crouch-walk categories, and repeated-seed no-helper qualification.
- Open presentation blockers: native arm/hand/foot review, interactive compact/default/ultrawide confirmation of the implemented workspace and separate art/zoom state, and a final package-authentic multi-phase capture matrix.

## Continuation and release handoff

Do not resume by tuning success percentages or polishing a single frozen pose. Start from the first deterministic physical invariant that fails, retain strict negative/adversarial tests, and re-run the exact native scenario that reopened it. Every source change must update `missioncache.md`; newly discovered consequences stay visible there even when an experiment is reverted.

The release sequence has two immutable phases. Phase A stabilizes focused physical/UI/terrain tests; runs clean Windows and Linux warnings-as-errors builds plus every CTest and packaged diagnostic; explicitly builds and executes `RunnerVisualCaptures`; installs both runtimes and runs their launchers from unrelated working directories before either archive is created; independently extracts the completed runtime and SDK archives and repeats the launcher/consumer checks from separate unrelated directories; audits the exact archive/source bytes and manifests; and uploads those commit-bound build artifacts without publishing them. Native review must download and run Phase A's exact Windows archive, then record every required matrix frame from that package. Phase B names that successful Phase A run, downloads and re-audits its artifacts without rebuilding them, verifies the commit/tag-bound evidence bundle, and compares the Windows ZIP embedded in the evidence bundle byte-for-byte with Phase A's downloaded ZIP. Before the step-scoped HMAC secret is exposed, both promotion jobs authenticate the dispatched workflow SHA/ref and current run against the exact candidate through GitHub's run API. The protected Phase-B environment then creates a canonical HMAC-authenticated external promotion receipt binding the candidate commit/tag; named Phase-A run and attempt; build, promotion-input, and native-evidence artifact IDs/digests; exact ordered 15-object byte/hash inventory; Phase-A build and ABI receipts; native-capture URL/bytes/hash and recursively exact extracted capture tree; repository/workflow/promotion run identity; publication target; and the canonical `SITE-HANDOFF` readiness summary. Final audit consumes that receipt and the real inputs and fails closed on missing, altered, foreign, or context-mismatched evidence; it does not demand chronologically impossible post-build PASS prose inside the already archived candidate tree. Repeating the same verification is intentionally valid and immutable. Genuine one-use protection belongs to the separate atomic Site ingest/activation grant, not this stateless context receipt. The candidate's open/pending language therefore remains truthful until external evidence exists. Only then may the audited source be tagged and the immutable v0.7.51 objects be published to the existing Site. GitHub's guarded path byte-audits the draft before making it public, then strips authentication from fresh public downloads and verifies the exact 15-object file set, bytes, and SHA-256 values before rollback cleanup. A failed Raw, native visual, package, phase binding, or publication check leaves the candidate unpublished and the matching mission open.

The native evidence ZIP is itself fail-closed. It contains the exact Phase A Windows Runner archive that produced the frames, a commit/tag-bound JSON manifest, and the complete required capture matrix. Verification checks the embedded runtime's real byte count and SHA-256, fully parses and decompresses every non-interlaced 8/16-bit RGB/RGBA PNG through a terminal IEND with no trailing bytes, and rejects missing, extra, truncated, CRC-invalid, duplicated, or contradictory inputs. Phase B compares the embedded runtime byte-for-byte with the Windows archive downloaded from the named successful Phase A run; it may reuse that immutable object but may never reconstruct or rebuild it. Manifest fields or operator labels alone never establish packaged provenance.

`Runner.exe --equipment-eye-test` is the narrow equipment-art inspection entry. It creates the canonical Human equipment-target `Environment`, keeps course motion off, advances the real Assisted equipment teacher through fixed-step physics, and freezes the first valid no-shot READY frame whose physical target is visible, both-foot plant and aim settle timers have matured, aim error/rate are within the weapon gate, and motion remains valid. The window labels `ASSISTED TEACHER / NOT RAW` and the measured state; it neither edits particles nor substitutes a renderer-only pose. Failure to reach that frame prevents the Vulkan window from opening. The headless `--diagnose-art` path also renders this exact frozen environment and includes it in vertex-budget validation.

When native desktop automation is unavailable, build the explicit `RunnerVisualCaptures` target. Its non-installed `RunnerVisualCapture` helper initializes an isolated no-persistence `Application`, renders the exact production triangle stream at the logical 2048x1152 client size, follows Vulkan's ordered source-over blend and top-left pixel-centre fill contract, and writes binary PPMs under the build tree. The matrix includes the complete Live interface, full authored Course, four Art subjects, the physical Assisted READY equipment frame, Presets/Structure/Motors/Art/Test with the trainer/statistics and control panes intact, zoomed counterparts for every page, and Chicken/Dog/Hexapod Art-editor counterparts. `manifest.txt` records exact dimensions, FNV-1a payload hashes, vertex/raster counts, art-versus-no-art subject bounds and viewport fractions, plus wheel-zoom changed pixels constrained to the editor viewport. Convert PPM to PNG only as a review convenience; retain the PPM/manifest pair as the deterministic source receipt.

This tool deliberately does not link SDL or Vulkan and is not installed or packaged. Its output can expose scale, clipping, detached anatomy, tiny subjects, crowded controls, or editor/live disagreement for direct correction, but it cannot prove native DPI/window behavior, packaged assets, or Raw learned gait. Final acceptance still requires true packaged Vulkan frames, and the equipment frame remains explicitly Assisted.

## Library and release boundary

`Epoch2DWalkEngine::Epoch2DWalkEngine` remains the platform-neutral C++23 library; `Runner` is its SDL3/Vulkan example. Development packages identify Epoch2DWalkEngine at their root, include the installed external-host example, and preserve EpochSimEngine provenance under `docs/third-party/EpochSimEngine`. The generated package requires the exact publicly consumable `EpochSimEngine 2.5.29` branded dependency. It does not silently fall back to a legacy package name.

The Linux Runner archive carries the application and its packaged data, not a
graphics driver. Its launcher therefore requires the host's Vulkan 1.3 loader
and display stack; release validation records `ldd` resolution and executes the
independently extracted application against the validation host's conformant
Vulkan implementation. A missing `libvulkan.so.1` is a stated system dependency,
not silently accepted as a complete runtime.

A completed release requires repository hygiene and `git diff --check`; Linux GCC 14 warnings-as-errors and every CTest; the complete Windows SDL3/Vulkan build and every test; all feature diagnostics including Raw inference, walk-eye, camera, course, art, equipment/Director coverage, and package acceptance; installed and independently extracted runtimes and SDK consumers from unrelated working directories; exact archive checksums and per-file manifests; an exact-commit source tracked-file/blob audit; direct packaged bitmap acceptance; immutable commit/tag identity; publication re-download byte comparison; and cleanup. Only recorded evidence in `missioncache.md` may close those gates.
