# hardware

| Folder | What's in it | Status |
|---|---|---|
| `perfboard/` | The build in progress: the Pico 2 W, screen, DAC and TP4056 charger on a 20 × 43-hole perfboard. The full guide (solder view, hole-by-hole map, front view, off-board wires, shopping list) is [`../docs/perfboard_build.pdf`](../docs/perfboard_build.pdf); `dac_wiring.pdf` is the stand-alone DAC sheet; `tools/` has the scripts that draw them | **Building** |
| `pod-pcb-easyeda/` | Rev A custom PCB drawn in EasyEDA Pro: 4-page schematic, assembly drawing, 1:1 fit test, and `fab/` with the Gerbers (outline fixed), BOM and pick-and-place, ready for JLCPCB | Designed, not ordered (about $90 assembled) |
| `pod-pcb/` | Early KiCad skeleton for a bare-RP2350 board | Superseded |

The design spec for the PCB is [`../docs/pcb-plan.md`](../docs/pcb-plan.md); the EasyEDA walkthrough is [`../docs/easyeda-guide.md`](../docs/easyeda-guide.md).
Pin assignments for both builds live in [`../sdk/common/pod_pins.h`](../sdk/common/pod_pins.h).
