# EpochRunner v0.7.2

- Reopens the simulation-quality missions after packaged runtime screenshots contradicted v0.7.1 validation.
- Adds coordinated bilateral joint synergies while preserving learned residual control.
- Replaces lower-leg endpoint feet with dedicated passive foot plates, heel contacts, and toe contacts.
- Restricts traction and foot support to explicit semantic foot nodes.
- Adds an obstacle-conditioned low-bar duck, clearance, pass, and return-to-stance lesson.
- Applies the same coordinated controller in PPO rollout, deterministic evaluation, rig evaluation, preview, and live execution.
- Rejects a previously qualified rollout when its current displayed frame is collapsed or unsupported.
- Bumps checkpoint and autonomy semantics so v0.7.1 behavior cannot silently resume.
- Makes source-tree run.bat prefer the current Release build instead of a stale root executable.
