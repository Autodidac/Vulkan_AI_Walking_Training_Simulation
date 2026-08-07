# Runner v0.7.19 general locomotion

Runner v0.7.19 changes the locomotion target from forward-speed optimization to reusable game-AI movement control.

## Control hierarchy

The shared `runner::locomotion` strategy consumes only physical/gameplay signals: torso uprightness, semantic support positions, local terrain height deltas and slope, current speed, recovery state, obstruction/burial, free-space direction, incoming threat velocity/time/density, and established gait cycles. It does not depend on SandHybrid material IDs or renderer state.

The strategy produces a small locomotion plan: hold, walk, run, recover, emergency crawl, or flee; signed travel direction; target speed; cadence; stride scale; swing lift; stance extension; balance reserve; terrain demand; braking state; and step-up intent. PPO still owns the final policy. The strategy is a decaying bootstrap and reward target, not a scripted controller.

## Balance reserve

Balance reserve combines torso uprightness, one-foot/two-foot support quality, and root position relative to the active support interval. Speed is useful only while reserve remains available. A controller is allowed to slow, take a corrective step, or briefly stop when terrain demand increases or reserve falls. Recovery progress is not treated as zero-motion failure.

## Plateaus and step-ups

Reachable positive height changes trigger a deliberate step-up plan: reduce speed, increase swing-foot clearance, load the stance chain, extend the stance leg, move the root over support, and resume normal cadence on top. Structural ledges remain real collision terrain but the authored training ledges are kept inside reachable ranges and vary deterministically by seed/difficulty.

## Walk before run

Running is gated behind established gait, adequate balance reserve, and low terrain demand. Clear terrain may increase cadence and target speed. Approaching a ledge, steep transition, hazard, or depleted reserve lowers target speed and rewards braking rather than raw velocity.

## Reversal and flee

Mixed hazard training uses a signed travel intent. Urgent incoming threats choose a free escape direction when one exists; otherwise the controller moves opposite the incoming horizontal velocity. A reversal must brake, transfer support, and produce real opposite-direction gait. Being thrown backward is not movement skill.

## Emergency crawling

Crawling is a survival fallback only. It is eligible only when the rig is already non-upright, obstructed or buried, balance reserve is poor, and a measurable free-space direction exists. Crawl motion gets no upright gait credit and cannot qualify a Walk/Run champion. Once clear, recovery-to-stand is preferred.

## Terrain transfer

The same policy inputs cover flat ground, rough ground, soft/firm deformable terrain, ramps, reachable ledges, plateaus, step-downs, falling/deposited sand, moving hazards, and thrown objects. SandHybrid supplies one physics environment; the locomotion contract is material-independent and suitable for another game runtime that can provide equivalent physical observations.

## Preview contract

The large Live preview displays the best validated policy when a champion exists instead of the actively-mutating exploratory policy. Failed preview episodes restart with varied deterministic seeds, preventing a single bad seed/plateau from making the visible preview look permanently limited to the same two steps while background training has already produced better behavior.
