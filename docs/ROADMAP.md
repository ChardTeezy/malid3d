# Roadmap

Each milestone ends with something testable on the phone.

0. **Probe** (current). `d3d11.dll` loads, logs the Vulkan driver's features,
   limits, formats and extensions. Confirms build + install path.
1. **Device and swapchain.** Real `ID3D11Device` / `ID3D11DeviceContext` /
   DXGI swapchain objects; a game gets a window cleared to a solid colour.
2. **First triangle.** Buffers, input layouts, DXBC to SPIR-V for simple
   vertex and pixel shaders, draw calls.
3. **Textures and state.** Texture upload (with BC transcoding if the GPU
   lacks BC formats), samplers, blend / depth / rasterizer state.
4. **First real game.** One simple D3D11 title rendering correctly.
5. **Mali tuning.** Render-pass merging so tiles are not flushed needlessly,
   load/store-op selection, avoiding mid-frame readbacks.
6. **Later.** Compute shaders, geometry-shader and tessellation fallbacks,
   more feature coverage game by game.

Design decisions are driven by the milestone 0 log from the target device.
