# EpochRunner v0.7.1

- Fixes humanoid shoulder and elbow motors pinning the parent chest/body.
- Adds reciprocal rotational-inertia-weighted motor reaction with center-of-mass preservation.
- Requires sustained stance and controlled recovery evidence before a policy is valid.
- Selects best policies, rollback anchors, rig candidates, imitation trajectories, and PIP samples lexicographically by stage-valid evidence before reward.
- Rejects collapsed, unsupported, body-contact, violent-joint, motionless, skating, rolling, hovering, and prerequisite-incomplete candidates.
- Adds a strong neutral-action standing bootstrap that decays as the policy learns.
- Invalidates v0.7.0 checkpoints and autosaves with a new training-semantics signature and v0.7.1 paths.
- Adds bounded deterministic training, adversarial collapsed-pose, reciprocal-motor, and cross-rig regression coverage.
- Expands humanoid observations from 32 to 40 channels so all eight motor angles and velocities are independent.
- Uses the same effective balance controller in rollout collection, deterministic evaluation, self-imitation, live preview, and displayed execution.
- Latches completed standing lessons and requires at least four of six deterministic perturbed starts to pass.
- Adds bounded stance-evidence hysteresis so brief solver contact transitions do not erase a valid sustained stand.
- Fixes startup from Visual Studio, shortcuts, extracted release folders, and unrelated working directories by resolving shaders and assets beside the executable.
- Restores `run.bat` as the supported one-click source-tree and extracted-release launcher.
- Packages and validates `run.bat`, shaders, assets, and runtime DLLs from an unrelated working directory.
