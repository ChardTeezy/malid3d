# Design

## Vision

Build a Direct3D translation layer for Mali from the ground up instead of
adapting DXVK. DXVK assumes desktop Vulkan features that Mali drivers often
lack (logic ops, dual-source blending, and others) and structures rendering
for desktop GPUs. malid3d starts from the features Mali drivers do expose and
makes the Mali-friendly path the main path, not a fallback.

## Decision: Vulkan is the boundary

    Game -> Direct3D 11 -> malid3d -> Vulkan -> Mali driver -> kernel -> GPU

malid3d translates Direct3D 11 to Vulkan and does not talk to the GPU itself.
Skipping Vulkan (a D3D11 driver built straight on driver internals, as Mesa's
Gallium Nine did for D3D9) was considered and rejected: far more work for a
small gain.

## Target drivers

1. **Primary: Arm's stock Mali driver.** It ships on every Mali phone and is
   the most mature. Its bugs cannot be fixed, only worked around, so errata
   handling is part of the design. Only features this driver exposes may be
   *required*.
2. **Secondary: PanVK** (Mesa, including the kbase port used by GameHub).
   Less stable and not available for every Mali GPU, but open source and
   richer in extensions (for example built-in BC texture emulation). Extra
   features it exposes are used as optional fast paths, never required.

First device: Mali-G615 MC2 (Valhall v11), MediaTek MT6878, Android 16,
running games through GameHub / BannerHub (Proton arm64x, FEX).

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
| BC texture formats | Decode or transcode on upload; use the driver's own BC support when it has it |
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
