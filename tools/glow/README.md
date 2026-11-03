# Glow table

Which texels of the game's textures are light sources (lit windows, neon, lamps, car lights, fire,
sparks...), for the OpenGL renderer's glow. `glow_table.json` maps a pixelmap name to its emissive
palette indices (optionally limited to rectangles, for car lights next to paint of the same colour),
a kind and a suggested intensity.

It was made by finding candidate texels by colour and reviewing contact sheets of them by eye; the
per-texture decisions are in `decisions.py`. For windows, lamps, signs, neon and screens the marked
bright cores are grown over the rest of the lit pane (connected texels of the same hue, a little
dimmer), limited to the panes' bounding boxes where those colours also appear elsewhere.

    python3 load.py <game>/DATA   # extracts the textures and palette (texs.pkl)
    python3 build.py              # applies decisions.py, writes glow_table.json
    python3 review.py             # contact sheets with the emissive texels highlighted
    python3 gen_c.py              # writes src/harness/glow_table.c, compiled into the game
