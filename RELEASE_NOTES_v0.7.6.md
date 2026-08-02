# Runner v0.7.6

- Fixes the impossible standing-mastery loop: evaluation now reaches the same six-second target required by strict mastery.
- Requires six-of-six seeded strict standing results before one of eight mastery confirmations is counted.
- Rejects arms-overhead standing, uncontrolled standing rotation, non-foot contact, violent joints, and short stance results.
- Raises the humanoid central shoulder/chest pivot above both lateral shoulder pivots and restores hanging neutral arm geometry.
- Keeps the training PIP populated with the best current finite training environment, including rejected attempts and exact failure reasons.
- Shows standing target time, valid evaluation seeds, spin threshold, and upper-body angle directly in the UI.
- Invalidates v0.7.5 standing checkpoints and autosaves so the accepted arms-up/spinning controller cannot resume as progress.
- Carries deformable sand terrain and falling-material/burial recovery missions forward unchanged.
