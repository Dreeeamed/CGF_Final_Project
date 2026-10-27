# Stylized Stone Wall 002

Provider: Stylized Textures.

- Asset: https://stylizedtextures.com/texture/stylized_stone_wall_002
- License information: https://stylizedtextures.com/faq
- License: CC0 1.0, https://creativecommons.org/publicdomain/zero/1.0/
- Original public folder: https://drive.google.com/drive/folders/1a-M5RHZqSl1sdCgnJKxasCE0i3WaAwOl
- Downloaded: 2026-10-20

These are the original 4096 x 4096 PNG maps, renamed without pixel changes.

| Local file | Original file ID | Role |
| --- | --- | --- |
| albedo.png | 15vfHdbbPR_Solf6aw8NhYUJv8MAIWe5d | Color, authored in sRGB |
| height.png | 1AvC4gf7C4FioBJq2lIcoGQV4TgHeTC-Q | Height; raised stone is lighter, recesses darker |
| normal.png | 18zd7aOXXodKJTS8XwPHtMbMa89b-jwN0 | Normal map; verify the green-channel convention before enabling |
| roughness.png | 1KY5XCNzDWicY04Ogev0Fw0mhmD1xeOlm | Roughness |

The first lesson connects the color map. Normal mapping, roughness lighting,
and POM need separate shader changes; placing these files here does not
implement those effects. For a POM ray marcher using depth measured inward,
sample depth as `1.0 - height` rather than treating this height map as depth.

When enabling physically linear lighting, decode albedo as sRGB and keep
height, normal, and roughness maps in linear data formats. Keep map UVs and
image orientation consistent across the material.
