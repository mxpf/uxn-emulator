# Playinghaus: chrome-led design direction

Scope: current local homepage, assessed against the user's selected airbrushed chrome logo. Read-only review; no website changes or deployment.

## Captured steps

1. Homepage arrival — clear but visually mismatched to the chrome direction. Large headline and pale hardware dominate; the small corner logo slot would lose the chrome lettering's detail.
   Screenshot: ../../build/chrome-design-audit-home-viewport.png
2. Games navigation — clear primary action and strong original cover art. The heavy beige bezel, green terminal lettering and scanlines are competing visual themes.
   Screenshot: ../../build/chrome-design-audit-library.png

## Recommended direction

A late-1970s home entertainment lounge: dark warm charcoal, ivory text, restrained amber/burnt-orange accents, chrome reserved for the brand. Keep the painted cover's identity and warm garden scene.

- Make the chrome logo the centered masthead, roughly 500–620 CSS pixels wide on desktop, responsive on smaller screens. Reduce Small games. Good company. to a short supporting line. Keep navigation visually quiet.
- Replace the pale page background with warm near-black; use slightly lighter charcoal for the featured game. Avoid introducing an unrelated neon palette.
- Simplify the console to one restrained dark surround and perhaps a fine metal edge. Remove stacked bevels, vents, scanlines over text and the decorative cursor from the library presentation. Keep any hardware styling subordinate to the art.
- Retain the full uncropped flat cover, simple game description, one clear Start game action and no-install reassurance. The game itself and ROMs remain untouched.
- Use readable ivory sans-serif body text and substantial simple headings. Keep monospace only for small catalog details; no chrome body text or chrome buttons.
- Use a solid amber play button with dark lettering and a clear focus treatment. Verify final contrast rather than assuming the palette is accessible.
- Mobile: compact masthead, no separate tall hero, cover/title/play action grouped closely. Avoid a screenful of branding before a game can be started.

## Asset preparation

The chosen image has a baked-in dark background and generous canvas padding, not transparency. Prepare a tightly framed web asset with preserved lettering, reflection colors, swash and glints, either composited for the exact masthead surface or with a carefully checked transparent edge. Create a simple flat small-size companion mark for the favicon and compact navigation. Keep the chrome as a static compressed image; no WebGL, canvas, lighting runtime or animation is needed. Use a text alternative, explicit dimensions and appropriately sized exports.

## Limits and next step

Current-run desktop visual/DOM inspection only; not a full accessibility audit or a new mobile test. New contrast, keyboard focus, small-size logo legibility, image weight and mobile ordering need validation in the proposed design. Next: desktop and mobile visual mockups using the exact selected logo and cover, then user selection before implementation.
