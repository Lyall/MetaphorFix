# Metaphor: ReFantazio Fix
[](https://www.patreon.com/Wintermance) [![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/W7W01UAI9)<br />
[![Github All Releases](https://img.shields.io/github/downloads/Lyall/MetaphorFix/total.svg)](https://github.com/Lyall/MetaphorFix/releases)

This is a work-in-progress fix for the Metaphor: ReFantazio demo that adds ultrawide/narrower support.

### 🚩Currently this fix only supports the DEMO version.

## Features
- Ultrawide/narrower support.

## Installation
- Grab the latest release of MetaphorFix from [here.](https://github.com/Lyall/MetaphorFix/releases)
- Extract the contents of the release zip in to the the game folder. e.g. ("**steamapps\common\Metaphor ReFantazio Demo**" for Steam).

### Steam Deck/Linux Additional Instructions
🚩**You do not need to do this if you are using Windows!**
- Open up the game properties in Steam and add `WINEDLLOVERRIDES="winmm=n,b" %command%` to the launch options.

## Configuration
- See **MetaphorFix.ini** to adjust settings for the fix.

## Known Issues
Please report any issues you see.
This list will contain bugs which may or may not be fixed.

## Screenshots
| |
|:--:|
| Gameplay |

## Credits
[Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) for ASI loading. <br />
[inipp](https://github.com/mcmtroffaes/inipp) for ini reading. <br />
[spdlog](https://github.com/gabime/spdlog) for logging. <br />
[safetyhook](https://github.com/cursey/safetyhook) for hooking.
