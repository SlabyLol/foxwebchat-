# Building from Source

## Prerequisites

- [devkitPro](https://devkitpro.org/) with the **3DS** toolchain
- `DEVKITARM` environment variable set (e.g. `/opt/devkitpro/devkitARM`)
- Libraries: `citro2d`, `citro3d`, `libctru`, `curl`, `mbedtls`, `zlib`

```bash
sudo dkp-pacman -S 3ds-dev 3ds-citro2d 3ds-citro3d 3ds-curl 3ds-mbedtls 3ds-zlib
```

## Build

```bash
git clone https://github.com/SlabyLol/foxwebchat-.git
cd foxwebchat-
make          # produces FoxWebChat.3dsx + .elf
make cia      # produces FoxWebChat.cia (requires banner assets)
```

### Required assets for CIA

- `resources/icon.png` – application icon
- `resources/banner.png` – banner image
- `resources/banner.wav` – 16-bit WAV banner sound

## Project structure

```
foxwebchat-/
├── source/main.cpp      # Entire application (single file)
├── Makefile
├── app.rsf              # CIA metadata (UniqueId 0xF0011)
├── themes/              # Community .fwct themes
├── makeYourOwnTheme/    # Theme Creator website
├── index.html           # Web Edition
├── resources/           # Icon, banner, sounds
└── wiki/                # This documentation
```

## Technical notes

- Uses **citro2d / citro3d** for dual-screen rendering.
- Networking via **libcurl** + **mbedtls** (SSL).
- Backend: Firebase Realtime Database (`foxwebchat-bd592`).
- Theme system is completely data-driven; custom themes are loaded from SD at runtime.
