#!/usr/bin/env python3
from __future__ import annotations
import hashlib
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
BG=(0,0,0); DARK=(13,22,32); DARK2=(25,37,50); MID=(60,78,94); LIGHT=(151,176,194); WHITE=(226,239,247); CYAN=(0,174,235); BLUE=(0,104,222); GREEN=(36,255,96); SKIN=(232,164,116); BROWN=(75,42,24); RED=(166,32,22)

def image(w,h): return [[BG for _ in range(w)] for _ in range(h)]
def put(im,x,y,c):
    if 0<=y<len(im) and 0<=x<len(im[0]): im[y][x]=c
def rect(im,x0,y0,x1,y1,c):
    for y in range(max(0,y0),min(len(im),y1)):
        for x in range(max(0,x0),min(len(im[0]),x1)): im[y][x]=c
def ellipse(im,cx,cy,rx,ry,c):
    for y in range(cy-ry,cy+ry+1):
        for x in range(cx-rx,cx+rx+1):
            if rx and ry and ((x-cx)/rx)**2+((y-cy)/ry)**2<=1: put(im,x,y,c)
def line(im,x0,y0,x1,y1,width,c):
    steps=max(abs(x1-x0),abs(y1-y0),1)
    for i in range(steps+1):
        x=round(x0+(x1-x0)*i/steps); y=round(y0+(y1-y0)*i/steps)
        rect(im,x-width//2,y-width//2,x+(width+1)//2,y+(width+1)//2,c)
def armor_body(im,cx,top,helmet=True,hair=None,gun=True,scale=1):
    if helmet:
        ellipse(im,cx,top+7*scale,7*scale,7*scale,LIGHT); rect(im,cx-6*scale,top+6*scale,cx+6*scale,top+11*scale,DARK2); rect(im,cx-5*scale,top+7*scale,cx+5*scale,top+9*scale,CYAN)
    else:
        ellipse(im,cx,top+7*scale,6*scale,7*scale,SKIN); ellipse(im,cx,top+3*scale,7*scale,4*scale,hair or BROWN)
    rect(im,cx-8*scale,top+14*scale,cx+8*scale,top+29*scale,DARK2); rect(im,cx-5*scale,top+15*scale,cx+5*scale,top+20*scale,MID); rect(im,cx-3*scale,top+16*scale,cx+3*scale,top+18*scale,CYAN)
    line(im,cx-7*scale,top+17*scale,cx-11*scale,top+31*scale,4*scale,DARK2); line(im,cx+7*scale,top+17*scale,cx+11*scale,top+31*scale,4*scale,DARK2)
    line(im,cx-4*scale,top+29*scale,cx-5*scale,top+48*scale,5*scale,DARK2); line(im,cx+4*scale,top+29*scale,cx+5*scale,top+48*scale,5*scale,DARK2)
    rect(im,cx-9*scale,top+47*scale,cx-2*scale,top+51*scale,MID); rect(im,cx+1*scale,top+47*scale,cx+9*scale,top+51*scale,MID)
    if gun:
        rect(im,cx+8*scale,top+23*scale,cx+22*scale,top+27*scale,DARK2); rect(im,cx+10*scale,top+24*scale,cx+21*scale,top+25*scale,GREEN)
def parts_sheet(im,offset=0,pixel=False):
    for i in range(4):
        ellipse(im,13+i*27,10,7,7,LIGHT); rect(im,7+i*27,9,19+i*27,13,DARK2); rect(im,9+i*27,10,18+i*27,12,CYAN)
        rect(im,5+i*29,22,22+i*29,35,DARK2); rect(im,9+i*29,24,18+i*29,28,MID); rect(im,11+i*29,24,16+i*29,26,CYAN)
    for i in range(6):
        rect(im,5+i*20,42,12+i*20,56,DARK2); rect(im,7+i*20,44,11+i*20,49,MID)
        rect(im,5+i*20,59,12+i*20,73,DARK2); rect(im,4+i*20,72,15+i*20,77,MID)
    for i,w in enumerate((25,20,16,12)):
        x=5+i*30; rect(im,x,80-w//5,x+w,84,DARK2); rect(im,x+2,81,x+w-2,82,GREEN)
def write_ppm(path,im):
    h=len(im); w=len(im[0]); lines=['P3',f'{w} {h}','255']
    for row in im: lines.append(' '.join(f'{r} {g} {b}' for r,g,b in row))
    data=('\n'.join(lines)+'\n').encode('ascii'); path.parent.mkdir(parents=True,exist_ok=True); path.write_bytes(data); return hashlib.sha256(data).hexdigest(),f'{w}x{h}'
def source_modular():
    im=image(128,85); armor_body(im,36,3,False,BROWN,True); armor_body(im,92,3,False,RED,True); return im
def source_humanoid():
    im=image(128,85); armor_body(im,27,2,False,BROWN,False); armor_body(im,85,2,False,(170,180,186),False); parts_sheet(im); return im
def source_helmeted():
    im=image(128,85); parts_sheet(im); armor_body(im,105,2,True,None,True); return im
def source_pixel():
    im=image(128,85); parts_sheet(im); armor_body(im,91,25,True,None,True); return im
def runtime_foot():
    im=image(64,40); rect(im,7,16,50,31,DARK2); rect(im,12,11,31,25,MID); rect(im,17,13,31,18,LIGHT); rect(im,35,18,57,26,DARK2); rect(im,43,19,58,22,GREEN); rect(im,7,29,54,34,MID); return im
def runtime_helmet():
    im=image(48,44); ellipse(im,24,21,18,18,LIGHT); rect(im,7,17,41,29,DARK2); rect(im,10,18,38,25,CYAN); rect(im,13,25,39,30,BLUE); rect(im,17,34,31,39,DARK2); return im
def runtime_torso():
    im=image(56,38); rect(im,5,8,51,34,DARK2); ellipse(im,10,10,9,8,MID); ellipse(im,46,10,9,8,MID); rect(im,14,10,42,27,MID); rect(im,19,12,37,18,LIGHT); rect(im,24,13,32,16,CYAN); rect(im,20,27,36,35,DARK); return im
def runtime_weapon():
    im=image(80,38); rect(im,5,11,69,27,DARK2); rect(im,13,7,60,13,MID); rect(im,20,9,59,11,LIGHT); rect(im,54,14,76,21,DARK); rect(im,64,15,78,18,GREEN); rect(im,24,25,37,36,DARK2); return im

def main():
    root=ROOT/'assets'/'optional'/'runner_armor_concepts'; hashes={}; dims={}
    files={
      'source/concept_modular_pair.ppm':source_modular(), 'source/concept_humanoid_parts.ppm':source_humanoid(),
      'source/concept_helmeted_parts.ppm':source_helmeted(), 'source/concept_pixel_parts.ppm':source_pixel(),
      'runtime/foot_side.ppm':runtime_foot(), 'runtime/helmet_side.ppm':runtime_helmet(),
      'runtime/torso_side.ppm':runtime_torso(), 'runtime/weapon_side.ppm':runtime_weapon(),
    }
    for rel,im in files.items(): hashes[rel],dims[rel]=write_ppm(root/rel,im)
    originals={
      'concept_modular_pair':'271ff41d9cd1c6df92a4e957238225e77ed2a69398288df8c7500b3342081b13',
      'concept_humanoid_parts':'98bd05f48fe59ea9a493ac4589713426115aba72e4f6d532583f448114b1589e',
      'concept_helmeted_parts':'b08513be54f60137458f9f58d9a9c466556315a2a296b298c78da0b3b977315b',
      'concept_pixel_parts':'74e9f5fd1598286fadb88921ff530e04d118068ff9812c8440acce906247af44'}
    lines=['# Runner optional armor concepts','', 'Four user-supplied concept sheets informed these compact side-view reference and runtime sprites. Optional art is visual only and never changes physics, collision, observations, rewards, or training.','', '## Original supplied captures','']
    for name,digest in originals.items(): lines.append(f'- `{name}` original 1536x1024 PNG SHA-256 `{digest}`')
    lines += ['', '## Packaged derived P3 files', '']
    for rel in sorted(hashes): lines.append(f'- `{rel}` — {dims[rel]} — SHA-256 `{hashes[rel]}`')
    lines += ['', 'Removing `assets/optional` preserves procedural rendering and all training behavior.', '']
    (root/'PROVENANCE.md').write_text('\n'.join(lines),encoding='utf-8',newline='\n')
    print('Runner v0.7.17 optional art materialized')
    for rel in sorted(hashes): print(rel,dims[rel],hashes[rel])
if __name__=='__main__': raise SystemExit(main())
