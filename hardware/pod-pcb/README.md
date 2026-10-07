# Pod PCB (Rev A)

KiCad project for the Pod board. **The spec it follows is [docs/pcb-plan.md](../../docs/pcb-plan.md)**:
RP2350A + RM2 Bluetooth, PCM5102A DAC, BQ24074 USB-C charging with power path, MAX17048 fuel gauge,
TPS63802 3.3 V buck-boost, 16 MB flash + 8 MB PSRAM, sockets for the 2.8" ILI9341 screen module,
play/pause button and power/hold switch. 4 layers, 0.8 mm, 50 × 94 mm.

| File | What's in it |
|---|---|
| `pod-pcb.kicad_pro` | Project, with JLCPCB-friendly rules and net classes (Power, USB) |
| `pod-pcb.kicad_sch` | Overview sheet linking the five blocks |
| `power.kicad_sch` … `screen_controls.kicad_sch` | One sheet per block, each with its checklist printed on it |
| `pod-pcb.kicad_pcb` | 4-layer board: outline, module area, M3 holes, battery area, RM2 antenna keep-out |

**Workflow:** open the project in KiCad 9 (it upgrades these KiCad 7 files), draw one sheet to its
checklist, delete the checklist box, commit and push. Each sheet is reviewed against the spec before
the next one. Start with **Power**.

Before routing, measure the screen module (outline, hole centres, header positions) and update
the outline, the holes and the J3/J4 socket positions.
