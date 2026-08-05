# Runner agent instructions

## Authoritative workflow

1. Read and update `missioncache.md` before changing source.
2. Preserve every unfinished mission explicitly. Never hide work by renaming, deleting, or moving it only into chat.
3. Inventory interactions across physics, rigs, contacts, terrain, observations, curriculum, policy dimensions, persistence, editor, renderer, diagnostics, tests, packaging, branches, and release assets.
4. Prefer one coherent implementation over visible-symptom patches.
5. Add deterministic positive, negative, adversarial, and repeated-seed coverage for every behavior change.
6. Re-read the diff and mission cache after tests. Record newly discovered consequences instead of waiving them.

## Required validation

For release work, run:

- repository hygiene and `git diff --check`;
- Linux GCC 14 warnings-as-errors and all CTest suites;
- the complete Windows SDL3/Vulkan build and all tests;
- `Runner.exe --diagnose-package`;
- `Runner.exe --diagnose-acceptance`;
- every feature-specific diagnostic, including `--diagnose-camera`;
- installed and independently extracted `run.bat` from an unrelated working directory;
- ZIP checksum and per-file manifest audit;
- published-asset re-download and byte comparison.

A compile is not completion. Visible packaged-runtime evidence can reopen an automated pass.

## Documentation obligations

Every release updates, as applicable:

- `missioncache.md`;
- `CHANGELOG.md`;
- `README.md`;
- a focused document under `docs/`;
- CMake/package install lists;
- release workflow version, package name, required files, notes, and cleanup;
- this file when repository process changes.

Do not create `RELEASE_NOTES*.md` or additional mission-ledger documents.

## Branch and release hygiene

- Use `agent/<release-or-scope>` branches.
- Temporary applicators, trigger files, and one-use workflows must delete themselves before the final PR.
- Do not merge marker-only or obsolete observer branches.
- Final publication requires zero open cleanup PRs and only `main` unless a documented next-release branch is intentionally retained.
- Never overwrite an existing release tag.
- Tag only audited source and verify every uploaded asset after publication.

## User eye testing

Screenshots and direct observations outrank claims inferred from metrics. Incorrect scale, zoom, posture, gait, feet, terrain synchronization, PIP framing, or UI readability reopens the exact matching mission.
