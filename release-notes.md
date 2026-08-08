Runner v0.7.25 is the compact-art, stance-integrity, EpochGui-font-sync, and readable-progress release.

- Keeps the approved helmet and foot artwork while replacing the oversized torso/shoulder overlay with compact node-attached geometry.
- Prevents paired walking legs from telescoping by enforcing authored two-link geometry through recovery and contact frames.
- Synchronizes Runner to EpochGui's published logical-pixel font contract without modifying EpochGui.
- Adds the missing percent glyph and turns overflowing compact counters into READY states.
- Separates sample completion from mastery so zero mastery cannot present as complete.
- Removes the tracked validated-artifact staging directory and temporary migration/finalizer files.
- Package is reused byte-for-byte from workflow 31249578242, whose Linux and complete Windows SDL3/Vulkan build, tests, runtime diagnostics, install, independent extraction, archive checksum, manifest, and artifact upload all passed. Runtime/product files at merged main are byte-identical to that validated source.
