# Runner v0.7.20 UI, preview continuity, and application identity

Runner v0.7.20 uses logical SDL window coordinates for application layout and input while Vulkan maps the same canvas into the drawable swapchain. This removes the high-DPI half-size interface seen on scaled Windows displays.

The world viewport and training PIP now use CPU-side rectangle clipping. Terrain cells, course markers, hazards, rigs, and particles cannot escape behind the side panel, through the panel gap, or outside their cards.

The live interface uses shared layout boxes for the top telemetry card, PIP, bottom controller card, world, and side panel. The required layouts are validated at 1280x820, 1600x900, 1920x1080, 2047x1112, and 2560x1440.

Immutable training publication no longer calls `set_course()` on every synchronization. The large preview keeps running through normal telemetry updates and newly retained champions; it resets only for a real rig/course change, explicit user reset, or terminal episode.

The executable is built from the canonical C++23 source files. The obsolete configure-time source patch generator was removed.

A deterministic high-contrast Runner icon generator produces transparent PNG artwork, an SDL BMP window icon, and a multi-resolution Windows ICO containing 16, 20, 24, 32, 40, 48, 64, 128, and 256 pixel entries. Windows embeds the ICO into `Runner.exe`, and packaged builds include all icon assets under `assets/ui`.
