# Runner + SandHybrid integration bridge

This file prevents mission loss while Runner becomes the live training application for the SandHybrid simulation library.

## Pinned library source

- Product: EpochSimEngine with SandHybrid as its bundled example/runtime
- Public mirror: `https://epoch.adamrushford.chatgpt.site/git/simengine.git`
- Library target: `EpochSimEngine::EpochSimEngine`
- Compatibility target: `SandHybrid::SandHybrid`
- Pinned source commit: `14f55d30df3e406215facf069c8b50082d7439eb`
- Upstream release represented by that commit: EpochSimEngine `2.5.26`
- Required public library API: `4`
- Upstream canonical ledger: `EpochSimEngine/missioncache.md`
- Runner canonical ledger: `Vulkan_AI_Walking_Training_Simulation/missioncache.md`

The 640x360 authored area is one footprint inside a connected persistent world,
not the resident-world extent. Empty is vacuum/open space; breathable air is
`Material::atmosphere`. The complete four-word, 16-byte `SceneCell` is canonical.

The two ledgers remain authoritative for their own products. Integration does not copy an upstream `OPEN`, `PARTIAL`, `REGRESSION`, or `DEFERRED` item into history, rename it away, or mark it complete. Updating the pin requires reviewing the complete upstream mission cache and updating this bridge in the same commit.

## Ownership

### SandHybrid-owned

SandHybrid continues to own material IDs and profiles, packed atmosphere contracts, 8×8 cell/tile semantics, 64×64 sparse-section scheduling, section dirty rectangles, packet transactions, terrain generation, world layout, scene images, actor/medium contracts, inventory, machinery transactions, and every unresolved mission in its canonical ledger.

Epoch2DWalkEngine does not fork those contracts. It links the complete platform-neutral EpochSimEngine library target and adapts its live training world to them. SandHybrid's SDL/Vulkan demo/runtime remains outside the reusable library boundary.

### Runner-owned

Runner continues to own rig anatomy, joint and toe control, PPO/autonomy training, curriculum order, live preview, trainer UI, locomotion rewards and invalid-motion gates, Windows packaging, and every unresolved mission in the Runner mission cache.

Runner additionally owns the adapter that converts SandHybrid cells and derived macro metadata into collision surfaces and renderer batches for live locomotion training.

## Integration requirements that must remain visible

- Toe command slew and physical hinge-rate limits from v0.7.13 remain required and are folded into v0.7.14.
- Every preset must retain the validated Stand and static Crouch gates.
- The primary humanoid scale must remain approximately 3–5 SandHybrid macro tiles tall.
- One macro tile is exactly 8×8 fine cells. Full uniform tiles promote immediately to derived macro metadata; any changed or partial cell demotes immediately to fine representation. Canonical cells remain authoritative.
- Sand, soil, silt, mud, stone, and ore identity comes from SandHybrid material contracts.
- Sand and other granular surfaces may form irregular blob/pixel edges. Structural material may create a true vertical face or hard 90-degree ledge without forcing the surrounding granular surface onto a square staircase.
- Runner training, preview collision, material impacts, burial, and terrain rendering must consume the same live map state.
- Reachable hard-wall ledges remain an explicit future curriculum requirement: climb without jumping when a hand can reach the ledge, and turn backward for a controlled descent when the drop is no greater than standing body height.
- Fixed physics advances at 60 Hz. Render cadence may not change material, actor,
  terrain, curriculum, or policy time, and stale debt is never a catch-up loop.
- Static 8x8 macro tiles are reversible scheduling metadata over canonical cells.
  Only actual cell operations move material; tiles never become a parallel map.

## Release gate

A combined release cannot publish until all of the following pass from the exact package source:

1. Exact EpochSimEngine commit `14f55d30df3e406215facf069c8b50082d7439eb` configures and links with Epoch2DWalkEngine on Linux and Windows.
2. The linked library reports API exactly `4`, a 16-byte `SceneCell`, all 68 append-only materials, `atmosphere=66`, and the expected platform-neutral capabilities.
3. Fine-cell volume is conserved under pressure, deposit, and granular settling.
4. 8×8 macro promotion and single-cell demotion are immediate and deterministic.
5. The primary humanoid height is between 3 and 5 macro tiles.
6. Pixel/blob terrain includes irregular fine boundaries and at least one deterministic structural 90-degree ledge.
7. Live preview draws the same cells used by collision and training.
8. Existing Runner core, terrain, concurrency, runtime, package, all-rig Stand, and all-rig Crouch acceptance remains green.
9. Windows and Linux Release builds pass.
10. Both canonical ledgers and this bridge remain in the package; no unresolved mission is deleted or silently reclassified.
