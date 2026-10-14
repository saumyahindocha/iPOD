import sys, numpy as np, trimesh, matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
out=sys.argv[1]
def render(meshes, elev, azim, name, title):
    fig=plt.figure(figsize=(9,7)); ax=fig.add_subplot(111, projection="3d")
    allv=[]
    light=np.array([0.3,-0.5,0.8]); light/=np.linalg.norm(light)
    for m,color in meshes:
        tri=m.vertices[m.faces]; n=m.face_normals
        shade=0.45+0.55*np.clip(n@light,0,1)
        cols=np.array([color])*shade[:,None]; cols=np.clip(cols,0,1)
        pc=Poly3DCollection(tri, facecolors=np.c_[cols,np.ones(len(cols))], edgecolor="none")
        ax.add_collection3d(pc); allv.append(m.vertices)
    v=np.vstack(allv); c=(v.max(0)+v.min(0))/2; r=(v.max(0)-v.min(0)).max()/2
    ax.set_xlim(c[0]-r,c[0]+r); ax.set_ylim(c[1]-r,c[1]+r); ax.set_zlim(c[2]-r,c[2]+r)
    ax.view_init(elev=elev, azim=azim); ax.set_proj_type("ortho"); ax.set_axis_off(); ax.set_title(title)
    fig.tight_layout(); fig.savefig(f"{out}/{name}.png", dpi=110); plt.close(fig)
tray=trimesh.load(f"{out}/pod_tray.stl"); bez=trimesh.load(f"{out}/pod_bezel.stl")
grey=(0.75,0.78,0.82); dark=(0.35,0.4,0.5)
render([(tray,grey)], 35, -50, "r_tray_iso", "Lower tray, from above (USB end bottom-right)")
render([(tray,grey)], 90, -90, "r_tray_top", "Lower tray, top view")
flip=bez.copy(); flip.apply_transform(trimesh.transformations.rotation_matrix(np.pi,[1,0,0]))
render([(flip,dark)], 35, -50, "r_bezel_under", "Upper bezel, turned over (inside)")
render([(bez,dark)], 35, -50, "r_bezel_iso", "Upper bezel, from above")
render([(tray,grey),(bez,dark)], 30, -50, "r_asm_iso", "Assembled")
render([(tray,grey),(bez,dark)], 0, 0, "r_asm_right", "Right side (Pico USB)")
render([(tray,grey),(bez,dark)], 0, 180, "r_asm_left", "Left side (microSD, power switch)")
render([(tray,grey),(bez,dark)], 0, 90, "r_asm_top_end", "Top end (headphone jack, USB-C charging)")
print("ok")
