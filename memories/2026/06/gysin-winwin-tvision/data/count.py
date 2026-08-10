import subprocess, glob, os, numpy as np
from PIL import Image
try:
    from scipy import ndimage
    HAVE=True
except Exception:
    HAVE=False
# extract 0.1s frames (10fps)
os.makedirs('f10',exist_ok=True)
subprocess.run(['ffmpeg','-nostdin','-loglevel','error','-y','-i','winwin.mp4','-vf','fps=10,scale=240:240','f10/f_%03d.png'],check=True)
files=sorted(glob.glob('f10/f_*.png'))
print(f"frames={len(files)} scipy={HAVE}")
rows=[]
for i,fp in enumerate(files):
    a=np.asarray(Image.open(fp).convert('RGB')).astype(int)
    R,G,B=a[...,0],a[...,1],a[...,2]
    red=(R>90)&(G<70)&(B<70)
    if HAVE:
        lbl,n=ndimage.label(red)
        # filter tiny specks
        sizes=ndimage.sum(np.ones_like(lbl),lbl,range(1,n+1))
        n=int((sizes>20).sum())
    else:
        n=-1
    redfrac=red.mean()
    rows.append((i/10.0,n,round(float(redfrac),3)))
# print compact table every 0.5s
print("t(s)  windows  redfrac")
for t,n,rf in rows:
    if abs((t*10)%5)<1e-6:
        print(f"{t:4.1f}   {n:4d}    {rf}")
# also print where count peaks
mx=max(rows,key=lambda r:r[1])
print("PEAK count:",mx)
