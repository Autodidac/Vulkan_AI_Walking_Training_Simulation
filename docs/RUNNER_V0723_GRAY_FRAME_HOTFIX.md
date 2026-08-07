# Runner v0.7.23 true rounded-outline rendering hotfix

## Observed package failure

Runner v0.7.22 replaced an opaque black inset with a zero-alpha inset, but the helper still drew the complete outer card using the outline color first. Transparent blending cannot erase that prior write. On the sRGB Windows swapchain, the linear border color appeared as the uniform gray shown in the packaged eye test.

## Rendering contract

A filled rounded rectangle now renders its fill once and then adds an independent perimeter ring. A border-only rounded rectangle emits only four bounded straight strips and four tessellated quarter-ring corners. No transparent primitive is treated as an eraser, and no border path writes center geometry.

## Regression contract

The deterministic test renders a known colored background, overlays a border-only rounded rectangle, and samples the final composite. The center must remain exactly the background color and the perimeter must contain the outline. The complete Live world, dashboard, PIP, Rig Lab viewport, and all four Rig Lab pages must also retain multiple final visible colors after the entire draw order.

## Compatibility

This release changes presentation only. Training semantics remain `0x0007'2101`; policy dimensions, fixed rig anatomy, gait qualification, terrain, checkpoints, and `runner-v0721-*` autosaves are unchanged.
