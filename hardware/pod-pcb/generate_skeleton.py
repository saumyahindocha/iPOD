import uuid, re, json, os
OUT='/home/claude/ipod/hardware/pod-pcb'
U=lambda: str(uuid.uuid4())
doc=open('/home/claude/ipod/docs/pcb-plan.md').read()

def section(title_prefix):
    i=doc.index('## '+title_prefix); j=doc.find('\n## ',i+3); j=len(doc) if j<0 else j
    body=doc[i:j].split('\n',1)[1].strip()
    # markdown -> plain text for the sheet
    body=body.replace('**','').replace('`','').replace('*','')
    lines=[]
    for ln in body.split('\n'):
        if re.match(r'^\|[-| ]+\|$', ln.strip()): continue
        if ln.startswith('|'):
            cells=[c.strip() for c in ln.strip().strip('|').split('|')]
            ln='   '.join(c for c in cells if c)
        lines.append(ln)
    return '\n'.join(lines)

def esc(s): return s.replace('\\','\\\\').replace('"','\\"').replace('\n','\\n')

TB=lambda title, sub: f'''  (title_block
    (title "{esc(title)}")
    (date "2026-10-07")
    (rev "A")
    (company "Saumya Hindocha")
    (comment 1 "{esc(sub)}")
    (comment 2 "Spec: docs/pcb-plan.md")
  )'''

sheets=[('Power','power.kicad_sch','Sheet 1 — Power'),
        ('MCU','mcu.kicad_sch','Sheet 2 — MCU'),
        ('Radio','radio.kicad_sch','Sheet 3 — Radio'),
        ('Audio','audio.kicad_sch','Sheet 4 — Audio'),
        ('Screen and controls','screen_controls.kicad_sch','Sheet 5 — Screen sockets')]

root_uuid=U()
for name,fn,sec in sheets:
    text=section(sec)
    sub=f'''(kicad_sch (version 20230121) (generator eeschema)
  (uuid {U()})
  (paper "A3")
{TB('Pod Rev A — '+name, 'Draw this sheet to the checklist below, then delete the checklist box.')}
  (lib_symbols)
  (text_box "{esc('CHECKLIST — '+sec+chr(10)+chr(10)+text)}"
    (at 15 20 0) (size 390 255)
    (stroke (width 0) (type default))
    (fill (type none))
    (effects (font (size 1.4 1.4)) (justify left top))
    (uuid {U()})
  )
)
'''
    open(os.path.join(OUT,fn),'w').write(sub)

