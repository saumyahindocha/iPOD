# Pod case (Rev A)

Two printed parts: `pod_tray` (lower) and `pod_bezel` (upper), snap-fit, no visible screws.
Outer 57.2 × 114.2 × 22.7 mm. Openings for the headphone jack and USB-C charging (top end),
the Pico's micro-USB (right), microSD and power switch (left).

- `Pod_Case_RevA.pdf` — renders, dimensioned drawings, part placement (battery between perfboard and screen), print settings, checks, 1:1 bezel template
- `pod_tray.stl/.step`, `pod_bezel.stl/.step`, `pod_case_assembled.step` — print or open in Fusion 360
- `pod_case.py` — parametric CadQuery source; every dimension is a named parameter at the top. `python3 pod_case.py out` regenerates the files; `drawings.py` and `render.py` make the figures

Coordinates: X 0–52 left→right as held (Pico USB on the right), Y 0–109 bottom→top (Pico end at the bottom, jack at the top), Z from the outside of the floor.
