# Runner v0.7.15 release audit

This audit records the final consolidation state used to publish Runner v0.7.15.

## Mainline source

- Release source: `main` at or after merge commit `d4c8ad8956984727450133841bf6b725a9c58c35`.
- Project version: `0.7.15`.
- The complete v0.7.15 implementation is already merged into `main`.
- No open pull requests remain.

## Validation evidence

The exact release source passed the full Linux and Windows release gates:

- Linux GCC 14 deterministic suite.
- Windows SDL3/Vulkan application build.
- 9/9 Windows CTest suites.
- All eight Stand preset cases.
- All eight static crouch hold/recovery cases.
- Complete 24/24 live acceptance matrix.
- Build-tree, installed-package, and independently extracted-package diagnostics.
- `run.bat` launch from an unrelated working directory.
- Optional artwork fallback behavior.
- ZIP archive, SHA-256 checksum, and per-file manifest creation and audit.

The complete PR packaging run was GitHub Actions run `31021182630`.

## Branch consolidation

The remaining non-main branches contain no unreleased product implementation:

- `agent/v0715-complete` is fully superseded by `main`.
- `agent/v0715-ground-camera-character-scale` is fully superseded by `main`.
- `agent/v0715-complete2` contains only a historical marker file.
- `agent/v0714-run-observer` contains only an obsolete workflow observer and ledger marker.

They are intentionally not merged into the release source. The release workflow removes them after the published assets are re-downloaded and byte-verified.

## Carry-forward

Runner v0.7.16 equipment, target, policy-extension, editor, regression, and release missions remain OPEN in `missioncache.md`. They are not represented by hidden feature branches and must be implemented cache-first after v0.7.15 is published and eye-tested.
