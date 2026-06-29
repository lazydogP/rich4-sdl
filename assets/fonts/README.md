Place a CJK TrueType/OpenType font here for the SDL_ttf text path.

Preferred filenames:

- rich4.ttf
- rich4.ttc
- MingLiU.ttf / MingLiU.ttc
- PMingLiU.ttf / PMingLiU.ttc
- SimSun.ttf / SimSun.ttc

Desktop builds render Big5 UI text through the profile-selected SDL_ttf/FreeType
backend. Handheld builds use the generated `rich4_font.r4atlas` bitmap atlas and
do not require a font engine at runtime. The atlas in this workspace was baked
from `assets/fonts/rich4.ttc` with the repository `font_bake` tool and actual
CP950 byte sequences. It contains 13,847 glyphs (printable ASCII and valid
CP950 double-byte codes). Rebuild with
`python scripts/make_minimal.py font-bake-miyoo`; replace it when distributing with a font whose
license permits redistribution.
