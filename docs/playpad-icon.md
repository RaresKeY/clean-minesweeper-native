# Playpad icon

Created: 2026-10-03

## Source review and style provenance

- **H/User:** The project is a native Minesweeper game. The README says its SDL graphical mode opens in dark mode by default.
- **H/User:** `specs/_readme.md` routes to local-launch guidance and the README. No `specs/design/` routing map, `specs/design/prompts/`, icon prompt record, or formal icon art direction was present in the owning checkout.
- **H/User:** The current renderer in `src/main.c` uses a near-black charcoal background (`#161817`), muted sage-teal closed cells (`#6C847E`), warm khaki open cells (`#706858`), bright blue and green number colors (`#5B9EFF`, `#5BC97A`), and red/coral accents (`#FF6666`). It draws a front-facing rectangular grid. The reviewed native BMP assets show beveled square tiles, a red flag, and a black mine with an ivory center. The reviewed `.local/render-1280x720-large.png` and `.local/native-assets-contact.png` show the same visual language.
- **AI/inference:** Since no formal art direction exists, the icon narrowly infers its style from those live assets and the current renderer: four large front-facing cells, one red flag, the blue `1`, the green `2`, and a black mine against coral on khaki. The icon uses a charcoal field, crisp block geometry, and the source palette. This composition is an icon-specific inference, not a recorded design requirement.
- **OPEN:** No separate written icon or brand direction was found.

## Generated asset

- File: `icon.png`
- Format: PNG, 1254×1254, square. The prompt requested 1024×1024; the built-in tool returned 1254×1254, and the generated file was copied unchanged.
- Provenance: generated on 2026-10-03 with the native built-in `image_gen` tool via `@image-gen` in the assigned GPT-6 Luna xhigh session. No source image was edited or supplied to the generator; style was specified from the sources above.

## Exact generation prompt

```text
Use @image-gen with the built-in image_gen tool to generate exactly one image.

Use case: stylized-concept
Asset type: square native Minesweeper game application icon, 1024×1024 PNG
Primary request: Create a distinctive, highly legible icon for Clean Mines, matching the project's observed in-game pixel art.
Scene/backdrop: Full-bleed near-black charcoal (#161817), with a centered square 2×2 Minesweeper tile grid, viewed straight-on.
Subject: Four large equal cells with crisp dark gutters. Upper-left is a closed beveled muted sage-green tile (#6C847E) holding a simple upright charcoal flagpole and bold triangular red flag (#E43C35). Upper-right is an open flat warm khaki tile (#706858) with one large crisp blue pixel numeral 1 (#5B9EFF). Lower-left is an open warm khaki tile with a large simple black eight-spoked mine silhouette, small warm ivory center and a clear coral-red circular backing (#FF6666). Lower-right is an open warm khaki tile with one large crisp green pixel numeral 2 (#5BC97A). Keep each symbol centered within its cell and large enough to read at 96–120 px.
Style/medium: Faithful hard-edged raster pixel art, flat front-on game UI, chunky regular pixel geometry, restrained palette, simple geometric bevels on the closed tile only. No perspective, no soft rendering, no texture noise.
Composition/framing: Balanced square composition with a narrow safe margin around the grid; all four cells and their symbols fully inside frame; strong silhouette and clear color separation at small icon size.
Lighting/mood: Even flat UI color, no dramatic lighting.
Color palette: Use the project's dark charcoal, desaturated sage-teal, warm khaki, black, ivory, bright blue, green, and flag/coral red.
Materials/textures: Solid pixel blocks and clean stepped edges, echoing the project's 32×32 BMP tiles and dark-theme screenshot.
Text (verbatim): None.
Constraints: One square icon only. No letters, words, initials, slogans, labels, watermark, logo text or stock branding. Keep the only numerals exactly “1” and “2”. Avoid extra symbols, tiny details, gradients, glow, photorealism, 3D, shadows, or decorative framing.
```