# root
sx,sy=30,60
blocks=[]
for k,(name,fn,sec) in enumerate(sheets):
    x=sx+(k%3)*120; y=sy+(k//3)*70
    blocks.append(f'''  (sheet (at {x} {y}) (size 90 45) (fields_autoplaced)
    (stroke (width 0.1524) (type solid))
    (fill (color 0 0 0 0.0000))
    (uuid {U()})
    (property "Sheetname" "{esc(name)}" (at {x} {y-0.7:.2f} 0)
      (effects (font (size 1.6 1.6)) (justify left bottom))
    )
    (property "Sheetfile" "{fn}" (at {x} {y+45.6:.2f} 0)
      (effects (font (size 1.27 1.27)) (justify left top))
    )
    (instances
      (project "pod-pcb"
        (path "/{root_uuid}" (page "{k+2}"))
      )
    )
  )''')
overview='''Pod Rev A — RP2350A + RM2 Bluetooth, PCM5102A DAC, BQ24074 USB-C charging, MAX17048 fuel gauge,
TPS63802 3.3 V buck-boost, 16 MB flash + 8 MB PSRAM, sockets for the 2.8" ILI9341 screen module,
play/pause button and power/hold switch. 4 layers, 0.8 mm, 50 x 94 mm.

Open each sheet below (double-click) and draw it to the checklist printed on it.
Order: Power -> MCU -> Radio -> Audio -> Screen and controls. Push each finished sheet for review.
Global net names (VBUS, VSYS, VBAT, 3V3, GND, SPI0_SCK ...) connect the sheets: use global labels.'''
root=f'''(kicad_sch (version 20230121) (generator eeschema)
  (uuid {root_uuid})
  (paper "A3")
{TB('Pod Rev A — overview','Hierarchical design: one sheet per block')}
  (lib_symbols)
  (text_box "{esc(overview)}"
    (at 30 22 0) (size 340 28)
    (stroke (width 0) (type default))
    (fill (type none))
    (effects (font (size 1.6 1.6)) (justify left top))
    (uuid {U()})
  )
{chr(10).join(blocks)}
  (sheet_instances
    (path "/" (page "1"))
  )
)
'''
open(os.path.join(OUT,'pod-pcb.kicad_sch'),'w').write(root)

# ---------------- PCB ----------------
X0,Y0,W,H,R=100.0,50.0,50.0,94.0,3.0
TAB=8.0
def line(a,b,layer='Edge.Cuts',w=0.1): return f'  (gr_line (start {a[0]} {a[1]}) (end {b[0]} {b[1]}) (stroke (width {w}) (type default)) (layer "{layer}") (tstamp {U()}))'
import math
def arc(c,start_ang):
    pts=[]
    for a in (start_ang, start_ang+45, start_ang+90):
        r=math.radians(a); pts.append((round(c[0]+R*math.cos(r),4), round(c[1]+R*math.sin(r),4)))
    return f'  (gr_arc (start {pts[0][0]} {pts[0][1]}) (mid {pts[1][0]} {pts[1][1]}) (end {pts[2][0]} {pts[2][1]}) (stroke (width 0.1) (type default)) (layer "Edge.Cuts") (tstamp {U()}))'
x1,y1=X0+W,Y0+H
edge=[line((X0+R,Y0),(x1-R,Y0)), line((x1,Y0+R),(x1,y1-R)), line((x1-R,y1),(X0+R,y1)), line((X0,y1-R),(X0,Y0+R)),
      arc((x1-R,Y0+R),270), arc((x1-R,y1-R),0), arc((X0+R,y1-R),90), arc((X0+R,Y0+R),180)]
def rect(a,b,layer,w=0.12): return f'  (gr_rect (start {a[0]} {a[1]}) (end {b[0]} {b[1]}) (stroke (width {w}) (type default)) (fill none) (layer "{layer}") (tstamp {U()}))'
def text(s,at,layer='Cmts.User',size=1.0,just='left'):
    return f'  (gr_text "{esc(s)}" (at {at[0]} {at[1]}) (layer "{layer}") (tstamp {U()})\n    (effects (font (size {size} {size}) (thickness 0.15)) (justify {just}))\n  )'
my=Y0+TAB
# ---- Screen module, measured from the photo of the module's back (2026-10-07).
# Back-view coordinates (bx, by) in mm from the module's top-left corner, header along the top.
# The module sits screen-up on our board, so its back faces us mirrored: x_pcb = X0 + 50 - bx.
MOD_W, MOD_H = 50.0, 86.0
HOLES_B = [(2.7, 6.9), (47.3, 6.9), (2.7, 83.0), (47.3, 83.0)]
J3_PIN1_B = (6.0, 2.0)          # VCC; pins continue in +bx at 2.54 mm
J4_PIN1_B = (21.19, 82.9)       # SD_CS; header is centred on the module
SD_HOLDER_B = (0.0, 32.5, 18.0, 60.8)    # card holder on the module's back; card enters from its left edge
def P(bx, by): return (round(X0 + MOD_W - bx, 3), round(my + by, 3))
j3_names = ["VCC","GND","CS","RESET","DC","SDI","SCK","LED","SDO","T_CLK","T_CS","T_DIN","T_DO","T_IRQ"]
j4_names = ["SD_CS","SD_MOSI","SD_MISO","SD_SCK"]
def pin_marker(xy, n):
    x, y = xy
    out = ['  (gr_circle (center %s %s) (end %s %s) (stroke (width 0.15) (type default)) (fill none) (layer "Dwgs.User") (tstamp %s))' % (x, y, round(x + 0.85, 3), y, U())]
    if n == 1:
        out.append('  (gr_rect (start %s %s) (end %s %s) (stroke (width 0.1) (type default)) (fill none) (layer "Dwgs.User") (tstamp %s))' % (round(x-1.2,3), round(y-1.2,3), round(x+1.2,3), round(y+1.2,3), U()))
    return out
draw = [rect((X0, my), (x1, y1), 'Dwgs.User'),
        text('2.8" module MSP2807 V1.2, 50 x 86 mm (from photo: verify with the print template)', (X0 + 1.0, my + 8.5), size=0.7)]
for k in range(14): draw += pin_marker(P(J3_PIN1_B[0] + 2.54 * k, J3_PIN1_B[1]), k + 1)
for k in range(4):  draw += pin_marker(P(J4_PIN1_B[0] + 2.54 * k, J4_PIN1_B[1]), k + 1)
px, py = P(*J3_PIN1_B); draw.append(text('J3 pin 1 VCC', (round(px - 4.5,3), round(py + 2.3,3)), size=0.7))
px, py = P(*J4_PIN1_B); draw.append(text('J4 pin 1 SD_CS', (round(px - 6.5,3), round(py - 2.0,3)), size=0.7))
hx0, hy0 = P(SD_HOLDER_B[2], SD_HOLDER_B[1]); hx1, hy1 = P(SD_HOLDER_B[0], SD_HOLDER_B[3])
draw += [rect((hx0, hy0), (hx1, hy1), 'Dwgs.User'),
         text('module SD holder above: keep clear', (round(hx0 + 0.5,3), round(hy0 + 2.0,3)), size=0.6),
         text('card enters from this edge ->', (round(hx0 + 0.5,3), round(hy0 + 4.0,3)), size=0.6),
         text('RM2 antenna tab: no copper, any layer', (X0 + 1.5, Y0 + 2.5)),
         text('USB-C', (X0 + 9, y1 - 1.2), size=0.7),
         text('jack', (X0 + 38, y1 - 1.2), size=0.7),
         text('<- SW1, SW2 on this edge', (X0 + 1.0, my + 66), size=0.7),
         '  (gr_line (start %s %s) (end %s %s) (stroke (width 0.3) (type default)) (layer "Dwgs.User") (tstamp %s))' % (X0, y1 + 6, X0 + 50, y1 + 6, U()),
         text('this bar must measure exactly 50 mm on paper', (X0, y1 + 8.5), size=1.0)]
keepout = ('  (zone (net 0) (net_name "") (layers "*.Cu") (tstamp %s) (name "RM2 antenna keep-out") (hatch edge 0.5)\n'
           '    (connect_pads (clearance 0))\n    (min_thickness 0.25) (filled_areas_thickness no)\n'
           '    (keepout (tracks not_allowed) (vias not_allowed) (pads not_allowed) (copperpour not_allowed) (footprints allowed))\n'
           '    (fill (thermal_gap 0.5) (thermal_bridge_width 0.5))\n'
           '    (polygon\n      (pts\n        (xy %s %s) (xy %s %s) (xy %s %s) (xy %s %s)\n      )\n    )\n  )') % (
           U(), X0+14, Y0+0.5, x1-14, Y0+0.5, x1-14, Y0+5.5, X0+14, Y0+5.5)
holes = []
for n, hb in enumerate(HOLES_B):
    hx, hy = P(*hb)
    holes.append(('  (footprint "MountingHole:MountingHole_2.7mm_M2.5" (layer "F.Cu")\n    (tstamp %s)\n    (at %s %s)\n'
                  '    (attr exclude_from_pos_files exclude_from_bom)\n'
                  '    (fp_text reference "H%d" (at 0 -2.6) (layer "F.SilkS") hide\n      (effects (font (size 1 1) (thickness 0.15)))\n      (tstamp %s)\n    )\n'
                  '    (fp_text value "M2.5 module hole" (at 0 2.6) (layer "F.Fab")\n      (effects (font (size 0.7 0.7) (thickness 0.1)))\n      (tstamp %s)\n    )\n'
                  '    (fp_circle (center 0 0) (end 2.5 0) (stroke (width 0.15) (type solid)) (fill none) (layer "Cmts.User") (tstamp %s))\n'
                  '    (pad "" np_thru_hole circle (at 0 0) (size 2.7 2.7) (drill 2.7) (layers "*.Cu" "*.Mask") (tstamp %s))\n  )') % (U(), hx, hy, n+1, U(), U(), U(), U()))

pcb=f'''(kicad_pcb (version 20221018) (generator pcbnew)
  (general (thickness 0.8))
  (paper "A4")
  (title_block (title "Pod Rev A") (date "2026-10-07") (rev "A") (company "Saumya Hindocha") (comment 1 "Spec: docs/pcb-plan.md"))
  (layers
    (0 "F.Cu" signal)
    (1 "In1.Cu" power "GND")
    (2 "In2.Cu" power "PWR")
    (31 "B.Cu" signal)
    (32 "B.Adhes" user "B.Adhesive")
    (33 "F.Adhes" user "F.Adhesive")
    (34 "B.Paste" user)
    (35 "F.Paste" user)
    (36 "B.SilkS" user "B.Silkscreen")
    (37 "F.SilkS" user "F.Silkscreen")
    (38 "B.Mask" user)
    (39 "F.Mask" user)
    (40 "Dwgs.User" user "User.Drawings")
    (41 "Cmts.User" user "User.Comments")
    (42 "Eco1.User" user "User.Eco1")
    (43 "Eco2.User" user "User.Eco2")
    (44 "Edge.Cuts" user)
    (45 "Margin" user)
    (46 "B.CrtYd" user "B.Courtyard")
    (47 "F.CrtYd" user "F.Courtyard")
    (48 "B.Fab" user)
    (49 "F.Fab" user)
  )
  (setup
    (stackup
      (layer "F.SilkS" (type "Top Silk Screen"))
      (layer "F.Paste" (type "Top Solder Paste"))
      (layer "F.Mask" (type "Top Solder Mask") (thickness 0.01))
      (layer "F.Cu" (type "copper") (thickness 0.035))
      (layer "dielectric 1" (type "prepreg") (thickness 0.1) (material "FR4") (epsilon_r 4.5) (loss_tangent 0.02))
      (layer "In1.Cu" (type "copper") (thickness 0.0152))
      (layer "dielectric 2" (type "core") (thickness 0.4) (material "FR4") (epsilon_r 4.5) (loss_tangent 0.02))
      (layer "In2.Cu" (type "copper") (thickness 0.0152))
      (layer "dielectric 3" (type "prepreg") (thickness 0.1) (material "FR4") (epsilon_r 4.5) (loss_tangent 0.02))
      (layer "B.Cu" (type "copper") (thickness 0.035))
      (layer "B.Mask" (type "Bottom Solder Mask") (thickness 0.01))
      (layer "B.Paste" (type "Bottom Solder Paste"))
      (layer "B.SilkS" (type "Bottom Silk Screen"))
      (copper_finish "ENIG")
      (dielectric_constraints no)
    )
    (pad_to_mask_clearance 0)
    (pcbplotparams
      (layerselection 0x00010fc_ffffffff)
      (outputformat 1)
      (outputdirectory "gerbers/")
    )
  )
  (net 0 "")
{chr(10).join(holes)}
{chr(10).join(edge)}
{chr(10).join(draw)}
{keepout}
)
'''
open(os.path.join(OUT,'pod-pcb.kicad_pcb'),'w').write(pcb)

pro={"meta":{"filename":"pod-pcb.kicad_pro","version":1},
 "board":{"design_settings":{
   "defaults":{"board_outline_line_width":0.1,"copper_line_width":0.15,"silk_line_width":0.12},
   "rules":{"min_clearance":0.1,"min_track_width":0.1,"min_via_diameter":0.3,"min_via_annular_width":0.075,
            "min_through_hole_diameter":0.15,"min_hole_to_hole":0.25,"min_copper_edge_clearance":0.3,
            "min_silk_clearance":0.0,"min_microvia_diameter":0.2,"min_microvia_drill":0.1},
   "track_widths":[0.0,0.15,0.2,0.3,0.5,0.8],
   "via_dimensions":[{"diameter":0.0,"drill":0.0},{"diameter":0.4,"drill":0.2},{"diameter":0.6,"drill":0.3}],
   "diff_pair_dimensions":[{"gap":0.0,"via_gap":0.0,"width":0.0},{"gap":0.127,"via_gap":0.25,"width":0.2}]}},
 "net_settings":{"classes":[
   {"name":"Default","clearance":0.15,"track_width":0.15,"via_diameter":0.4,"via_drill":0.2,"diff_pair_gap":0.127,"diff_pair_width":0.2,"wire_width":6,"bus_width":12,"line_style":0,"pcb_color":"rgba(0, 0, 0, 0.000)","schematic_color":"rgba(0, 0, 0, 0.000)","microvia_diameter":0.3,"microvia_drill":0.1,"diff_pair_via_gap":0.25},
   {"name":"Power","clearance":0.15,"track_width":0.4,"via_diameter":0.6,"via_drill":0.3,"diff_pair_gap":0.127,"diff_pair_width":0.2,"wire_width":6,"bus_width":12,"line_style":0,"pcb_color":"rgba(0, 0, 0, 0.000)","schematic_color":"rgba(0, 0, 0, 0.000)","microvia_diameter":0.3,"microvia_drill":0.1,"diff_pair_via_gap":0.25},
   {"name":"USB","clearance":0.15,"track_width":0.2,"via_diameter":0.4,"via_drill":0.2,"diff_pair_gap":0.127,"diff_pair_width":0.2,"wire_width":6,"bus_width":12,"line_style":0,"pcb_color":"rgba(0, 0, 0, 0.000)","schematic_color":"rgba(0, 0, 0, 0.000)","microvia_diameter":0.3,"microvia_drill":0.1,"diff_pair_via_gap":0.25}],
  "meta":{"version":3},
  "netclass_patterns":[{"netclass":"Power","pattern":"VBUS"},{"netclass":"Power","pattern":"VSYS"},{"netclass":"Power","pattern":"VBAT"},{"netclass":"Power","pattern":"3V3"},{"netclass":"USB","pattern":"USB_D*"}]},
 "sheets":[[root_uuid,"Root"]]+[[None,n] for n,_,_ in sheets],
 "text_variables":{}}
pro["sheets"]=[[root_uuid,"Root"]]
json.dump(pro,open(os.path.join(OUT,'pod-pcb.kicad_pro'),'w'),indent=2)
print("ok")
