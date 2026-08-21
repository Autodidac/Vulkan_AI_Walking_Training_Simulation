# Runner v0.7.40 physical-facing return

Runner v0.7.40 closes the packaged eye-test failure where a rig could face left while its motors and reconstructed legs still behaved in the old world-space frame. The turn now reflects the articulated physical plant about its root and keeps controller observations, joint angles, motor targets, topology reflexes, equipment aim, modular art, preview, and PIP in one facing-local contract.

## Root cause and correction

- The two-link leg reconstruction used a constant world-space knee-bend sign. After a left turn, constraint projection pushed both knees back toward their outbound branch, so signed leftward displacement could be produced by a backward brace instead of a forward return gait.
- Knee reconstruction now uses the current physical facing. All motor and observation direction channels use the same facing-local basis, while the explicit facing observation remains world-relative.
- Direction changes reflect particle positions about the root, clear stale velocity/contact/motor history, and reflect equipment aim. Rendering reads those physical positions directly; it no longer applies a second display-only mirror.
- Authored static terrain remains immutable. Only explicitly dropped granular overlay cells may fall, stack, settle, or be removed.

## Deterministic proof

- A paired 45-step reflected plant trace requires facing-local teacher actions and every particle pose to remain mirror-equivalent before the next boundary transition.
- An adversarial stale outbound residual must still return left by more than 1 m with real gait cycles, zero course translation, no invalid motion, and no sustained backward brace. The corrected focused trace travelled 8.73 m left with 9 gait cycles.
- The physical biped teacher completed five turns, 66.5 m, and 86 gait cycles without invalid motion; a repeated-seed humanoid completed five turns and 70.2 m with an identical fingerprint.
- Negative evidence explicitly distinguishes forward left-facing posture from a torso that braces opposite travel. Equipment and modular-art tests require the physical mount and skin to reflect exactly once.

## Persistence and release boundary

Training semantics are 0x0007'4001, checkpoint magic is EPPO40, autonomy state is RUNAUTONOMY 22, and automatic files use runner-v0740-physical-facing-*. v0.7.39 and older checkpoints can contribute validated lifetime totals only; incompatible controller, optimizer, champion, and mastery state start fresh. The local release contains both the runnable Windows package and an exact-commit source archive.
