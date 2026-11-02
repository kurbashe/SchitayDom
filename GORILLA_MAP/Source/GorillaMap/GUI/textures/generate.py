#!/usr/bin/env python3
"""Generate original GORILLA MAP artwork and DXT5 PAA textures.

Requires Pillow >= 11.2 with the DDS DXT5 encoder. The PAA container layout is
cross-checked against https://github.com/KoffeinFlummi/armake/src/img2paa.c.
No third-party artwork is used. This file is a development helper, not a game asset.
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter, ImageFont, ImageChops, ImageStat
import io
import math
import struct

ROOT = Path(__file__).resolve().parent
SCALE = 3


def write_paa(name, image):
    """Wrap independently encoded DXT5 mip levels in a PAA container."""
    image = image.convert('RGBA')
    image.save(ROOT / (name + '.png'))
    avg = tuple(round(x) for x in ImageStat.Stat(image).mean)
    maxc = tuple(x[1] for x in image.getextrema())
    # PAA uses BGRA colour hints; texture data is standard DXT5 (BC3).
    def bgra(c): return bytes((c[2], c[1], c[0], c[3]))
    out = bytearray(struct.pack('<H', 0xFF05))
    out += b'GGATCGVA' + struct.pack('<I', 4) + bgra(avg)
    out += b'GGATCXAM' + struct.pack('<I', 4) + bgra(maxc)
    out += b'GGATSFFO' + struct.pack('<I', 64)
    offset_table = len(out)
    out += bytes(64)
    out += b'\x00\x00'  # no indexed-colour palette
    offsets = []
    mip = image
    while min(mip.size) >= 4:
        encoded = io.BytesIO()
        mip.save(encoded, format='DDS', pixel_format='DXT5')
        dds = encoded.getvalue()
        assert dds[:4] == b'DDS ' and dds[84:88] == b'DXT5'
        payload = dds[128:]
        width, height = mip.size
        assert len(payload) == width * height
        offsets.append(len(out))
        out += struct.pack('<HH', width, height)
        out += len(payload).to_bytes(3, 'little')
        out += payload
        mip = mip.resize((width // 2, height // 2), Image.Resampling.LANCZOS)
    offsets.append(len(out))
    out += bytes(6)  # end of mip chain / palette terminator
    for i, offset in enumerate(offsets):
        struct.pack_into('<I', out, offset_table + i * 4, offset)
    (ROOT / (name + '.paa')).write_bytes(out)


def rounded_surface(size, accent=None, radius=24, primary=False):
    w,h = size
    high = Image.new('RGBA', (w*SCALE,h*SCALE))
    mask = Image.new('L', high.size)
    md = ImageDraw.Draw(mask)
    md.rounded_rectangle((1*SCALE,1*SCALE,(w-2)*SCALE,(h-2)*SCALE), radius=radius*SCALE, fill=255)
    pix = high.load()
    for y in range(h*SCALE):
        yn=y/(h*SCALE)
        for x in range(w*SCALE):
            xn=x/(w*SCALE)
            if primary:
                pix[x,y]=(int(5+4*xn),int(122-61*xn-24*yn),int(255-57*xn-45*yn),250)
            else:
                edge=max(0,1-math.hypot(xn*1.7,yn*2))
                pix[x,y]=(int(5+13*edge),int(16+22*edge),int(29+31*edge),int(225+18*yn))
    high.putalpha(ImageChops.multiply(high.getchannel('A'),mask))
    # Broad, low-opacity reflected light in the upper left; no baked-in text.
    sheen=Image.new('RGBA',high.size)
    sd=ImageDraw.Draw(sheen)
    sd.ellipse((-w*SCALE*.55,-h*SCALE*.48,w*SCALE*.68,h*SCALE*.52),fill=(132,183,219,32 if not primary else 8))
    sheen=sheen.filter(ImageFilter.GaussianBlur(max(8,int(min(w,h)*SCALE*.09))))
    sheen.putalpha(ImageChops.multiply(sheen.getchannel('A'),mask))
    high=Image.alpha_composite(high,sheen)
    draw=ImageDraw.Draw(high)
    border=(75,113,145,150) if not primary else (49,163,255,230)
    draw.rounded_rectangle((1*SCALE,1*SCALE,(w-2)*SCALE,(h-2)*SCALE),radius=radius*SCALE,outline=border,width=SCALE)
    if accent:
        glow=Image.new('RGBA',high.size)
        gd=ImageDraw.Draw(glow)
        gd.line((6*SCALE,18*SCALE,6*SCALE,(h-18)*SCALE),fill=accent+(190,),width=7*SCALE)
        glow=glow.filter(ImageFilter.GaussianBlur(12*SCALE))
        glow.putalpha(ImageChops.multiply(glow.getchannel('A'),mask))
        high=Image.alpha_composite(high,glow)
        draw=ImageDraw.Draw(high)
        draw.line((3*SCALE,19*SCALE,3*SCALE,(h-19)*SCALE),fill=accent+(230,),width=2*SCALE)
    return high.resize(size,Image.Resampling.LANCZOS)


def symbol(kind, size=128, ring=False):
    im=Image.new('RGBA',(size*SCALE,size*SCALE))
    d=ImageDraw.Draw(im)
    s=size*SCALE/100
    def line(points,width=5,fill=(255,255,255,255)):
        d.line([(int(x*s),int(y*s)) for x,y in points],fill=fill,width=max(1,int(width*s)),joint='curve')
    def ellipse(box,width=5,fill=None):
        d.ellipse(tuple(int(v*s) for v in box),outline=(255,255,255,255),width=max(1,int(width*s)),fill=fill)
    def poly(points,fill=(255,255,255,255)):
        d.polygon([(int(x*s),int(y*s)) for x,y in points],fill=fill)
    if ring:
        ellipse((7,7,93,93),width=5)
    if kind=='pin':
        ellipse((29,17,71,59),5)
        poly(((30,45),(50,82),(70,45)))
        ellipse((42,30,58,46),0,fill=(0,0,0,0))
    elif kind=='home':
        line(((21,48),(50,22),(79,48)),7)
        line(((30,44),(30,76),(44,76),(44,58),(58,58),(58,76),(71,76),(71,44)),6)
    elif kind=='skull':
        ellipse((28,23,72,65),4,fill=(255,255,255,255))
        d.rectangle(tuple(int(v*s) for v in (38,58,62,75)),fill=(255,255,255,255))
        for b in ((35,36,46,48),(54,36,65,48)):
            d.ellipse(tuple(int(v*s) for v in b),fill=(0,0,0,0))
        poly(((50,49),(45,56),(55,56)),fill=(0,0,0,0))
        for x in (44,51,58):line(((x,65),(x,76)),2,fill=(0,0,0,0))
    elif kind=='star':
        p=[]
        for i in range(10):
            a=-math.pi/2+i*math.pi/5;r=31 if i%2==0 else 14
            p.append((50+math.cos(a)*r,50+math.sin(a)*r))
        poly(p)
    elif kind=='water':
        poly(((50,18),(30,50),(31,67),(41,78),(58,78),(70,67),(70,52)))
        line(((39,57),(38,64),(43,69)),3,fill=(0,0,0,0))
    elif kind=='medical':
        poly(((42,22),(58,22),(58,42),(78,42),(78,58),(58,58),(58,78),(42,78),(42,58),(22,58),(22,42),(42,42)))
    elif kind=='camp':
        line(((22,76),(50,24),(78,76),(22,76)),5)
        line(((50,24),(50,76)),4)
        line(((50,46),(63,76)),4)
    elif kind=='radiation':
        ellipse((42,42,58,58),1,fill=(255,255,255,255))
        for a in (-90,30,150):
            pts=[]
            for radius,angles in ((34,range(a-26,a+27,3)),(16,range(a+26,a-27,-3))):
                pts.extend((50+radius*math.cos(math.radians(t)),50+radius*math.sin(math.radians(t))) for t in angles)
            poly(pts)
    elif kind=='prohibited':
        ellipse((24,24,76,76),6)
        line(((31,69),(69,31)),6)
    elif kind=='person':
        ellipse((38,21,62,45),5)
        d.arc(tuple(int(v*s) for v in (25,48,75,94)),180,360,fill='white',width=int(5*s))
    elif kind=='group':
        ellipse((38,20,62,44),4)
        ellipse((15,32,33,50),3)
        ellipse((67,32,85,50),3)
        d.arc(tuple(int(v*s) for v in (30,49,70,91)),180,360,fill='white',width=int(5*s))
        d.arc(tuple(int(v*s) for v in (8,55,39,87)),180,300,fill='white',width=int(4*s))
        d.arc(tuple(int(v*s) for v in (61,55,92,87)),240,360,fill='white',width=int(4*s))
    elif kind=='eye':
        line(((16,50),(25,38),(40,31),(60,31),(75,38),(84,50),(75,62),(60,69),(40,69),(25,62),(16,50)),5)
        ellipse((40,40,60,60),5)
    elif kind=='trash':
        line(((28,30),(72,30)),5)
        line(((40,28),(40,21),(60,21),(60,28)),4)
        line(((33,35),(37,78),(63,78),(67,35)),4)
        line(((45,40),(46,69)),3);line(((56,40),(55,69)),3)
    elif kind=='gear':
        for i in range(8):
            a=i*math.pi/4
            line(((50+25*math.cos(a),50+25*math.sin(a)),(50+34*math.cos(a),50+34*math.sin(a))),10)
        ellipse((23,23,77,77),7);ellipse((40,40,60,60),5)
    elif kind=='map':
        line(((18,28),(39,21),(61,30),(82,22),(82,73),(61,81),(39,72),(18,80),(18,28)),5)
        line(((39,21),(39,72)),4);line(((61,30),(61,81)),4)
    elif kind=='plus':
        line(((50,25),(50,75)),5);line(((25,50),(75,50)),5)
    elif kind=='close':
        line(((30,30),(70,70)),5);line(((70,30),(30,70)),5)
    elif kind=='chevron':
        line(((59,28),(37,50),(59,72)),5)
    elif kind=='target':
        ellipse((28,28,72,72),4);ellipse((41,41,59,59),4)
        for a in range(0,360,90):
            ar=math.radians(a);line(((50+26*math.cos(ar),50+26*math.sin(ar)),(50+37*math.cos(ar),50+37*math.sin(ar))),4)
    elif kind=='globe':
        ellipse((20,20,80,80),4);ellipse((35,20,65,80),3)
        line(((22,40),(78,40)),3);line(((22,60),(78,60)),3)
    elif kind=='menu':
        for y in (30,50,70):line(((25,y),(75,y)),5)
    return im.resize((size,size),Image.Resampling.LANCZOS)


def main():
    write_paa('panel', rounded_surface((256,512),radius=20))
    write_paa('panel_modal', rounded_surface((512,512),radius=24))
    for name,rgb in [('blue',(0,149,255)),('yellow',(255,203,43)),('red',(255,48,80)),('green',(0,231,150)),('purple',(182,96,255))]:
        write_paa('card_'+name,rounded_surface((512,128),rgb,radius=20))
    write_paa('button_blue',rounded_surface((512,64),radius=20,primary=True))
    write_paa('button_square',rounded_surface((128,128),radius=25))
    write_paa('button_square_active',rounded_surface((128,128),(0,149,255),radius=25))
    for kind in ('pin','home','skull','star','water','medical','camp','radiation','prohibited','person','group'):
        write_paa('marker_'+kind+'_ca',symbol(kind,128,True))
    for kind in ('eye','trash','gear','map','plus','close','chevron','target','globe','person','group','menu','star','prohibited'):
        write_paa('icon_'+kind+'_ca',symbol(kind,128,False))
    # Brand symbol: an original folded-map/locator glyph on a blue glass tile.
    logo=rounded_surface((128,128),(0,149,255),radius=28,primary=True)
    glyph=symbol('map',128)
    logo=Image.alpha_composite(logo,glyph)
    write_paa('logo',logo)
    compass=Image.new('RGBA',(128*SCALE,128*SCALE))
    d=ImageDraw.Draw(compass)
    d.ellipse((4*SCALE,4*SCALE,124*SCALE,124*SCALE),fill=(9,23,39,225),outline=(84,122,151,190),width=SCALE)
    d.polygon([(64*SCALE,25*SCALE),(46*SCALE,76*SCALE),(64*SCALE,64*SCALE),(82*SCALE,76*SCALE)],fill=(227,241,255,255))
    d.polygon([(64*SCALE,25*SCALE),(64*SCALE,64*SCALE),(82*SCALE,76*SCALE)],fill=(151,178,205,255))
    font=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',14*SCALE)
    d.text((64*SCALE,92*SCALE),'N',font=font,fill=(185,211,235,255),anchor='mm')
    write_paa('compass',compass.resize((128,128),Image.Resampling.LANCZOS))


def verify():
    """Check every mip offset/payload and decode first mip using Pillow's DDS reader."""
    for path in sorted(ROOT.glob('*.paa')):
        data=path.read_bytes()
        assert struct.unpack_from('<H',data)[0]==0xFF05
        p=2
        offsets=[]
        while data[p:p+4]==b'GGAT':
            kind=data[p+4:p+8];n=struct.unpack_from('<I',data,p+8)[0]
            if kind==b'SFFO':offsets=list(struct.unpack_from('<16I',data,p+12))
            p+=12+n
        assert data[p:p+2]==bytes(2)
        for index,offset in enumerate(offsets):
            if not offset:break
            w,h=struct.unpack_from('<HH',data,offset)
            if not w and not h:break
            size=int.from_bytes(data[offset+4:offset+7],'little')
            assert w>=4 and h>=4 and size==w*h
            assert offset+7+size<=len(data)
            if index==0:
                # Build a DDS header independently to test the actual DXT5 payload.
                header=bytearray(128)
                header[:4]=b'DDS '
                struct.pack_into('<7I',header,4,124,0x81007,h,w,size,0,1)
                struct.pack_into('<II4sIIIII',header,76,32,4,b'DXT5',0,0,0,0,0)
                struct.pack_into('<I',header,108,0x1000)
                restored=Image.open(io.BytesIO(header+data[offset+7:offset+7+size])).convert('RGBA')
                original=Image.open(path.with_suffix('.png')).convert('RGBA')
                assert restored.size==original.size
                alpha_rms=ImageStat.Stat(ImageChops.difference(restored.getchannel('A'),original.getchannel('A'))).rms[0]
                assert alpha_rms<17,(path.name,alpha_rms)
        print(f'{path.name}: valid DXT5 PAA, {len(data):,} bytes')

if __name__=='__main__':
    main()
    verify()
