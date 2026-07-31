# Installation

## Requirements

- A Nintendo 3DS / 2DS / New 3DS running custom firmware (Luma3DS recommended)
- An SD card
- Internet connection (Wi-Fi)
- FBI (for installing `.cia`) **or** Homebrew Launcher (for `.3dsx`)

## Download

1. Go to the [Releases](https://github.com/SlabyLol/foxwebchat-/releases/tag/nightly) page.
2. Download the latest **FoxWebChat.cia**.

The app also offers an in-app update download when a newer version is detected.

## Install via FBI (recommended)

1. Copy `FoxWebChat.cia` to your SD card (any folder).
2. Open **FBI** on your 3DS.
3. Navigate to the `.cia` file → **Install and delete CIA**.
4. Launch **FoxWebChat** from the HOME Menu.

## Install via Homebrew Launcher (`.3dsx`)

1. Build or obtain `FoxWebChat.3dsx`.
2. Place it in `sdmc:/3ds/`.
3. Launch via the Homebrew Launcher.

## First Launch

On first start the app will:

1. Check for updates against Firebase.
2. Check the GitHub `themes/` folder for new community themes and offer to download them.
3. Create the folder structure on your SD card:

```
sdmc:/3ds/FoxWebChat/
├── themes/          ← put custom .fwct files here
├── theme.cfg        ← last selected theme index
└── FoxWebChat.cia   ← downloaded updates land here
```

## Web Edition

No installation needed. Open:

**https://slabylol.github.io/foxwebchat-/**

Enter a name and chat with the same room as the 3DS clients.

Copyright DarkFox Co.
