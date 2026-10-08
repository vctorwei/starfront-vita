# Vita system artwork

`assets/sce_sys/icon0.png`, `pic0.png`, and the two PNGs under
`livearea/contents/` are resized Starfront: Collision HD artwork from the
maintainer-supplied Android 1.0.0 APK. The source members are
`res/drawable/icon.png` and `res/drawable/gi_background.png`.

Copyright © Gameloft. These images are not licensed under this project's MIT
or GPL terms. Attribution is not a grant of redistribution rights. They are
included for the Vita bubble, launch image and LiveArea appearance at the
maintainer's request; the original APK and playable game data are not included.

`template.xml` follows the `ad0` background + clickable frame arrangement in
[Rocroverss/Gun-Bros-Psvita](https://github.com/Rocroverss/Gun-Bros-Psvita/tree/dab9e6a22b77c352bb74baf12787501c53d4e1c7/extras/livearea).
The 280×158 image dimensions preserve Starfront's source aspect ratio.
The system Shell supplies the page-peel interaction.

To regenerate from your own APK: `python3 scripts/prepare_shell_art.py --apk Starfront.apk`
(requires Pillow). No network download is performed by that script.
