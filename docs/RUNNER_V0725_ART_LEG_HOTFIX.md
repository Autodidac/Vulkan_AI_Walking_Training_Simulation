# Runner v0.7.25 compact armor and stance-leg integrity

Runner v0.7.25 addresses the remaining packaged eye-test failures from v0.7.24.

## Armor presentation

The optional helmet and support-foot artwork are retained unchanged. The oversized bitmap torso overlay is removed from runtime rendering. Optional body armor is now assembled from compact geometry attached to the actual root, torso, shoulder, elbow, and hand nodes:

- a narrow layered chest plate aligned to the torso axis;
- small shoulder caps located on the authored shoulder joints;
- restrained forearm guards following each forearm segment;
- a compact cyan chest indicator;
- no translucent rectangular body sheet, oversized circular shoulder blobs, or duplicate ghost arms.

The underlying physical skeleton remains visible enough to inspect gait and support transfer. Armor remains a visual-only toggle and never changes particles, radii, constraints, policy state, observations, or collision.

## Walking-leg integrity

Fixed segment lengths alone do not prevent a two-link leg from folding until the knee appears to telescope into the pelvis. The v0.7.25 post-solver now handles the full paired walking chain:

1. supported stance legs reserve a minimum hip-to-foot extension;
2. when a supported chain folds below that reserve, the upper-body assembly is lifted without moving the planted support;
3. each knee is reconstructed analytically from the authored upper- and lower-leg lengths;
4. the prior bend side is retained so a leg does not flip through itself;
5. both segment lengths are projected exactly after the extension correction;
6. crouch-walk, static crouch, monoped, quadruped, crawler, and hexapod behavior keep their dedicated motion paths.

The correction starts immediately in upright walking lessons rather than waiting through the visible startup interval. Swing legs may still bend naturally; only supported legs are prevented from collapsing into a compressed stance.

## State and release contract

Training semantics and autosave paths are advanced to v0.7.25 so older controllers cannot silently resume against the corrected stance-chain behavior. The release requires deterministic forced-compression recovery, natural walking soaks, exact segment-length checks, compact-art source checks, the complete Linux suite, full Windows SDL3/Vulkan build and tests, installed/extracted diagnostics, package checksums, release re-download verification, and branch cleanup.

## EpochGui font and progress synchronization

Runner's renderer-neutral bitmap font follows EpochGui commit `130f33fe31d73564a35a622f3bb5ddcc2b5105d5`: font sizes represent logical glyph height, and the renderer derives cell size, advance, measurement, and line advance from one shared metrics object. The application remains in logical SDL coordinates, so the font DPI multiplier is one while Vulkan maps the complete logical surface to the drawable surface.

The fallback glyph table includes `%`, preventing `30%`, `80%`, and `100%` from appearing as question marks. Once a lesson's sample budget is met, the compact header displays `UPDATES READY`, `RUNS READY`, and `TESTS READY` instead of misleading values such as `RUNS 17465/8`. Actual high-volume simulation totals remain available on the Totals page.

EpochGui is consumed read-only for this synchronization pass. Runner pins and mirrors the published font-sizing contract without modifying the EpochGui repository, so concurrent EpochGui development remains isolated.