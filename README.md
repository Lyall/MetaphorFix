# Metaphor: ReFantazio Fix
[![Patreon-Button](https://github.com/user-attachments/assets/0468283d-b663-4820-b0f5-40e41d96832c)](https://www.patreon.com/Wintermance) [![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/W7W01UAI9) <br />
[![Github All Releases](https://img.shields.io/github/downloads/Lyall/MetaphorFix/total.svg)](https://github.com/Lyall/MetaphorFix/releases)

This is a work-in-progress fix for the Metaphor: ReFantazio demo that adds ultrawide/narrower support and more.

### 🚩Currently this fix only supports the DEMO version.

## Features
### General
- Intro skip.
- Disable pause when game loses focus.
- Disable ALT+F4/exit handler.
- Disable dash blur + speed lines.
- Remove 60fps cap in menus.

### Ultrawide/narrower
- Support for any resolution/aspect ratio.
- Correctly scaled movies.

## Installation
- Grab the latest release of MetaphorFix from [here.](https://github.com/Lyall/MetaphorFix/releases)
- Extract the contents of the release zip in to the the game folder. e.g. ("**steamapps\common\Metaphor ReFantazio Demo**" for Steam).

### Steam Deck/Linux Additional Instructions
🚩**You do not need to do this if you are using Windows!**
- Open up the game properties in Steam and add `WINEDLLOVERRIDES="winmm=n,b" %command%` to the launch options.

## Recommended Mods
- [SpecialK](https://steamcommunity.com/app/2679460/discussions/0/4842022494093910068/) - I highly recommend using this build of SpecialK with Metaphor: ReFantazio. It can improve performance significantly, along with offering many other useful features like an excellent framerate limiter.

## Configuration
- See **MetaphorFix.ini** to adjust settings for the fix.

## Known Issues
Please report any issues you see.
This list will contain bugs which may or may not be fixed.

- HUD fix is very much a work in progress right now and causes many issues. If you enable it, expect to see bugs.

## Screenshots
| ![ezgif-4-30ce77eaf8](https://github.com/user-attachments/assets/a8d2c026-1992-4c79-b5a4-edb603cc833f) |
|:--:|
| Gameplay |

## Credits
[Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) for ASI loading. <br />
[inipp](https://github.com/mcmtroffaes/inipp) for ini reading. <br />
[spdlog](https://github.com/gabime/spdlog) for logging. <br />
[safetyhook](https://github.com/cursey/safetyhook) for hooking.
