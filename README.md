# malid3d

A Direct3D 11 translation layer for Arm Mali GPUs, written from scratch on top
of Vulkan. It targets Arm's stock Mali driver first and PanVK second. First
device: Mali-G615 MC2 through GameHub / BannerHub.

**Status: milestone 0 (probe).** This build does not render anything yet. It
replaces `d3d11.dll`, records what the Vulkan driver on your device can do, and
then tells the game D3D11 is unsupported. Its job is to prove the build and
install path work and to collect the data the renderer will be designed around.

## Download

Every push is built by GitHub Actions. Open the repository's **Releases** page
and download the newest `malid3d-xxxxxxx.zip`.

    malid3d/x64/d3d11.dll   for 64-bit games
    malid3d/x32/d3d11.dll   for 32-bit games

## Test (milestone 0)

1. Put the matching `d3d11.dll` where your launcher takes its DXVK `d3d11.dll`
   from (a custom DXVK component, or the game's own folder next to the `.exe`
   with a `d3d11=native` DLL override).
2. Start any Direct3D 11 game. It is expected to fail with a D3D11 error.
3. Find `malid3d.log` in the game's folder (next to the `.exe`). If it is not
   there, check the Windows temp folder inside the container.
4. Send the whole log back.
5. If you can, repeat with the other GPU driver selected (system driver and
   PanVK) and send both logs. Rename the first log before the second run.

Set the environment variable `MALID3D_LOG_PATH` to a folder to choose where the
log goes.

## Design and roadmap

See [docs/DESIGN.md](docs/DESIGN.md) and [docs/ROADMAP.md](docs/ROADMAP.md).
