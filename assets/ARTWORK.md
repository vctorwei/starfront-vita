# Vita system artwork

`assets/sce_sys/icon0.png`, `pic0.png`, and
`livearea/contents/bg-r2.png` are resized Starfront: Collision HD artwork from the
maintainer-supplied Android 1.0.0 APK. The source members are
`res/drawable/icon.png` and `res/drawable/gi_background.png`.

Copyright © Gameloft. These images are not licensed under this project's MIT
or GPL terms. Attribution is not a grant of redistribution rights. They are
included for the Vita bubble, launch image and LiveArea appearance at the
maintainer's request; the original APK and playable game data are not included.

`livearea/contents/startup-r3.png` is an AI-generated launch card made for this
project using the built-in image_gen tool. Gun Bros's “New Mission” card was a
layout reference, and Starfront artwork was a style reference. It is not an
extracted or official Starfront game asset. The generated master and final prompt
are in `assets/source/new-mission.png` and `new-mission.prompt.txt`.
System artwork is excluded from the software licenses; no rights in the original
games, names or reference artwork are granted.

`template.xml` follows the `ad0` background + clickable frame arrangement in
[Rocroverss/Gun-Bros-Psvita](https://github.com/Rocroverss/Gun-Bros-Psvita/tree/dab9e6a22b77c352bb74baf12787501c53d4e1c7/extras/livearea).
The generated 425×344 transparent card is displayed at 280×225, matching Gun Bros.
The whole card launches the existing game; its wording does not create or reset a save.
The system Shell supplies the page-peel interaction.

To regenerate from your own APK: `python3 scripts/prepare_shell_art.py --apk Starfront.apk`
(requires Pillow). Re-encode the included generated card with
`node scripts/prepare_launch_card.cjs` (requires sharp). Neither script downloads artwork.
