# Third-party notices

Original project contributions use MIT. The combined loader binary is distributed
under GPLv3 because it includes GPL libraries. Component licenses and notices
remain intact; they do not grant rights to the original game or system software.

## Reference projects

| Project | Authors / maintainers | Reviewed revision | Use |
| --- | --- | --- | --- |
| [Backstab Vita](https://github.com/v-atamanenko/backstab-vita) | Volodymyr Atamanenko and contributors | [`290130a8`](https://github.com/v-atamanenko/backstab-vita/commit/290130a8d6cb4b882ad445da4b4d9ac9f8377436) | Related Gameloft engine, sprite/menu and graphics interfaces, LiveArea |
| [Modern Combat 3 Vita](https://github.com/v-atamanenko/mc3-vita) | Volodymyr Atamanenko, TheFloW, Rinnegatamante and contributors | [`6fd1808d`](https://github.com/v-atamanenko/mc3-vita/commit/6fd1808dbaaa82f97fc653d1616efdcb7b856ea5) | Android compatibility, shader interfaces, LiveArea and README organization |
| [Modern Combat 2: Black Pegasus Vita](https://github.com/WolffsRoom/MC2BPegasus-Vita) | WolffsRoom and upstream contributors | [`253676d4`](https://github.com/WolffsRoom/MC2BPegasus-Vita/commit/253676d4e0a3c33cfab133a06c3c1e6ad552a26f) | Gameloft JNI initialization and graphics compatibility |
| [GTA: San Andreas Vita](https://github.com/TheOfficialFloW/gtasa_vita) | Andy Nguyen / TheFloW and contributors | [`96941714`](https://github.com/TheOfficialFloW/gtasa_vita/commit/96941714673c56b689d51ce6f79df68bbd0bebd5) | ELF-loader foundation, ABI and relocation reference |

Additional reference: [Gun Bros PSVita](https://github.com/Rocroverss/Gun-Bros-Psvita) by Rocroverss and contributors, revision `dab9e6a22b77c352bb74baf12787501c53d4e1c7`, reviewed for Vita system LiveArea templates and VPK packaging. No Gun Bros game assets or runtime code are included; the LiveArea XML arrangement is adapted with its MIT notice preserved in `licenses/Gun-Bros-MIT.txt`.

Additional audio reference: [SDL Vita audio backend](https://github.com/libsdl-org/SDL/blob/SDL2/src/audio/vita/SDL_vitaaudio.c), by the SDL contributors, reviewed for aligned double buffering and blocking output. SDL is not a linked dependency; this project retains its own AudioTrack implementation.

Their MIT notices are reproduced in `licenses/`. Reference acknowledgement does not imply that each project's code, configuration tools or artwork is bundled with Starfront.

## Contributors acknowledged by MC3

Andy “TheFloW” Nguyen (SO loader), Rinnegatamante (rendering, audio/video and
trophies), CatoTheYounger (QA), Once13One (LiveArea art), and GrapheneCt
(CapUnlocker), as credited by [MC3](https://github.com/v-atamanenko/mc3-vita#credits).
These credits acknowledge their prior work, not participation in Starfront testing.

## Included code and libraries

Full dependency revisions and source URLs are in [deps/sources.json](deps/sources.json).
The sources in this release, including local changes, are the sources used to
build its VPK. Upstream license texts accompany each component.

| Component | License / notices | Source in this repository and changes |
| --- | --- | --- |
| TheFloW Android SO loader | MIT, Andy Nguyen | `vendor/so-loader`; bounds/read checks, import diagnostics, relocation addends and state cleanup |
| ELF interface header | LGPL-2.1-or-later, Free Software Foundation | `vendor/so-loader/elf.h`; existing glibc header notice retained |
| VitaSDK debug screen / PSPSDK font | Upstream notices; BSD font notice in `licenses/PSPSDK-BSD.txt` | `vendor/debugscreen`; framebuffer resubmission for error display; upstream VitaSDK samples common files |
| vitaGL | LGPL-3.0; `COPYING` and `COPYING.LESSER` | `deps/vitaGL`; local postponed shader compilation failure guard, already applied and also supplied as a patch |
| vitaShaRK | LGPL-3.0 | `deps/vitaShaRK`; unchanged source |
| SceShaccCgExt v1.0.1 | GPL-3.0 | `deps/SceShaccCgExt`; optional `sce_intrinsics.h` text and its loading call removed on 2026-10-08; pragma/extension hooks retained |
| pthread-embedded | LGPL-2.0-or-later (upstream ships LGPL-2.1 text); Vita backend MIT, Davee | `deps/pthread-embedded`; unchanged source; standalone CMake target uses the upstream Vita source list |
| math-neon | MIT, Lachlan Tychsen-Smith | `deps/math-neon`; this fork's source files carry MIT notices |
| zlib 1.2.12 | zlib, Jean-loup Gailly and Mark Adler | `deps/zlib`; unchanged source |
| PolarSSL 1.3.9 | GPL-2.0-or-later, Brainspark B.V. | `deps/polarssl`; unchanged source; only AES/Base64 and their platform support modules linked |
| VitaSDK headers | MIT, VitaSDK contributors | `deps/vita-headers`; unchanged interface headers |
| Apache Harmony / AOSP | Apache-2.0 | `src/drm_preferences.c` SHA1PRNG compatibility arithmetic and `tests/reference/drm-gingerbread`; original notices retained |

Local build integration is in `CMakeLists.txt`, `deps/CMakeLists.txt` and
`deps/pthread-sources.cmake`. The vitaGL guard and SceShaccCgExt omission are
local changes, not upstream releases. The omitted header carried a Sony
confidential/all-rights-reserved notice; it is neither published nor embedded
in this release. No replacement proprietary declaration text is supplied.

The C/C++ compiler, libc/libm, compiler runtime and standard C++ runtime, platform
stubs, and taiHEN/kubridge interfaces come from the softfp VitaSDK toolchain;
see [BUILD.md](docs/BUILD.md). GCC runtime components carry their runtime
exceptions; Newlib and toolchain components retain their respective upstream
notices. External kernel modules, Sony firmware and `libshacccg.suprx` are not
included. Runtime dependency sources above are rebuilt locally, not taken from
prebuilt SDK runtime archives.

## Artwork and game files

- Three shell PNGs are resized Starfront artwork from the supplied Android
  1.0.0 APK, © Gameloft. They are excluded from the project's MIT/GPL licenses;
  see `assets/ARTWORK.md`. The LiveArea template follows Gun Bros's `ad0` +
  clickable frame arrangement, with an explicit application CONTENT_ID.
- The launch card is AI-generated for this project, with Gun Bros's card as a
  layout reference and Starfront's artwork as a style reference. Its source
  master and prompt are included under `assets/source/`; it is not original game
  artwork. See `assets/ARTWORK.md` for provenance and scope.
- The README embeds an externally hosted Starfront promotional screenshot from
  [Gameloft's public post](https://x.com/gameloft/status/964888517984837637).
  Copyright remains with Gameloft. No permission to redistribute or relicense
  that screenshot is claimed. It is not a build input or a file in the VPK.
- The MP4/GIF is a maintainer-supplied recording of the earlier 00.06 build on a
  physical Vita. Depicted game content retains its owner's rights.
- Apart from the credited system artwork, APK/OBB, `libstarfront.so`,
  `igli.bin`, `serialkey.txt`, game archives, saves and
  derived game shaders are not distributed. `scripts/prepare_game.py` extracts
  resources and adapts two shaders locally from the user's verified 1.0.0 files.
  File names, sizes and hashes are used to identify the supported version.

Thanks also to VitaShell, Vita3K, kubridge, PSPSDK and the wider Vita homebrew
community. These acknowledgements and notices are not a statement of legal
approval from Gameloft, Sony or the reference-project authors.
