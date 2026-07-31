# Themes

FoxWebChat supports multiple color schemes. Switch them anytime with **D-Pad Left / Right**.

## Built-in Themes

| Name | Description |
|------|-------------|
| Orange | Default warm fox theme |
| Blau/Violett | Blue / violet |
| Feuerrot | Fire red |
| Dunkel | Dark mode |
| Wald | Forest green |
| Pastell | Soft pastel pink |
| Sonnenschein | Sunshine yellow |
| Tuerkis | Teal |

## Custom Themes (`.fwct`)

You can create and load your own themes.

### File format

A `.fwct` file is a simple key=value text file:

```
# FoxWebChat theme file (.fwct)
name=My Theme
bg=247,127,51
mid=225,90,35
white=255,255,255
cream=255,247,240
dark=61,33,26
selectBg=255,213,181
muted=120,90,80
textColor=61,33,26
```

Colors are `R,G,B` or `R,G,B,A` (0–255). Lines starting with `#` are comments.

### Installing a custom theme

1. Copy the `.fwct` file to:
   ```
   sdmc:/3ds/FoxWebChat/themes/
   ```
2. Launch (or re-launch) FoxWebChat.
3. Use D-Pad Left/Right – your theme appears in the list automatically.

An example file is created on first launch.

### Theme Creator (Web)

Use the online editor:

**https://slabylol.github.io/foxwebchat-/makeYourOwnTheme/**

- Live preview
- Random theme generator
- One-click download of the `.fwct` file
- Submit button that opens a GitHub Issue template

### Community Themes

Themes placed in the repository’s `themes/` folder are automatically offered for download when the 3DS app starts (if they are not already present on the SD card).

Current community themes in the repo:

- `darkmatter.fwct`
- `hotsummer.fwct`
- `springsun.fwct`
- `test-theme.fwct`

### Submitting a Theme

1. Create your theme with the Theme Creator or by hand.
2. Open a GitHub Issue using the **Theme Submission** template, **or**
3. Open a Pull Request that adds your `.fwct` file under `themes/`.

Copyright DarkFox Co.
