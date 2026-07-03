#!/usr/bin/env python3
"""
img2lv.py - Convert PNG/JPG -> LVGL C image (TRUE_COLOR_ALPHA, RGB565, swap=0).

Sinh file src/<name>_img.c + .h dung cho pet tricks (khop format bao_img / eat_img / heart_img).
- 1 anh  -> `const lv_img_dsc_t <name>_img;`            (nhu heart_img / bao_img)
- Nhieu anh -> `const lv_img_dsc_t* const <name>_imgs[N];` + `#define <NAME>_FRAMES N` (nhu eat_img)

Vi du:
  # 1 khung, cao 88px, cat sat noi dung, xoa nen den -> trong suot
  python tools/img2lv.py --name heart --in assets/heart.png --h 88 --crop --key-black

  # flipbook nhieu khung, rong 200px moi khung
  python tools/img2lv.py --name dance --in assets/dance_1.png assets/dance_2.png --w 200 --crop --key-black

Yeu cau: pip install Pillow
"""
import argparse, os, sys
from PIL import Image


def content_bbox(im, thr, pad_frac=0.06):
    """Bbox cua vung sang hon `thr` (bo nen den), them le pad_frac."""
    w, h = im.size
    px = im.load()
    minx, miny, maxx, maxy = w, h, 0, 0
    step = max(1, min(w, h) // 200)
    found = False
    for y in range(0, h, step):
        for x in range(0, w, step):
            r, g, b = px[x, y][:3]
            if max(r, g, b) > thr:
                found = True
                minx = min(minx, x); miny = min(miny, y)
                maxx = max(maxx, x); maxy = max(maxy, y)
    if not found:
        return (0, 0, w, h)
    pw = int((maxx - minx) * pad_frac) + 2
    ph = int((maxy - miny) * pad_frac) + 2
    return (max(0, minx - pw), max(0, miny - ph),
            min(w, maxx + pw), min(h, maxy + ph))


def to_frame_bytes(im, args):
    """1 anh PIL -> (bytes RGB565+alpha, w, h)."""
    if args.crop:
        im = im.convert('RGB').crop(content_bbox(im.convert('RGB'), args.lo))
    has_alpha = (im.mode == 'RGBA') and not args.key_black
    rgba = im.convert('RGBA')

    # resize giu ti le theo --w hoac --h
    w0, h0 = rgba.size
    if args.w:
        tw = args.w; th = round(args.w * h0 / w0)
    elif args.h:
        th = args.h; tw = round(args.h * w0 / h0)
    else:
        tw, th = w0, h0
    rgba = rgba.resize((tw, th), Image.LANCZOS)
    px = rgba.load()

    lo, hi = args.lo, args.hi
    out = bytearray()
    for y in range(th):
        for x in range(tw):
            r, g, b, a0 = px[x, y]
            if args.key_black:                       # nen den -> alpha theo do sang
                m = max(r, g, b)
                a = 0 if m <= lo else (255 if m >= hi else round((m - lo) * 255 / (hi - lo)))
            elif has_alpha:
                a = a0
            else:
                a = 255
            c = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)   # RGB565
            out.append(c & 0xFF)                     # lo (LV_COLOR_16_SWAP=0)
            out.append((c >> 8) & 0xFF)              # hi
            out.append(a)                            # alpha
    return bytes(out), tw, th


def emit_map(f, sym, data):
    f.write('static const uint8_t %s[] = {\n' % sym)
    for i in range(0, len(data), 60):
        f.write('  ' + ','.join('0x%02x' % b for b in data[i:i + 60]) + ',\n')
    f.write('};\n')


def dsc_line(name, w, h, data_sym):
    return ('{ .header.always_zero = 0, .header.w = %d, .header.h = %d, '
            '.header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA, .data_size = %d, .data = %s }'
            % (w, h, w * h * 3, data_sym))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--name', required=True, help='ten symbol (vd: heart, dance)')
    ap.add_argument('--in', dest='inp', nargs='+', required=True, help='1 hoac nhieu anh')
    ap.add_argument('--w', type=int, help='rong dich (giu ti le)')
    ap.add_argument('--h', type=int, help='cao dich (giu ti le)')
    ap.add_argument('--crop', action='store_true', help='cat sat vung noi dung (bo nen den)')
    ap.add_argument('--key-black', action='store_true', help='nen den -> trong suot')
    ap.add_argument('--lo', type=int, default=10, help='nguong toi (alpha=0)')
    ap.add_argument('--hi', type=int, default=46, help='nguong sang (alpha=255)')
    ap.add_argument('--out-dir', default='src', help='thu muc xuat (mac dinh src/)')
    args = ap.parse_args()

    frames = []
    for p in args.inp:
        if not os.path.exists(p):
            sys.exit('Khong thay file: ' + p)
        data, w, h = to_frame_bytes(Image.open(p), args)
        frames.append((data, w, h))
        print('  %s -> %dx%d (%d bytes)' % (os.path.basename(p), w, h, len(data)))

    name = args.name
    cpath = os.path.join(args.out_dir, name + '_img.c')
    hpath = os.path.join(args.out_dir, name + '_img.h')
    multi = len(frames) > 1

    with open(cpath, 'w') as f:
        f.write('#include "lvgl.h"\n')
        f.write('// Sinh boi tools/img2lv.py - TRUE_COLOR_ALPHA, RGB565 (LV_COLOR_16_SWAP=0)\n\n')
        for i, (data, w, h) in enumerate(frames):
            emit_map(f, '%s%d_map' % (name, i) if multi else '%s_map' % name, data)
            f.write('\n')
        if multi:
            for i, (data, w, h) in enumerate(frames):
                f.write('static const lv_img_dsc_t %s%d = %s;\n'
                        % (name, i, dsc_line(name, w, h, '%s%d_map' % (name, i))))
            f.write('\nconst lv_img_dsc_t* const %s_imgs[%d] = { %s };\n'
                    % (name, len(frames), ', '.join('&%s%d' % (name, i) for i in range(len(frames)))))
        else:
            data, w, h = frames[0]
            f.write('const lv_img_dsc_t %s_img = %s;\n' % (name, dsc_line(name, w, h, '%s_map' % name)))

    with open(hpath, 'w') as f:
        f.write('#pragma once\n#include "lvgl.h"\n')
        if multi:
            f.write('#define %s_FRAMES %d\n' % (name.upper(), len(frames)))
            f.write('extern const lv_img_dsc_t* const %s_imgs[%s_FRAMES];\n' % (name, name.upper()))
        else:
            f.write('extern const lv_img_dsc_t %s_img;\n' % name)

    print('Da ghi %s + %s' % (cpath, hpath))


if __name__ == '__main__':
    main()
