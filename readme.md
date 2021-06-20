
# LabFont

![LabFont][LabFont.jpg]

This project is an exploration of getting text into a rendeing pipeline
based on @floooh's sokol. Frameworks such as Dear ImGui solve text
rendering well, but are bound into their overall framework scheme; it's
not easy to use Dear ImGui's font rendering outside of GUI rendering, for
example to draw in game text, or use text as an effect in a particle system.

LabFont therefore takes sokol's fontstash binding as a starting point, and
wraps the minutiaea of using it in a simple interface with not much more
utility than loading a TTF format font, and rendering it via sokol gl.

It also adds support for loading bitmapped fonts laid out according to QuadPlay's
conventions, and allows for rendering of those as well.

LabFont supports state baking, so that font rendering isn't a large chunk
of code with discrete state setting; a baked state encapsulates size, spacing,
color, and a number of other attributes so that rendering text can be as simple
as

```c
// returns first pixel's x coordinate following the drawn text
float LabFontDraw(const char* str, float x, float y, LabFontState* fs);
```

Since LabFont is built around sokol, a simple sokol instantiater is provided
for build simplicity, via LabSokol.h. As a proof of concept, @rxi's microui
has been refactored to draw via LabFont and demonstrated with QuadPlay
fonts and TTF fonts. @rezonality's Zep vim emulator has been ported to
LabFont and Sokol as well, and then Zep embedded in microui to demonstrate
that all these systems with different font rendering requirements can
interoperate without interfering with each other.

As a future development, I'd like for LabFont to talk to a drawing interface
making sokol one available back end. As it stands, the current architectural
restriction is that LabFont rendering has to occur during the sokol-gl
phase of your framing function.

##Acknowledgements

This project combines several libraries, thanks to their respective authors!

- https://github.com/memononen/fontstash
- https://github.com/nothings/stb
- https://github.com/floooh/sokoli
- https://github.com/rxi/microui
- https://github.com/rezonality/zep
- https://github.com/Tencent/rapidjson
- https://github.com/morgan3d/quadplay (bitmapped font layout) 

