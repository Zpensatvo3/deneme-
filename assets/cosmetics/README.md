# Cosmetic artwork: Wings and Cape

Five original cosmetic textures generated with the built-in image_gen tool on 2026-10-20.
No CLI/API fallback, downloaded third-party art or assets from the reference Java client were used.

[Module behavior](../../docs/MODULES.md#kozmetikler-ve-efektler) covers local-only
rendering and limitations; [third-party notices](../../THIRD_PARTY_NOTICES.md)
remain the separate attribution/licensing record. The exact prompts below are
asset provenance, not a request to regenerate or modify any image.

## Runtime assets

- `wing-dragon.png`, `wing-angel.png`, `wing-butterfly.png`: 640 x 640 RGBA, genuinely transparent silhouettes. Used by Wings styles 0/1/2 when Detailed Art is on (default).
- `cape-phoenix.png`, `cape-astral.png`: 512 x 768, opaque textile artwork. Used by Cape styles 8/9. Existing styles 0-7 remain available.
- Original generated images remain in Codex's generated_images library; project assets are mechanically downsampled PNG copies, not repainted versions. `scripts/Build-CosmeticAssets.ps1` accepts the five original PNG paths to regenerate production-sized assets. Transparent wing alpha is retained.
- Resource IDs 320-324 embed these images. They use the existing WIC/OpenGL mipmapped texture cache; GUI shutdown deletes them in the owning GL context. No runtime network request or loose-file dependency.
- Tint Artwork is off by default so original artwork colors survive. Old theme/custom-color fields still import; they control procedural styles, or artwork when tint is explicitly enabled.

## Physics / rendering

Cape uses 9 x 13 Verlet particles, pinned shoulders, structural/shear constraints, drag and torso/leg collision. Movement and turns drive inertia; Physics scales the response. Wings use damped shoulder and delayed tip joints reacting to movement/turns plus Flapping/Flap Speed. Both use a 120 Hz bounded step (maximum six steps per rendered frame). Pause, teleport, player/world change, menus and disable reset stored motion; Reduce Motion/Animation off gives a static pose. This changes only cosmetic geometry, never player motion, reach, inputs or packets. Environment blocks occlude rendering; they are not a full world-collision cloth simulation.

Textured triangle UVs are interpolated through body/block shadow and near/frustum clipping, then projected/tessellated into the existing ImGui overlay. Wings mirror the same single-wing texture on the two sides. Cape textures follow the simulated cloth rather than a camera-facing quad. Glow uses the same clipped textured geometry, not a rectangular background.

Offline checks: `Test-Offline.py --compiler clang++ --test cosmetic_physics --test remaining_settings --jobs 2`, plus `Test-Infrastructure.py` and MSVC syntax checks. In-game rendering/FPS and final DLL build require separate validation; no live game was modified.

## Exact generation prompts (built-in image_gen)

### dragon

Transparent background: yes.

Use case: stylized-concept. Production game cosmetic texture, square RGBA PNG with genuinely transparent background. Exactly ONE fully spread dragon/bat wing, a flat orthographic cutout viewed perpendicular to the membrane; not a pair. The shoulder attachment sits at the LEFT edge around 52% canvas height, the wing fans to the RIGHT. Intricate obsidian-violet membranes, layered dark metallic rib bones, fine luminous amethyst veins and small silver edge highlights, elaborate but legible in Minecraft third-person. A broad swept upper lobe with sharp tips, scalloped lower membrane; fill nearly the full square leaving 3% transparent safety padding. Neutral even illumination, all fine details inside the wing silhouette, crisp anti-aliased alpha edges. No dragon body, character, shoulder armor, pedestal, floor, cast shadow, backdrop, lettering, logo or watermark. No perspective, no folded pose, no texture atlas.

### angel

Transparent background: yes.

Use case: stylized-concept. Production game cosmetic texture, square RGBA PNG with genuinely transparent background. Exactly ONE angel wing fully open, orthographic flat cutout, not a pair. Attachment at LEFT edge around 52% canvas height; layered feather fan stretches RIGHT, upper arc lifting slightly, longest individual flight feathers sweeping outwards and down. Beautiful intricately layered ivory and silver feathers with subtle pale-gold quills and restrained cool-blue iridescent highlights, soft clean shading painted into the material; detailed white-on-silver feather separation, strong readable silhouette at game scale. Fill nearly the full square with 3% transparent safety padding. No person, angel body, second wing, armor, scenery, cast shadow, text, logos or watermark. Flat perpendicular view, no perspective or garment mockup.

### butterfly

Transparent background: yes.

Use case: stylized-concept. Production game cosmetic texture, square RGBA PNG with genuinely transparent background. Exactly ONE side of an open butterfly's wings, upper and lower lobe connected at a narrow root on LEFT edge around 52% canvas height, fanning to the RIGHT, orthographic flat texture. Jewel-like lunar moth/butterfly design: luminous teal, cobalt, violet and a few warm-gold eye spots, complex filigree vein network, dark navy velvet borders with tiny pearlescent speckles, delicate scalloped silhouette. Rich detailed fantasy game quality, not a simple gradient or line icon. Fill almost full square with 3% transparent safety margin. No insect body, antennae, other wing, human, scene, shadows, lettering, logo, watermarks or atlas. No perspective.

### cape_phoenix

Transparent background: no.

Use case: stylized-concept. A production diffuse COLOR TEXTURE for a game cape, vertical 2:3 image, perfectly flat front-facing rectangular fabric print covering the entire image edge to edge. Near-black burgundy silk with an elaborate regal phoenix emblem spreading its fiery gold and crimson wings across the upper and middle sections; fine feather filigree, embroidered burnished gold double border with small geometric corner ornaments and a restrained ember pattern toward the bottom. Luxurious detailed fantasy cosmetic, balanced symmetric heraldic layout, excellent readability at small scale. It must be FLAT TEXTURE ART, not an image of a hanging cape: no cape outline, folds, pleats, lighting gradient, mannequin, person, buckle, neck opening, perspective, background outside the cloth, text, logo or watermark. The runtime will deform the cloth; don't paint large fold shadows.

### cape_astral

Transparent background: no.

Use case: stylized-concept. A production diffuse COLOR TEXTURE for a game cape, vertical 2:3 rectangular artwork covering the full image edge to edge, flat orthographic textile print. Deep midnight navy cloth, an intricate luminous silver and ice-blue celestial dragon coiling around a central crescent moon, finely etched scales and feather-like fins, restrained star constellations and circular celestial filigree in the background, elegant silver and muted violet embroidered border. Premium fantasy MMO cosmetic, sophisticated detailed craftsmanship and a strong central motif readable in Minecraft. Entire image is the actual texture, not a garment photo or product mockup. No folds, wrinkles, cape-shaped silhouette, body, mannequin, perspective, neck hole, lettering, logo, watermark or margins outside the textile.
