# Repository cleanup after EpochRunner v0.6.5

- Open pull requests: `0`
- Remaining branches:
  - `main`
  - `feature/v070-runtime-pipeline`
- Deleted completed temporary branches:
  - `feature/mission-cache-completion`
  - `feature/self-imitation-mission-cache`
  - `diagnostics/action-status`
- Removed stale v0.6.1 release workflow from the retained v0.7 branch.
- Removed obsolete `tools/integrate_v061.py` from the retained v0.7 branch.
- Removed the temporary v0.7 merge-diagnostic workflow.
- Kept `feature/v070-runtime-pipeline` because it contains unfinished coroutine, worker-ownership, asynchronous persistence, and ThreadSanitizer mission work that must not be silently discarded.
- Historical closed pull requests remain as GitHub audit history; there are no active PRs to merge or close.
