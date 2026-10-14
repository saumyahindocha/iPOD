# Pod case (Rev B)

Two printed parts: `pod_tray` (lower) and `pod_bezel` (upper), held by four M2 screws from underneath
(countersunk, hidden under the rubber feet). Outer 57.2 × 125.0 × 21.8 mm. Round Ø8.2 headphone-jack hole
(top end), USB-C charging and the Pico's micro-USB (right), microSD and power switch (left). Cavities sized
from the ordered parts (TP4056 29 × 17.3 × 4.3, slide switch 13 × 6.5 × 5.5, LiPo 403048, GY-PCM5102).

- `Pod_Case_RevB.pdf` — renders, what changed, part table with sources, dimensioned drawings, fitting notes, print settings, checks, 1:1 bezel + floor templates
- `pod_tray.stl/.step`, `pod_bezel.stl/.step`, `pod_case_assembled.step` — print or open in Fusion 360
- `pod_case.py` — parametric CadQuery source; every dimension is a named parameter at the top. `python3 pod_case.py out` regenerates the files; `drawings.py` and `render.py` make the figures

Coordinates (board frame): X 0–52 left→right as held (Pico USB on the right), Y 0–109 bottom→top (Pico end at the bottom, jack at the top), Z from the outside of the floor.
