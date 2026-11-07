# genera la "mappa" di FUZZ INCREASE: arco 270° che si spezza verso il 10
import math
W,H=560,430; cx,cy=280,236; R=150
def pt(a,r): # a in gradi, 0 = in alto, orario
    t=math.radians(a); return cx+r*math.sin(t), cy-r*math.cos(t)
a0,a1=-135,135
out=[f'<svg viewBox="0 0 {W} {H}" xmlns="http://www.w3.org/2000/svg" class="arcmap">']
# segmenti: pieni a sinistra, sempre più spezzati a destra
n=150
for i in range(n):
    f=i/n; f2=(i+1)/n
    gap=0 if f<0.32 else (f-0.32)/0.68
    if gap>0.2 and (i*7919)%13 < 13*gap*0.45: continue   # crepitio: salti irregolari
    aa=a0+(a1-a0)*f; ab=a0+(a1-a0)*f2-(0.3+1.1*gap)
    if ab<=aa: continue
    w=22 - 10*f
    x1,y1=pt(aa,R); x2,y2=pt(ab,R)
    col='#2B1231' if f<0.3 else ('#8E3C94' if f<0.7 else '#B04CB8')
    out.append(f'<path d="M{x1:.1f},{y1:.1f} A{R},{R} 0 0 1 {x2:.1f},{y2:.1f}" stroke="{col}" stroke-width="{w:.1f}" fill="none"/>')
# tacche e numeri
for k in range(11):
    a=a0+(a1-a0)*k/10
    x1,y1=pt(a,R+18); x2,y2=pt(a,R+26); tx,ty=pt(a,R+42)
    out.append(f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" stroke="#2B1231" stroke-width="1.4"/>')
    out.append(f'<text x="{tx:.1f}" y="{ty+4:.1f}" class="tick">{k}</text>')
# manopola
out.append(f'<circle cx="{cx}" cy="{cy+4}" r="78" fill="#2B1231" opacity=".18"/>')
out.append(f'<circle cx="{cx}" cy="{cy}" r="74" fill="#151216"/>')
out.append(f'<circle cx="{cx}" cy="{cy}" r="64" fill="#221d24"/>')
dx,dy=pt(a0+(a1-a0)*0.75,46)
out.append(f'<circle cx="{dx:.1f}" cy="{dy:.1f}" r="7" fill="#fff"/>')
out.append('</svg>')
open('arc.svg','w').write('\n'.join(out))
