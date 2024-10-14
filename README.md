# Metaphor: ReFantazio Fix
[![Patreon-Button](https://github.com/user-attachments/assets/0468283d-b663-4820-b0f5-40e41d96832c)](https://www.patreon.com/Wintermance) [![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/W7W01UAI9) <br />
[![Github All Releases](https://img.shields.io/github/downloads/Lyall/MetaphorFix/total.svg)](https://github.com/Lyall/MetaphorFix/releases)

This is a fix for Metaphor: ReFantazio that adds ultrawide/narrower support and much more.

## Features
### General
- Intro skip.
- Disabled ALT+F4/exit handler.
- Remove 60fps cap in menus.
- Fix 8-way analog movement.
- Custom resolution scale.
- Adjust resolution of ambient occlusion.
- Adjust shadow resolution.

### Ultrawide/narrower
- Support for any resolution/aspect ratio.
- Fix cropped FOV at <16:9.
- Correctly scaled movies.
- Fixes stretched HUD.

### Graphics
- Disable dash blur + speed lines.
- Adjust ambient occlusion resolution.
- Adjust level of detail distance.
- Disable black outlines.

## Installation
- Grab the latest release of MetaphorFix from [here.](https://github.com/Lyall/MetaphorFix/releases)
- Extract the contents of the release zip in to the the game folder. e.g. ("**steamapps\common\METAPHOR**" for Steam).

### Steam Deck/Linux Additional Instructions
🚩**You do not need to do this if you are using Windows!**
- Open up the game properties in Steam and add `WINEDLLOVERRIDES="winmm=n,b" %command%` to the launch options.

## SpecialK
- As of the latest update for the game (5/10/24) [SpecialK](https://www.special-k.info/) is no longer essential for improving performance. Still, there are many other benefits to using SpecialK with this (and any other) game.
- If you are using this fix with SpecialK, you will need to delete the bundled Ultimate ASI Loader DLL (`winmm.dll`). You can then add MetaphorFix as a plug-in through the SpecialK OSD (accessed with `CTRL+SHIFT+BACKSPACE`).

## Configuration
- See **MetaphorFix.ini** to adjust settings for the fix.

## Known Issues
Please report any issues you see.
This list will contain bugs which may or may not be fixed.

#### HUD Fix
- Some 3D menu elements like pause screen text are invisible.
- Screen captures may appear squashed as they are resized to 16:9.
  
## Screenshots
| ![ezgif-7-f7f2cd227e](https://github.com/user-attachments/assets/b3fa0f83-4c51-4e06-908b-81288c2f6957) |
|:--:|
| Gameplay |

## Credits
[Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) for ASI loading. <br />
[inipp](https://github.com/mcmtroffaes/inipp) for ini reading. <br />
[spdlog](https://github.com/gabime/spdlog) for logging. <br />
[safetyhook](https://github.com/cursey/safetyhook) for hooking.
