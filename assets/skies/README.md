# Sky artwork and panorama indices

Generated for this project using the built-in image generation tool. These PNGs
are original outputs copied without retouching or resizing. The older JPEGs are
separate CC0 Poly Haven assets; see the
[third-party notices](../../THIRD_PARTY_NOTICES.md#gökyüzü-panoramaları).

Runtime behavior is documented in [module notes](../../docs/MODULES.md#sky);
version-specific render contracts are in the [SDK audit](../../docs/MINECRAFT_26_PORT.md).

The renderer samples the upper sky, blends the horizontal wrap seam and zenith,
and uses explicit periodic texture gradients to avoid blurry seam lines. The
horizon and lower hemisphere use the current frame's actual terrain fog color;
fog distances control haze and suppress details in dense fog. Artwork exposure
does not change the fog tint. Texture loading is lazy. The latest fog fixes have
not been checked in Minecraft yet.

Config indices remain stable: 0 Starry Night, 1 Milky Way, 2 Cloudy Sunrise,
3 Candy Dusk (new default), 4 Nebula, 5 Aurora. Existing configs retain their choice.

## Shared generation prompt

Use case: stylized-concept. Asset type: finished game skybox texture, not a preview of a game. Generate one 360 degree equirectangular panorama in a precise 2:1 landscape canvas, preferably 4096x2048. Sky only, no landscape, no ground, no water, no reflection, no architecture or people, no text, no labels, no UI, no watermark. North pole is the entire top edge, equatorial horizon is exactly the horizontal middle row. Content across left and right image edges must seamlessly wrap; identical color/brightness and continuous cloud forms there. Calm nearly uniform zenith within top 8% so the sky sphere has no pole starburst. Fill the lower half with a quiet continuation of the horizon color; the game samples only the upper hemisphere. Stylized polished painterly Minecraft PvP texture-pack aesthetic with clear cloud silhouettes, smooth color gradients, uncluttered readable sky, crisp but soft-edged shapes. Not realistic photography. Strong visual design, no overexposure.

## candy_dusk.png — appended prompt

Violet and rose dusk. Deep periwinkle zenith smoothly descends into lavender, blush pink and a thin peach horizon. Large sculpted cotton-candy cumulus cloud banks throughout the middle-upper hemisphere, peach lit edges, purple shadow cores. Broad clean open spaces between clouds. Dreamy anime sky, no visible sun or moon, no stars. Center of interest in upper hemisphere 20-42 percent from top, never confined to the lower half.

## nebula.png — appended prompt

Indigo space sky. A luminous flowing ribbon of magenta violet and cyan nebula sweeps around the upper hemisphere, surrounded by deep navy with small sparse sharp stars of varied brightness. A restrained soft cosmic glow, not noisy and not photographic. No planets or giant celestial discs, no lens flares. Faint lavender atmospheric horizon. Wide negative space so game entities remain readable. Main colored nebula sits between 15 and 35 percent from top.

## aurora.png — appended prompt

Arctic blue midnight sky with flowing ribbons of mint cyan and soft violet aurora above the horizon, sweeping horizontally across the upper hemisphere between 20 and 40 percent from top. Crisp sparse stars, deep midnight-blue zenith, gentle blue lavender horizon. Very soft distant wispy cloud layer. Painterly stylized fantasy game sky, subtle glow, no moon, no sun, no mountains, no giant discs, no strong highlights.
