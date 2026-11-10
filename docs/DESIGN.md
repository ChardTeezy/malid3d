# Design

## Decision: Vulkan is the boundary

    Game -> Direct3D 11 -> malid3d -> Vulkan -> PanVK -> kernel (kbase) -> Mali GPU

malid3d translates Direct3D 11 to Vulkan and does not talk to the GPU itself.
Mali-specific work happens on both sides of that boundary:

- **Above it (this repo):** how D3D11 is mapped to Vulkan so a tile-based GPU
  does as little wasted work as possible.
- **Below it (the driver):** the target driver is an open-source Mesa PanVK
  build, so a missing feature or a bug can be fixed in the driver instead of
  being worked around here.

Skipping Vulkan (a D3D11 driver built straight on the driver internals, as
Mesa's Gallium Nine did for D3D9) was considered and rejected: far more work,
tied to Mesa's internal code, for a small gain.

## Target

- GPU: Mali-G615 MC2 (Valhall v11), MediaTek MT6878, Android 16.
- Runtime: GameHub / BannerHub, Proton arm64x, FEX.
- Vulkan driver: `mali-panvk-27.0.0-devel` package r10/v11, PanVK from Mesa
  27.0.0-devel on the stock Arm kbase kernel driver (`/dev/mali0`), reporting
  Vulkan 1.3.

## What we know about the hardware

**Unverified.** These come from a text report whose origin is unconfirmed,
describing the stock Arm driver (r44p1, Vulkan 1.3.247). Treat every line
below as a hypothesis until the milestone 0 probe log from the target phone
confirms or replaces it.

Works in hardware: geometry shaders, tessellation, compute, 4x MSAA,
transform feedback (single stream), instancing, indirect draws, dynamic
rendering, synchronization2, descriptor indexing, `VK_EXT_shader_tile_image`,
`VK_EXT_rasterization_order_attachment_access`, custom border colours.
Subgroup size is 16. One 11 GiB memory heap shared with the system.

Reported missing on the stock driver; each needs a plan if the probe agrees:

| Missing | Plan |
|---|---|
| BC texture formats | Use the driver's BC emulation if PanVK exposes it; otherwise decode on upload |
| Dual-source blending, logic ops | Read the framebuffer through tile-image access and blend in the shader |
| Wireframe fill mode | Emit lines from a rewritten index buffer or geometry stage |
| Multiple viewports | Only viewport 0 at first |
| Clip / cull distances | Emulate in the shader (discard / degenerate primitives) |
| Null descriptors (robustness2) | Bind small dummy resources |
| Depth-clip disable | Use depth clamp where the result matches |
| Pipeline statistics queries | Report zeros |
| Graphics pipeline libraries | Compile pipelines on worker threads, cache to disk |

Direct3D feature level to report first: 11_0.

## Principles for a tile-based GPU

1. One Vulkan render pass per run of draws to the same targets; never end a
   pass just because state changed.
2. Choose load/store ops deliberately: clear or don't-care instead of load
   whenever D3D11 semantics allow, and don't store what is never read again.
3. Resolve MSAA inside the pass, not with a separate copy.
4. Avoid mid-frame readbacks and copies from an attachment that is still
   being drawn to; they force a tile flush.
5. Prefer framebuffer fetch (tile image) over ping-ponging render targets.
6. Keep the number of shader cores in mind: MC2 has two. Cheap shaders and
   fewer passes matter more than draw-call throughput.
