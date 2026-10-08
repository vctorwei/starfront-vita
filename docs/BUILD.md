# Building and relinking 00.06-r3

Use a **softfp VitaSDK** with CMake 3.18+, Python 3.9+ and a Make or Ninja build tool.
The release was built on macOS with the VitaSDK `softfp-osx-v2.228` toolchain
(GCC 10.3.0). Toolchain sources and setup: [VitaSDK buildscripts](https://github.com/vitasdk/buildscripts),
[softfp package recipes](https://github.com/Rinnegatamante/vitasdk-packages-softfp).

```sh
export VITASDK=/path/to/vitasdk
export PATH="$VITASDK/bin:$PATH"
sh scripts/build.sh
```

Output: `build/Starfront-test.vpk`, `build/starfront`, `build/starfront.velf`,
`build/eboot.bin` and `build/starfront.map`. No APK or game data is needed to build.
A build from the release's **Source code (zip)** includes all sources under
`src/`, `vendor/` and `deps/`; no Git submodules are required.

`deps/CMakeLists.txt` builds vitaGL, vitaShaRK, SceShaccCgExt, pthread-embedded,
math-neon, zlib and the used PolarSSL modules from the included source. Their
versions and local changes are recorded in `deps/sources.json` and
`THIRD_PARTY.md`. These targets do not use SDK-installed copies of those
libraries. VitaSDK supplies compiler runtime support, libc/libm, C++ runtime,
platform stubs and the taiHEN/kubridge interface headers/stubs.

To relink a modified library, edit its source under `deps/` and rerun the build.
The application sources are also supplied, so no opaque application object is
needed. Install the resulting VPK with VitaShell on a homebrew-enabled Vita;
no project signing key or project activation is required. Supply your own game
data and the external plugins described in the README. The project adds no
restriction on debugging modified library versions.

Default release settings preserve 00.06's 1024×600 logical layout scaled to
960×544, disable file logging and start the game automatically after preflight. This revision does not
include the withdrawn 00.07 LiveArea refresh calls. It changes APK resource
paths, shell artwork, dependency build provenance, and removes the optional
Sony intrinsic declaration text from SceShaccCgExt. The remaining extension
hooks use the user's installed compiler library. These changes require new
hardware testing; this build is not byte-identical to the old 00.06 binary.

## Verification

```sh
python3 -m unittest discover -s tests -p 'test_*.py'
python3 -m pip install pyelftools
python3 scripts/validate_build.py --build-dir build --vpk build/Starfront-test.vpk
```

The checks cover host JNI behavior, local data preparation and static build
properties. They do not establish Vita hardware compatibility or complete
campaign playability.

The icon, background and launch splash use Starfront's Android APK artwork.
The New Mission card is separately AI-generated. Provenance is recorded in
`assets/ARTWORK.md`; artwork is outside the software licenses. Regeneration
of the three game-derived PNGs from your own APK requires Pillow:

```sh
python3 -m pip install Pillow
python3 scripts/prepare_shell_art.py --apk Starfront.apk
```

The generated launch-card master is included in `assets/source/new-mission.png`.
To resize and encode it as a transparent, indexed PNG, use Node.js with sharp:

```sh
npm install --no-save --package-lock=false sharp
node scripts/prepare_launch_card.cjs
```

00.06-r3 updates packaging and shell presentation. Its only runtime source
change relative to r1/r2 is `src/log.c`: normal PSV startup leaves the debug
screen disabled, while fatal errors initialize the screen and display the
cause. Emulator startup diagnostics are retained. The vitaGL initialization
and gameplay sources are unchanged. A host test covers quiet startup and
visible fatal errors for both build modes; the new binary is not byte-identical
to r1/r2 and has not been retested in-game.
The Gun Bros-inspired template uses `ad0`, a background image and a clickable
`frame` targeting `psla:eboot`; no runtime LiveArea refresh function is called.

`scripts/make_sfo.py` generates the SFO using the current VitaSDK game schema,
avoiding deprecated defaults from the older softfp SDK's mksfoex. With Gun Bros
v1.0's metadata inputs it reproduces that release's SFO byte for byte. See
[VitaSDK issue 282](https://github.com/vitasdk/vita-toolchain/issues/282).
The maintainer confirmed the system page working with the r3 metadata test VPK;
the new generated card has only received static format and readability checks.

The website promotional screenshot is only externally linked; it is not part of
the loader, its shell artwork, or the build inputs.
