<h1 align="center">Starfront: Collision HD<br>PlayStation Vita Port</h1>

<p align="center">
  <a href="https://github.com/vctorwei/starfront-vita/releases/tag/v0.0.6">Download VPK</a> ·
  <a href="#installation">Installation</a> ·
  <a href="#controls">Controls</a> ·
  <a href="#status-and-known-issues">Known issues</a> ·
  <a href="#credits">Credits</a> ·
  <a href="README.zh-CN.md">简体中文</a>
</p>

An experimental PS Vita port of **Starfront: Collision HD**, Gameloft's real-time strategy game. The loader runs the original Android ARMv7 game library through a compatibility layer for Vita.

**Development preview · 00.06 · Title ID `SFHP00001`**

The campaign's first mission has been reached in an isolated Vita3K test environment. Complete campaign progression and long-term PSV hardware stability have not been verified. This release is a **prerelease**, not a finished port.

## Downloads

| Package | Purpose |
| --- | --- |
| [Starfront-PSV-00.06.vpk](https://github.com/vctorwei/starfront-vita/releases/download/v0.0.6/Starfront-PSV-00.06.vpk) | PSV hardware development build; file logging disabled |

Release downloads contain **VPK files only**. No APK, OBB, original `libstarfront.so`, full game-data archive, saved games, or private test logs are uploaded. Supply your own Android game library and data.

## Installation

**Already running the previous test build?** Keep your existing game data and saves and overwrite-install the 00.06 VPK in VitaShell. This restores the previous startup path; it does not include the withdrawn automatic LiveArea updater.

For a new installation:

1. Use a homebrew-enabled PSV with [VitaShell](https://github.com/TheOfficialFloW/VitaShell). Install [kubridge](https://github.com/bythos14/kubridge) as a kernel plugin. A typical taiHEN configuration is:

   ```text
   *KERNEL
   ur0:tai/kubridge.skprx
   ```

   Reboot after changing kernel plugins. The port also needs `libshacccg.suprx` at `ur0:data/libshacccg.suprx` or `ur0:data/external/libshacccg.suprx`; see [ShaRKBR33D](https://github.com/Rinnegatamante/ShaRKBR33D).

2. Obtain your own copy of **Starfront: Collision HD for Android** and its matching game data. This build was prepared for the tested **1.0.0** native library. Extract `lib/armeabi-v7a/libstarfront.so` from your APK to `ux0:data/starfront/libstarfront.so`. Its SHA-256 must be:

   ```text
   a362b3b46cacad41d3d6c2961b5e4c1dce0027dd4a3c027471652665c275a618
   ```

   The expected library size is **8,733,105 bytes**. Other native-library builds are unsupported by this preview.

3. Copy the unpacked `GloftSFHP` data directory to `ux0:data/starfront/GloftSFHP/` (approximately **1.02 GiB**). The loader expects the extracted files, including `effects.gla`, `entities.gla`, `sounds.gla`, and `sprites_1024.gla`; dropping an APK or an OBB into that directory does not unpack it. For a fresh installation, extract the APK's `assets/data.save` to both `GloftSFHP/data.save` and `save/files/data.save` under `ux0:data/starfront/`. **Keep existing saves when updating.**

4. On first installation, open the downloaded VPK with a ZIP-capable archive program and merge its `compat/GloftSFHP` folder into `ux0:data/starfront/GloftSFHP/`. This supplies two small terrain-shader adaptations. They are included in the VPK so no additional data-patch ZIP is needed. Skip this step if your previous test installation already contains them.

   The relevant layout is:

   ```text
   ux0:data/starfront/
   ├── libstarfront.so
   ├── GloftSFHP/
   │   ├── data.save
   │   ├── effects.gla
   │   ├── sprites_1024.gla
   │   ├── ... other original data files ...
   │   └── TerrainShaders/
   │       ├── UnlitMultiTextureNoVertexColorVS.glsl
   │       └── UnlitMultiTextureBlendNoVertexColorFS.glsl
   └── save/files/data.save
   ```

5. Transfer the VPK to the Vita and install it with VitaShell. Start **Starfront Test**. When the resource checks pass, press **×** to enter the game.

6. The bubble icon can update while the full LiveArea page retains its old appearance. This is a known issue in 00.06; the automatic repair attempted in 00.07 has been removed after the reported startup regression.

## Controls

| Input | Action |
| --- | --- |
| Front touchscreen | Original touch interface: menus, selection and commands |
| Two fingers on the front touchscreen | Original multi-touch gestures, including box selection |
| × on the startup diagnostic screen | Continue after the resource checks |
| START | Android Back action |
| SELECT + START | Exit the game |

The current build uses a **1024 × 600** logical canvas, fitted into the Vita's **960 × 544** screen with matching touch coordinates. A complete gamepad control scheme is not implemented.

## Status and known issues

- **00.06:** uses the earlier startup path. The full-page LiveArea artwork issue is unresolved. An experimental 00.07 updater was withdrawn after a hardware startup hang and is not included in this release.
- **00.05–00.06:** include the `Game.notifyTrophy(I)V` fix. Achievements are stored in the game's local `androidTrophy.dat`; this does not provide PSN trophies.
- PSV builds do **not** create or update `port.log`. Startup errors are displayed on screen.
- Full campaign progression, audio quality, save/resume behavior, performance and extended hardware sessions are not yet fully tested.
- Unsupported Android/JNI calls can still stop execution with a diagnostic message. Network and multiplayer behavior are unverified.
- Vita3K testing used a separate build and local graphics fixes. Compatibility with an unmodified Vita3K installation is not claimed by this PSV release.

For a bug report, [open an issue](https://github.com/vctorwei/starfront-vita/issues) with the VPK version, device/firmware, steps to reproduce, and a photo of the complete error message. For `C2-12828-1`, include the reported core-dump filename. Do not attach APKs, OBBs, or game-data archives.

## Credits

### Port and reference projects

- [vctorwei](https://github.com/vctorwei) — Starfront port project and PSV testing.
- **Gameloft** — the original game and its artwork. The Vita cover uses artwork from the supplied game assets.
- [Volodymyr Atamanenko](https://github.com/v-atamanenko), [Backstab Vita](https://github.com/v-atamanenko/backstab-vita) — reference for the related Gameloft engine, menus, graphics interfaces and LiveArea packaging.
- [Volodymyr Atamanenko](https://github.com/v-atamanenko), [Modern Combat 3 Vita](https://github.com/v-atamanenko/mc3-vita) — reference for Android/Gameloft compatibility, shaders and the LiveArea layout; inspiration for this README's organization.
- [WolffsRoom](https://github.com/WolffsRoom), [Modern Combat 2: Black Pegasus Vita](https://github.com/WolffsRoom/MC2BPegasus-Vita) and its upstream contributors — reference for JNI initialization and graphics compatibility.
- [Andy “TheFloW” Nguyen](https://github.com/TheOfficialFloW), [GTA: San Andreas Vita](https://github.com/TheOfficialFloW/gtasa_vita) — the original Android `.so` loader used as this port's loader foundation, and an ELF/ABI reference.

All four reference projects are acknowledged above. Their authorship and licenses remain with their respective contributors.

### Acknowledgements carried forward from MC3

The complete contributor list from [MC3's Credits](https://github.com/v-atamanenko/mc3-vita#credits) is retained here, with each contribution attributed to **MC3**, not presented as direct work on Starfront:

| Contributor | Contribution acknowledged by MC3 |
| --- | --- |
| [Andy “TheFloW” Nguyen](https://github.com/TheOfficialFloW) | Foundation of the Android shared-library loader |
| [Rinnegatamante](https://github.com/Rinnegatamante) | Rendering and audiovisual troubleshooting, plus video playback and trophy implementation |
| [CatoTheYounger](https://github.com/CatoTheYounger97) | Testing and quality checks |
| [Once13One](https://github.com/once13one) | MC3's LiveArea artwork |
| [GrapheneCt](https://github.com/GrapheneCt) | CapUnlocker |

### Tools and libraries

Thanks to the **VitaSDK / VitaSDK softfp** developers; **Rinnegatamante** and contributors to **vitaGL**, **vitaShaRK**, **SceShaccCgExt** and the softfp package collection; **bythos14** and **kubridge** contributors; the **VitaShell**, **Vita3K**, **PSPSDK**, **AOSP / Apache Harmony**, **pthreads**, **math-neon**, **zlib**, and **PolarSSL** contributors. Version references and license notices are collected in [THIRD_PARTY.md](THIRD_PARTY.md).

## Distribution and licensing

This is an independent homebrew project, unaffiliated with Gameloft or Sony. Starfront and the game artwork remain the property of their owners.

This repository hosts release documentation and VPK downloads. It does not contain the development workspace or a source build tree. The VPK includes the compatibility loader, LiveArea artwork, small APK-derived support resources and the two terrain adaptations; it does not include the original native game library or the full game data. Use your own game installation.

Third-party components keep their own license terms. See [LICENSE.md](LICENSE.md), [THIRD_PARTY.md](THIRD_PARTY.md), and [licenses/](licenses/). The upstream MIT notices are not a blanket MIT license for the combined VPK or for Gameloft's assets.
