# Runner v0.7.45 zombie-gait baseline

This directory preserves the exact minimum viable Human locomotion baseline that shipped locally as Runner v0.7.45. It is evidence and a regression reference, not a production runtime fallback.

- Tag: `v0.7.45`
- Commit: `37ab59a2b3524fbb2681e7f8aecca7ccb1c3c1d9`
- Training semantics: `0x00074501`
- Generator: the `RunnerRigDefaults` executable built from that exact commit
- Human rig SHA-256: `6C07910184467CA6D0D5EF14DD540E8D5BEEA20285ACFA58D970114F396E3529`

The baseline walks and turns repeatably, but packaged eye tests show a bent-knee zombie shuffle: the support leg stays flexed, feet do not complete a credible heel-to-toe transfer, the trunk leans back, and the arms project forward. It also trips at the visual water boundary. Those defects are intentionally documented here because the working locomotion minimum is valuable and must remain reproducible while the production gait advances.

User eye-test evidence was captured in screenshots `2026-08-23 054334`, `154731`, `154737`, `154744`, `155004`, and `155009`. The exact teacher/controller implementation remains recoverable from the tagged commit; the four generated rig files preserve the authored morphology inputs.

To reproduce from an exact checkout of the commit above, build the Windows test configuration and run `RunnerRigDefaults.exe` with an output directory. Compare the emitted files against `SHA256SUMS.txt` before using the result as baseline evidence.
