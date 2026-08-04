# Runner + SandHybrid mission bridge

This file prevents mission loss while Runner becomes the live training application for the SandHybrid simulation library.

## Pinned library source

- Repository: `Autodidac/EpochSimEngine`
- Library target: `SandHybrid::SandHybrid`
- Pinned source commit: `99dd8acddfa9be1402981052b39cbf6284ed99ae`
- Upstream release represented by that commit: SandHybrid `2.5.9`
- Upstream canonical ledger: `EpochSimEngine/missioncache.md`
- Runner canonical ledger: `Vulkan_AI_Walking_Training_Simulation/missioncache.md`

The two ledgers remain authoritative for their own products. Integration does not copy an upstream `OPEN`, `PARTIAL`, `REGRESSION`, or `DEFERRED` item into history, rename it away, or mark it complete. Updating the pin requires reviewing the complete upstream mission cache and updating this bridge in the same commit.

## Ownership

### SandHybrid-owned

SandHybrid continues to own material IDs and profiles, packed atmosphere contracts, 8×8 cell/tile semantics, 64×64 sparse-section scheduling, section dirty rectangles, packet transactions, terrain generation, world layout, scene images, actor/medium contracts, inventory, machinery transactions, and every unresolved mission in its canonical ledger.

Runner does not fork those contracts. Runner links the complete platform-neutral library target and adapts its live training world to them.

### Runner-owned

Runner continues to own rig anatomy, joint and toe control, PPO/autonomy training, curriculum order, live preview, trainer UI, locomotion rewards and invalid-motion gates, Windows packaging, and every unresolved mission in the Runner mission cache.

Runner additionally owns the adapter that converts SandHybrid cells and derived macro metadata into collision surfaces and renderer batches for live locomotion training.

## Integration missions that must remain visible

- Toe command slew and physical hinge-rate limits from v0.7.13 remain required and are folded into v0.7.14.
- Every preset must retain the validated Stand and static Crouch gates.
- The primary humanoid scale must remain approximately 3–5 SandHybrid macro tiles tall.
- One macro tile is exactly 8×8 fine cells. Full uniform tiles promote immediately to derived macro metadata; any changed or partial cell demotes immediately to fine representation. Canonical cells remain authoritative.
- Sand, soil, silt, mud, stone, and ore identity comes from SandHybrid material contracts.
- Sand and other granular surfaces may form irregular blob/pixel edges. Structural material may create a true vertical face or hard 90-degree ledge without forcing the surrounding granular surface onto a square staircase.
- Runner training, preview collision, material impacts, burial, and terrain rendering must consume the same live map state.
- Reachable hard-wall ledges remain an explicit future curriculum mission: climb without jumping when a hand can reach the ledge, and turn backward for a controlled descent when the drop is no greater than standing body height.

## Release gate

A combined release cannot publish until all of the following pass from the exact package source:

1. The pinned SandHybrid core configures and links with RunnerCore on Linux and Windows.
2. SandHybrid's linked library reports its expected API/name capabilities.
3. Fine-cell volume is conserved under pressure, deposit, and granular settling.
4. 8×8 macro promotion and single-cell demotion are immediate and deterministic.
5. The primary humanoid height is between 3 and 5 macro tiles.
6. Pixel/blob terrain includes irregular fine boundaries and at least one deterministic structural 90-degree ledge.
7. Live preview draws the same cells used by collision and training.
8. Existing Runner core, terrain, concurrency, runtime, package, all-rig Stand, and all-rig Crouch acceptance remains green.
9. Windows and Linux Release builds pass.
10. Both canonical mission ledgers and this bridge remain in the package; no unresolved mission is deleted or silently reclassified.
