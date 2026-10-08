> **This project was generated using AI.** Thank you to everyone behind the earlier Vita ports: your work provided essential references for the AI.

<h1 align="center">Starfront: Collision HD · PSVita Port</h1>

<p align="center">
  <a href="https://github.com/vctorwei/starfront-vita/releases/download/v0.0.6/Starfront-PSV-00.06.vpk"><strong>Download 00.06 VPK</strong></a> ·
  <a href="https://github.com/vctorwei/starfront-vita/releases/tag/v0.0.6">Release</a> ·
  <a href="#installation">How to install</a> ·
  <a href="#hardware-demo">Demo</a> ·
  <a href="#credits">Credits</a> ·
  <a href="README.zh-CN.md">简体中文</a>
</p>

![Starfront: Collision HD original in-game HD artwork](media/cover.png)

A PS Vita port of Gameloft's **Starfront: Collision HD**. Requires the **Android 1.0.0 APK and matching 1.0.0 OBB game data**. Other versions are unsupported.

**00.06 is a development preview.** Full campaign progression and long-term stability are still being tested. The LiveArea background may retain its previous appearance.

## Disclaimer

Starfront: Collision HD © 2011 Gameloft. The game, artwork, names and trademarks belong to their respective owners. This is an unofficial community project, neither produced nor endorsed by Gameloft or Sony.

The original APK, OBB and native game executable are not included. You must supply your own legally obtained copy. The authors do not support or encourage piracy.

## Installation

1. Install [kubridge](https://github.com/bythos14/kubridge). Place `kubridge.skprx` in `ur0:tai/`, add the following entry to `ur0:tai/config.txt`, then reboot:

   ```text
   *KERNEL
   ur0:tai/kubridge.skprx
   ```

2. Make sure `libshacccg.suprx` is in `ur0:data/`. Use [ShaRKBR33D](https://github.com/Rinnegatamante/ShaRKBR33D) if it is missing.
3. Obtain your **Starfront: Collision HD Android 1.0.0 APK and corresponding OBB**. Open the APK with [7-Zip](https://www.7-zip.org/) or another archive tool. Extract `lib/armeabi-v7a/libstarfront.so` to `ux0:data/starfront/libstarfront.so`.
4. Unpack the OBB game data and copy the complete `GloftSFHP` folder to `ux0:data/starfront/GloftSFHP/`. Copy the APK's `assets/data.save` to both `ux0:data/starfront/GloftSFHP/data.save` and `ux0:data/starfront/save/files/data.save`. **Do not overwrite existing saves when updating.**
5. Download the [VPK](https://github.com/vctorwei/starfront-vita/releases/tag/v0.0.6). Open it with your archive tool and copy its `compat/GloftSFHP` contents into `ux0:data/starfront/GloftSFHP/`.
6. Install the VPK with [VitaShell](https://github.com/TheOfficialFloW/VitaShell), launch **Starfront Test**, and press **×** after the startup checks pass.

**Updating an existing test installation:** keep your game data and saves; overwrite-install the VPK.

## Controls

Use the front touchscreen, including two-finger box selection. **START** goes back; **SELECT + START** exits. Full button controls are not yet implemented.

## Hardware demo

A 16-second recording on a real PS Vita. Click the silent preview for the full video with audio.

[![PSV touchscreen gameplay demo](media/demo.gif)](media/demo.mp4)

[Download MP4](https://raw.githubusercontent.com/vctorwei/starfront-vita/main/media/demo.mp4)

## Credits

- **Gameloft** — the original game and in-game HD artwork.
- [vctorwei](https://github.com/vctorwei) — project and hardware testing.
- [TheFloW](https://github.com/TheOfficialFloW) — Android SO loader and [GTA SA Vita](https://github.com/TheOfficialFloW/gtasa_vita).
- [Volodymyr Atamanenko](https://github.com/v-atamanenko) — [Backstab Vita](https://github.com/v-atamanenko/backstab-vita) and [Modern Combat 3 Vita](https://github.com/v-atamanenko/mc3-vita).
- [WolffsRoom](https://github.com/WolffsRoom) and contributors — [Modern Combat 2 Vita](https://github.com/WolffsRoom/MC2BPegasus-Vita).
- [Rinnegatamante](https://github.com/Rinnegatamante) — vitaGL, vitaShaRK and Vita porting work.
- [CatoTheYounger](https://github.com/CatoTheYounger97), [Once13One](https://github.com/once13one), [GrapheneCt](https://github.com/GrapheneCt), and everyone who contributed to the reference ports.
- **VitaSDK, kubridge, VitaShell, Vita3K, PSPSDK, AOSP / Apache Harmony, pthreads, math-neon, zlib and PolarSSL** developers and contributors.

## License

Original project contributions use the [MIT License](LICENSE.md). Third-party components and game assets retain their own terms.

[Third-party notices](THIRD_PARTY.md) · [Report a bug](https://github.com/vctorwei/starfront-vita/issues)
