> **This project was generated using AI.** Thank you to everyone behind the earlier Vita ports: your work provided essential references for the AI.

# Starfront: Collision HD · PS Vita Loader

[**Download VPK**](https://github.com/vctorwei/starfront-vita/releases/tag/v0.0.6-r3) · [Installation](#installation) · [Demo](#hardware-demo) · [Credits](#credits) · [简体中文](README.zh-CN.md)

[![Starfront promotional screenshot published by Gameloft](https://pbs.twimg.com/media/DWP4997W4AEr0pW.jpg)](https://x.com/gameloft/status/964888517984837637)

*Original-game promotional screenshot from [Gameloft's post](https://x.com/gameloft/status/964888517984837637), © Gameloft. Externally hosted; not a screenshot of this Vita build.*

This repository contains a loader for the **Android 1.0.0 release of Starfront: Collision HD**, based on TheFloW's Android SO loader. It supplies the Android compatibility functions needed to run the original ARMv7 executable on PS Vita. You need your own **1.0.0 APK and matching OBB/data**.

**00.06-r3 is a development preview based on 00.06.** It updates the LiveArea with a Starfront-style “New Mission” launch card and corrected installation metadata. Normal PSV startup is now silent; errors still show a diagnostic screen. Full campaign stability remains unverified.

## Disclaimer

Starfront: Collision HD © 2011 Gameloft. The game and all associated names, artwork and trademarks belong to their respective owners. This is an unofficial project, not produced, authorized or endorsed by Gameloft or Sony.

The loader includes game artwork for the Vita system UI, credited to Gameloft. It does not include the original game executable, APK, OBB or playable game data. Players must supply their own legally obtained copy. The authors do not support or encourage piracy. System artwork, screenshots and the gameplay demonstration retain the game owner's rights and are not covered by the software license.

## Installation

1. Install [kubridge](https://github.com/bythos14/kubridge): copy `kubridge.skprx` to `ur0:tai/`, add it under `*KERNEL` in `ur0:tai/config.txt`, then reboot:

   ```text
   *KERNEL
   ur0:tai/kubridge.skprx
   ```

2. Make sure `libshacccg.suprx` is in `ur0:data/`. See [ShaRKBR33D](https://github.com/Rinnegatamante/ShaRKBR33D) if it is missing.
3. Download and extract the release's **Source code (zip)**. Install [Python 3.9+](https://www.python.org/downloads/). In the extracted folder, run this with your own Android 1.0.0 files:

   ```sh
   python3 scripts/prepare_game.py --apk "Starfront.apk" --data "GloftSFHP" --output "prepared/starfront"
   ```

   On Windows, use `py -3` instead of `python3`. `--data` accepts the unpacked `GloftSFHP` folder or a ZIP-compatible OBB/data archive. The script checks the APK and prepares everything locally; it downloads no game files.
4. Copy the resulting `starfront` folder to `ux0:data/starfront/`. **Keep existing saves when updating.** Merge the prepared files without replacing your existing `data.save` files.
5. Install the [VPK](https://github.com/vctorwei/starfront-vita/releases/tag/v0.0.6-r3) with [VitaShell](https://github.com/TheOfficialFloW/VitaShell), open **Starfront Test**. The game starts automatically after the startup checks pass.

**Updating from 00.06:** run preparation once to obtain the newly required `apk/igli.bin` and `apk/serialkey.txt`, copy them alongside the existing data, then install the new VPK. Resolution remains 1024×600 scaled to 960×544; file logging is disabled.

**Updating from r1/r2:** install the VPK; no new game data is needed. If the default LiveArea remains, restart the console. If necessary, delete only the Starfront bubble and reinstall, keeping `ux0:data/starfront/` and your saves.

## Controls

Use the front touchscreen, including two-finger box selection. **START** goes back; **SELECT + START** exits. Full button controls are not yet implemented.

## Hardware demo

Maintainer-recorded footage of the earlier 00.06 build on PS Vita, not a demonstration of the new LiveArea. Click for the video with audio.

[![PSV touchscreen gameplay demo](media/demo.gif)](media/demo.mp4)

## Credits

- **Gameloft** — the original game.
- [vctorwei](https://github.com/vctorwei) — project and hardware testing.
- [TheFloW / Andy Nguyen](https://github.com/TheOfficialFloW) — Android SO loader and [GTA SA Vita](https://github.com/TheOfficialFloW/gtasa_vita).
- [Volodymyr Atamanenko](https://github.com/v-atamanenko) — [Backstab Vita](https://github.com/v-atamanenko/backstab-vita) and [Modern Combat 3 Vita](https://github.com/v-atamanenko/mc3-vita).
- [WolffsRoom](https://github.com/WolffsRoom) — [Modern Combat 2 Vita](https://github.com/WolffsRoom/MC2BPegasus-Vita).
- [Rocroverss](https://github.com/Rocroverss) and contributors — [Gun Bros PSVita](https://github.com/Rocroverss/Gun-Bros-Psvita), an additional Vita LiveArea packaging reference.
- [Rinnegatamante](https://github.com/Rinnegatamante) — vitaGL, vitaShaRK and Vita porting work.
- [CatoTheYounger](https://github.com/CatoTheYounger97), [Once13One](https://github.com/once13one), [GrapheneCt](https://github.com/GrapheneCt), and all contributors to the reference ports.
- **VitaSDK, SceShaccCgExt, kubridge, VitaShell, Vita3K, PSPSDK, AOSP / Apache Harmony, pthreads, math-neon, zlib and PolarSSL** developers.

## License

Original contributions: **MIT**. The combined loader binary includes GPL/LGPL dependencies and is distributed under **GPLv3**. See [license scope](LICENSE.md), [component notices](THIRD_PARTY.md) and [build instructions](docs/BUILD.md).
