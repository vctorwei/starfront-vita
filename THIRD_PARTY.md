# Third-party components and reference acknowledgements

The project credits below distinguish code used in the loader, dependencies, and projects consulted as references. Original game files are not relicensed by this project.

## Reference projects

| Project | Authors / maintainers | Reviewed revision | Use |
| --- | --- | --- | --- |
| [Backstab Vita](https://github.com/v-atamanenko/backstab-vita) | Volodymyr Atamanenko and contributors | [`290130a8`](https://github.com/v-atamanenko/backstab-vita/commit/290130a8d6cb4b882ad445da4b4d9ac9f8377436) | Related Gameloft engine, sprite/menu and graphics interfaces, LiveArea |
| [Modern Combat 3 Vita](https://github.com/v-atamanenko/mc3-vita) | Volodymyr Atamanenko, TheFloW, Rinnegatamante and contributors | [`6fd1808d`](https://github.com/v-atamanenko/mc3-vita/commit/6fd1808dbaaa82f97fc653d1616efdcb7b856ea5) | Android compatibility, shader interfaces, LiveArea and README organization |
| [Modern Combat 2: Black Pegasus Vita](https://github.com/WolffsRoom/MC2BPegasus-Vita) | WolffsRoom and upstream contributors | [`253676d4`](https://github.com/WolffsRoom/MC2BPegasus-Vita/commit/253676d4e0a3c33cfab133a06c3c1e6ad552a26f) | Gameloft JNI initialization and graphics compatibility |
| [GTA: San Andreas Vita](https://github.com/TheOfficialFloW/gtasa_vita) | Andy Nguyen / TheFloW and contributors | [`96941714`](https://github.com/TheOfficialFloW/gtasa_vita/commit/96941714673c56b689d51ce6f79df68bbd0bebd5) | ELF-loader foundation, ABI and relocation reference |

Their MIT notices are reproduced in `licenses/`. Reference acknowledgement does not imply that each project's code, configuration tools or artwork is bundled with Starfront.

## MC3's full acknowledgement list

As credited by [the MC3 project](https://github.com/v-atamanenko/mc3-vita#credits):

- **Andy “TheFloW” Nguyen** — foundational shared-library loading work.
- **Rinnegatamante** — MC3 rendering and audiovisual assistance, video playback and trophies.
- **CatoTheYounger** — MC3 testing / QA.
- **Once13One** — MC3 LiveArea art.
- **GrapheneCt** — CapUnlocker.

These are acknowledgements of work on MC3 and its dependencies, not assertions that those contributors tested or implemented this Starfront port. Starfront's cover uses its own game's assets, not Once13One's MC3 artwork.

## Loader and runtime components

- **Android SO loader:** [TheFloW's GTA SA loader](https://github.com/TheOfficialFloW/gtasa_vita/tree/96941714673c56b689d51ce6f79df68bbd0bebd5/loader), MIT, copyright Andy Nguyen. Local changes cover bounds/read checks, unresolved-import diagnostics, relocation addends and loader state cleanup.
- **Debug screen:** [VitaSDK samples/common](https://github.com/vitasdk/samples/tree/fe8fbef570f3280586c0c20157146e3faefb2181/common). Local initialization change permits re-submitting a framebuffer for error display. The embedded font carries PSPSDK BSD notices, including Marcus R. Brown, James Forshaw and John Kelley.
- **vitaGL:** [Rinnegatamante/vitaGL](https://github.com/Rinnegatamante/vitaGL/tree/dca4b9d143290d78ec043131a19be36c78cdc7b5), revision `dca4b9d143290d78ec043131a19be36c78cdc7b5`, GNU LGPL v3. Built with softfp and GLSL support. A local change preserves shader-compiler failures instead of continuing a postponed link with a null program.
- **vitaShaRK:** [Rinnegatamante/vitaShaRK](https://github.com/Rinnegatamante/vitaShaRK/tree/df24065e65098b2d1ac533760109ad4367573f28), revision `df24065e65098b2d1ac533760109ad4367573f28`, GNU LGPL v3. GLSL translation / compiler integration.
- **SceShaccCgExt, pthreads, math-neon and zlib:** runtime dependencies supplied by the [VitaSDK softfp package collection](https://github.com/Rinnegatamante/vitasdk-packages-softfp). Their respective upstream terms apply.
- **PolarSSL 1.3.9:** AES/Base64 support for Android preference compatibility. Copyright Brainspark B.V.; the installed library headers carry GPL v2-or-later terms. GNU license texts and the component notice accompany this release.
- **kubridge:** [bythos14/kubridge](https://github.com/bythos14/kubridge), an external kernel-plugin requirement. The plugin itself is not packaged in the VPK.
- **VitaSDK / VitaSDK softfp:** compiler, platform headers, system-library stubs and runtime support. The development toolchain was the `softfp-osx-v2.228` distribution.
- **AOSP / Apache Harmony:** Android ABI and Gingerbread preference behavior were used as references. Apache Harmony SHA1PRNG reference files were used for local test vectors; those Java files are not part of the VPK executable.

The third-party GNU licenses retain their source-availability and redistribution conditions. This page documents provenance; it does not replace those terms or claim that the combined binary is MIT-only.

## Game support resources

- **Gameloft:** original game and artwork. The VPK contains resized original artwork for the Vita shell and the small `igli.bin` / `serialkey.txt` support resources used by the compatibility layer. The original APK, OBB, game library and full game data are not distributed here.
- **Terrain shader adaptations:** the VPK's `compat/GloftSFHP/TerrainShaders/` contains two derived variants of shaders from the tested game's `effects.gla`. They remove the vertex-color attribute/varying and final color modulation for the corresponding material pass. They are adaptations, not recovered originals, and are not covered by the reference projects' MIT licenses.

## Development and testing tools

Thanks to [VitaShell](https://github.com/TheOfficialFloW/VitaShell), [Vita3K](https://github.com/Vita3K/Vita3K), [PSPSDK](https://github.com/pspdev/pspsdk), AOSP, Python/Pillow, CMake, and the compiler/library contributors. Emulator-specific diagnostic changes and tools are not included in this PSV hardware release.
