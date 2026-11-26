#!/usr/bin/env python3
"""Write the WibWob-DOS PDF: the asm.js build plus kernel/pages/pdf-host.js as a document-level script,
and the form fields it draws into and reads from.

    python3 kernel/tools/make_pdf.py build-pdf/wwdos-pdf.js dist-web/wwdos.pdf [--no-mouse] [--cols 80 --rows 25]

Needs fontTools (pip install fonttools) and a monospace TTF with box-drawing glyphs (DejaVu Sans Mono by
default, --font to pick another). Layout of the page and of the fields: docs/pdf-port.md.
"""
import argparse
import io
import json
import os
import re
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))

# The 16 BIOS colours, as Turbo Vision's default palette draws them.
PALETTE = [0x000000, 0x0000AA, 0x00AA00, 0x00AAAA, 0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
           0x555555, 0x5555FF, 0x55FF55, 0x55FFFF, 0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF]

FONT_CANDIDATES = ['/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf',
                   '/Library/Fonts/DejaVuSansMono.ttf',
                   os.path.expanduser('~/Library/Fonts/DejaVuSansMono.ttf')]

# Key buttons under the screen: (label, key name for wwKeyName)
KEY_ROWS = [
    [('Esc', 'Esc'), ('Tab', 'Tab'), ('Sh-Tab', 'BackTab'), ('Enter', 'Enter'), ('Bksp', 'Bksp'), ('Del', 'Del'),
     ('Ins', 'Ins'), ('Home', 'Home'), ('End', 'End'), ('PgUp', 'PgUp'), ('PgDn', 'PgDn'),
     ('←', 'Left'), ('↑', 'Up'), ('↓', 'Down'), ('→', 'Right')],
    [('F1', 'F1'), ('F2', 'F2'), ('F3', 'F3'), ('F4', 'F4'), ('F5', 'F5'), ('F6', 'F6'), ('F7', 'F7'),
     ('F8', 'F8'), ('F9', 'F9'), ('F10', 'F10'), ('F11', 'F11'), ('F12', 'F12'), ('Alt', 'Alt'), ('Ctrl', 'Ctrl')],
]


def pdf_str(s):
    """A PDF literal string (ASCII only)."""
    return '(' + s.replace('\\', '\\\\').replace('(', '\\(').replace(')', '\\)') + ')'


def js_ascii(s):
    """JavaScript source with every non-ASCII character as a \\u escape (the stream is then plain ASCII)."""
    out = []
    for ch in s:
        o = ord(ch)
        if o < 128:
            out.append(ch)
        elif o <= 0xFFFF:
            out.append('\\u%04x' % o)
        else:
            o -= 0x10000
            out.append('\\u%04x\\u%04x' % (0xD800 + (o >> 10), 0xDC00 + (o & 0x3FF)))
    return ''.join(out)


class Pdf:
    def __init__(self):
        self.objs = []

    def add(self, body):
        self.objs.append(body if isinstance(body, bytes) else body.encode('latin-1'))
        return len(self.objs)

    def reserve(self):
        self.objs.append(None)
        return len(self.objs)

    def set(self, num, body):
        self.objs[num - 1] = body if isinstance(body, bytes) else body.encode('latin-1')

    def stream(self, data, extra='', compress=True):
        if compress:
            z = zlib.compress(data, 9)
            return self.add(b'<< /Length %d /Filter /FlateDecode %s >>\nstream\n' % (len(z), extra.encode()) + z + b'\nendstream')
        return self.add(b'<< /Length %d %s >>\nstream\n' % (len(data), extra.encode()) + data + b'\nendstream')

    def write(self, path, root):
        out = bytearray(b'%PDF-1.7\n%\xe2\xe3\xcf\xd3\n')
        offs = []
        for i, o in enumerate(self.objs):
            offs.append(len(out))
            out += b'%d 0 obj\n' % (i + 1) + o + b'\nendobj\n'
        x = len(out)
        out += b'xref\n0 %d\n0000000000 65535 f \n' % (len(self.objs) + 1)
        for o in offs:
            out += b'%010d 00000 n \n' % o
        out += b'trailer\n<< /Size %d /Root %d 0 R >>\nstartxref\n%d\n%%%%EOF\n' % (len(self.objs) + 1, root, x)
        with open(path, 'wb') as f:
            f.write(out)
        return len(out)


def wanted_chars():
    """ASCII, CP437's upper half and every character in the browser atlas (the app's and primers' glyphs)."""
    chars = set(chr(i) for i in range(32, 127))
    chars |= set(bytes([i]).decode('cp437') for i in range(128, 256))
    chars |= set('☺☻♥♦♣♠•◘○◙♂♀♪♫☼'
                 '►◄↕‼¶§▬↨↑↓→←∟↔▲▼⌂')
    atlas = os.path.join(ROOT, 'kernel', 'pages', 'atlas.js')
    if os.path.exists(atlas):
        m = re.search(r'cps:\[([0-9,]+)\]', open(atlas).read())
        if m:
            chars |= set(chr(int(c)) for c in m.group(1).split(',') if int(c) <= 0xFFFF)
    return chars


def embed_font(pdf, path, chars):
    """A Type0 font (CIDFontType2, Identity-H, ToUnicode) subset to the characters the font has.
    Returns (font object, the characters it draws, metrics)."""
    from fontTools import subset
    from fontTools.ttLib import TTFont
    full = TTFont(path)
    have = set(full.getBestCmap().keys())
    chars = sorted(c for c in chars if ord(c) in have)
    opts = subset.Options()
    opts.glyph_names = False
    opts.notdef_outline = True
    opts.name_IDs = ['*']
    opts.drop_tables += ['FFTM']
    f = TTFont(path)
    s = subset.Subsetter(opts)
    s.populate(unicodes=[ord(c) for c in chars])
    s.subset(f)
    cmap = f.getBestCmap()
    order = f.getGlyphOrder()
    gid = {n: i for i, n in enumerate(order)}
    upm = f['head'].unitsPerEm
    adv = f['hmtx'][cmap[ord('M')]][0] * 1000.0 / upm
    # The full block's extent is the cell's height: background blocks then tile without gaps.
    blk = f['glyf'][cmap[0x2588]] if 0x2588 in cmap else None
    asc = (blk.yMax if blk else f['hhea'].ascent) * 1000.0 / upm
    desc = (blk.yMin if blk else f['hhea'].descent) * 1000.0 / upm
    # Push the block elements' bottom (and a little of their sides) out past the cell, so neighbouring cells
    # overlap instead of leaving hairline seams when the viewer rounds positions. Only the bottom: the row
    # below is drawn later and covers the overhang, so no row hides part of the one above.
    if blk is not None:
        glyf = f['glyf']
        top, bot, left, right = blk.yMax, blk.yMin, blk.xMin, blk.xMax
        e = upm // 40
        for u in range(0x2580, 0x25A0):
            if u not in cmap:
                continue
            g = glyf[cmap[u]]
            if g.isComposite() or not g.numberOfContours:
                continue
            coords = g.coordinates
            for i in range(len(coords)):
                x, y = coords[i]
                if y <= bot: y = bot - 3 * e      # rows are drawn top to bottom: the next row covers this
                if x <= left: x = left - e
                if x >= right: x = right + e
                coords[i] = (x, y)
            g.recalcBounds(glyf)
    buf = io.BytesIO()
    f.save(buf)
    data = buf.getvalue()
    ff = pdf.stream(data, '/Length1 %d' % len(data))
    fd = pdf.add('<< /Type /FontDescriptor /FontName /WWMono /Flags 33 /FontBBox [-50 %d %d %d] /ItalicAngle 0 '
                 '/Ascent %d /Descent %d /CapHeight 730 /StemV 80 /FontFile2 %d 0 R >>'
                 % (desc, adv + 50, asc, asc, desc, ff))
    cid = pdf.add('<< /Type /Font /Subtype /CIDFontType2 /BaseFont /WWMono /CIDSystemInfo << /Registry (Adobe) '
                  '/Ordering (Identity) /Supplement 0 >> /FontDescriptor %d 0 R /DW %d /CIDToGIDMap /Identity >>'
                  % (fd, round(adv)))
    lines = ['/CIDInit /ProcSet findresource begin 12 dict begin begincmap',
             '/CIDSystemInfo << /Registry (Adobe) /Ordering (UCS) /Supplement 0 >> def',
             '/CMapName /Adobe-Identity-UCS def /CMapType 2 def',
             '1 begincodespacerange <0000> <FFFF> endcodespacerange']
    items = sorted((gid[n], u) for u, n in cmap.items() if u <= 0xFFFF)
    for i in range(0, len(items), 100):
        chunk = items[i:i + 100]
        lines.append('%d beginbfchar' % len(chunk))
        lines += ['<%04X> <%04X>' % (g, u) for g, u in chunk]
        lines.append('endbfchar')
    lines.append('endcmap CMapName currentdict /CMap defineresource pop end end')
    tu = pdf.stream('\n'.join(lines).encode())
    font = pdf.add('<< /Type /Font /Subtype /Type0 /BaseFont /WWMono /Encoding /Identity-H '
                   '/DescendantFonts [%d 0 R] /ToUnicode %d 0 R >>' % (cid, tu))
    drawn = ''.join(c for c in chars if ord(c) in cmap)
    return font, drawn, {'adv': adv, 'asc': asc, 'desc': desc, 'bytes': len(data)}


def rgb(c):
    return '%.3f %.3f %.3f' % (((c >> 16) & 255) / 255.0, ((c >> 8) & 255) / 255.0, (c & 255) / 255.0)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('engine', help='the asm.js build (kernel/build-pdf.sh writes build-pdf/wwdos-pdf.js)')
    ap.add_argument('out', help='the PDF to write')
    ap.add_argument('--cols', type=int, default=80)
    ap.add_argument('--rows', type=int, default=25)
    ap.add_argument('--size', type=float, default=9.0, help='font size in points (cell width is 0.6 of it)')
    ap.add_argument('--font', default=None, help='monospace TTF with box drawing (default DejaVu Sans Mono)')
    ap.add_argument('--no-mouse', action='store_true', help='leave out the per-cell click buttons')
    ap.add_argument('--draw-ms', type=int, default=40, help='delay before a changed screen is drawn')
    ap.add_argument('--debug', action='store_true', help='show timer, key and frame counts in the status line')
    args = ap.parse_args()

    font_path = args.font or next((p for p in FONT_CANDIDATES if os.path.exists(p)), None)
    if not font_path:
        sys.exit('no font: pass --font /path/to/DejaVuSansMono.ttf')
    C, R, fs = args.cols, args.rows, args.size

    pdf = Pdf()
    wwfont, glyphs, met = embed_font(pdf, font_path, wanted_chars())
    helv = pdf.add('<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>')
    cw = fs * met['adv'] / 1000.0
    pitch = fs * (met['asc'] - met['desc']) / 1000.0

    margin = 16.0
    sw, sh = C * cw, R * pitch
    pad = 2.0                      # the inset a text field puts before its text
    key_h, key_gap = 15.0, 3.0
    kb_h = 26.0
    W = sw + 2 * margin
    H = margin + sh + 10 + len(KEY_ROWS) * (key_h + key_gap) + 6 + kb_h + 34 + margin
    x0 = margin - pad
    top = H - margin
    # a little room for the field inset on the right
    fields = []
    page = pdf.reserve()

    def widget(body):
        n = pdf.add(body.replace('/P PAGE', '/P %d 0 R' % page))
        fields.append(n)
        return n

    tx_flags = 1 | (1 << 22) | (1 << 23)   # ReadOnly, DoNotSpellCheck, DoNotScroll
    layers = [('b%d' % k, PALETTE[k]) for k in range(16)] + [('f%d' % k, PALETTE[k]) for k in range(16)] + [('c', 0xFFFF55)]
    ov = 2.0    # each row's fields reach this far into the next row: single-line text is centred, so the
                # baseline stays put while the background blocks overlap instead of leaving seams
    for y in range(R):
        y1 = top - y * pitch
        y0 = y1 - pitch
        for name, col in layers:
            widget('<< /Type /Annot /Subtype /Widget /FT /Tx /T (r%d.%s) /Rect [%.3f %.3f %.3f %.3f] /F 4 /Ff %d /Q 0 '
                   '/DA (/WW %.3f Tf %s rg) /V () /MK << >> /BS << /W 0 >> /P PAGE >>'
                   % (y, name, x0, y0 - ov, x0 + sw + 2 * pad + 1, y1 + ov, tx_flags, fs, rgb(col)))

    if not args.no_mouse:
        clear = pdf.stream(b'', '/Type /XObject /Subtype /Form /BBox [0 0 %.3f %.3f]' % (cw, pitch), compress=False)
        for y in range(R):
            y1 = top - y * pitch
            for x in range(C):
                xl = margin + x * cw
                widget('<< /Type /Annot /Subtype /Widget /FT /Btn /Ff 65536 /T (m.%d.%d) /Rect [%.2f %.2f %.2f %.2f] '
                       '/F 4 /H /N /MK << >> /AP << /N %d 0 R >> /A << /S /JavaScript /JS (wwClick\\(%d,%d\\)) >> /P PAGE >>'
                       % (x, y, xl, y1 - pitch, xl + cw, y1, clear, x, y))

    # key buttons
    ky = top - sh - 10
    content = ['0.067 0.067 0.067 rg 0 0 %.2f %.2f re f' % (W, H),
               '0 0 0.667 rg %.3f %.3f %.3f %.3f re f' % (margin, top - sh, sw, sh)]
    # Shown until the app's first frame covers it, and for good in viewers that run no JavaScript.
    for i, line in enumerate(['WIBWOB-DOS', '',
                              'This PDF runs the WibWob-DOS desktop with the JavaScript in the file.',
                              'If this text stays, your viewer does not run PDF JavaScript:',
                              'open the file in Chrome, Edge or Brave (their built-in PDF viewer).']):
        if line:
            size = 14 if i == 0 else 8
            content.append('BT /Helv %d Tf 1 1 1 rg %.2f %.2f Td %s Tj ET'
                           % (size, margin + 16, top - sh / 2 + 30 - i * 13, pdf_str(line)))
    for row in KEY_ROWS:
        n = len(row)
        bw = (sw - (n - 1) * key_gap) / n
        for i, (label, name) in enumerate(row):
            bx = margin + i * (bw + key_gap)
            by = ky - key_h
            lab = label.encode('cp1252', 'replace').decode('latin-1') if all(ord(c) < 256 for c in label) else None
            ap_lines = ['0.8 0.8 0.8 rg 0 0 %.2f %.2f re f' % (bw, key_h), '0.4 0.4 0.4 RG 0.6 w 0.3 0.3 %.2f %.2f re S' % (bw - 0.6, key_h - 0.6), '0 g']
            if lab is not None:
                tw = len(lab) * 7 * 0.55
                ap_lines.append('BT /Helv 7 Tf %.2f %.2f Td %s Tj ET' % (max(1, (bw - tw) / 2), 4.5, pdf_str(lab)))
            else:
                # arrows: Helvetica has none in WinAnsi; draw a triangle
                cx, cy, s = bw / 2, key_h / 2, 3.5
                tri = {'Left': (cx + s, cy + s, cx + s, cy - s, cx - s, cy), 'Right': (cx - s, cy + s, cx - s, cy - s, cx + s, cy),
                       'Up': (cx - s, cy - s, cx + s, cy - s, cx, cy + s), 'Down': (cx - s, cy + s, cx + s, cy + s, cx, cy - s)}[name]
                ap_lines.append('%.2f %.2f m %.2f %.2f l %.2f %.2f l f' % tri)
            apo = pdf.stream('\n'.join(ap_lines).encode(), '/Type /XObject /Subtype /Form /BBox [0 0 %.2f %.2f] /Resources << /Font << /Helv %d 0 R >> >>' % (bw, key_h, helv))
            cap = pdf_str(lab if lab is not None else name)
            widget('<< /Type /Annot /Subtype /Widget /FT /Btn /Ff 65536 /T (key.%s) /Rect [%.2f %.2f %.2f %.2f] /F 4 /H /P '
                   '/DA (/Helv 7 Tf 0 g) /MK << /BG [0.8 0.8 0.8] /BC [0.4 0.4 0.4] /CA %s >> /AP << /N %d 0 R >> '
                   '/A << /S /JavaScript /JS (wwKeyName\\(\'%s\'\\)) >> /P PAGE >>'
                   % (name, bx, by, bx + bw, by + key_h, cap, apo, name))
        ky -= key_h + key_gap

    # keyboard field
    ky -= 6
    kx = margin + 92
    content.append('BT /Helv 8 Tf 0.85 0.85 0.85 rg %.2f %.2f Td (Click here, then type:) Tj ET' % (margin, ky - kb_h / 2 - 3))
    widget('<< /Type /Annot /Subtype /Widget /FT /Tx /T (k) /Rect [%.2f %.2f %.2f %.2f] /F 4 /Ff %d /DA (/Helv 10 Tf 0 g) /V () '
           '/MK << /BG [1 1 0.85] /BC [1 0.85 0.2] >> /BS << /W 1.5 >> /AA << /K << /S /JavaScript /JS (wwKey\\(event\\);) >> >> /P PAGE >>'
           % (kx, ky - kb_h, margin + sw, ky, 4096 | (1 << 22)))
    ky -= kb_h + 6
    # status/log line
    widget('<< /Type /Annot /Subtype /Widget /FT /Tx /T (log) /Rect [%.2f %.2f %.2f %.2f] /F 4 /Ff 1 /DA (/Helv 6 Tf 0.6 0.6 0.6 rg) /V () /MK << >> /P PAGE >>'
           % (margin, ky - 10, margin + sw, ky))
    content.append('BT /Helv 6.5 Tf 0.6 0.6 0.6 rg %.2f %.2f Td (WIBWOB-DOS in a PDF. Open in Chrome, Edge or Brave. Keys: type in the yellow box; Alt/Ctrl are sticky (press, then a key). F10 opens the menu.) Tj ET' % (margin, ky - 22))

    # document-level script: settings, the host glue, the engine, start
    host = open(os.path.join(ROOT, 'kernel', 'pages', 'pdf-host.js'), encoding='utf-8').read()
    engine = open(args.engine, encoding='utf-8').read()
    settings = 'var WWPDF = {cols: %d, rows: %d, drawMs: %d, tickMs: 10, palette: [%s], glyphs: %s, debug: %s, env: {}};\n' % (
        C, R, args.draw_ms, ','.join(str(p) for p in PALETTE), json.dumps(glyphs), 'true' if args.debug else 'false')
    script = settings + host + '\n' + engine + '\nwwStart();\n'
    js = pdf.stream(js_ascii(script).encode('ascii'))
    jsact = pdf.add('<< /S /JavaScript /JS %d 0 R >>' % js)
    names = pdf.add('<< /Names [(wibwob-dos) %d 0 R] >>' % jsact)

    cs = pdf.stream('\n'.join(content).encode('latin-1'))
    pages = pdf.reserve()
    pdf.set(page, '<< /Type /Page /Parent %d 0 R /MediaBox [0 0 %.2f %.2f] /Contents %d 0 R /Resources << /Font << /Helv %d 0 R /WW %d 0 R >> >> /Annots [%s] >>'
            % (pages, W, H, cs, helv, wwfont, ' '.join('%d 0 R' % f for f in fields)))
    pdf.set(pages, '<< /Type /Pages /Kids [%d 0 R] /Count 1 >>' % page)
    acro = pdf.add('<< /Fields [%s] /DA (/Helv 0 Tf 0 g) /DR << /Font << /WW %d 0 R /Helv %d 0 R >> >> >>'
                   % (' '.join('%d 0 R' % f for f in fields), wwfont, helv))
    info = pdf.add('<< /Title (WIBWOB-DOS) /Creator (kernel/tools/make_pdf.py) >>')
    cat = pdf.add('<< /Type /Catalog /Pages %d 0 R /AcroForm %d 0 R /Names << /JavaScript %d 0 R >> /PageMode /UseNone '
                  '/ViewerPreferences << /DisplayDocTitle true >> >>' % (pages, acro, names))
    size = pdf.write(args.out, cat)
    print('%s: %d bytes, %d fields, font %d glyph chars (%d bytes), engine %d bytes'
          % (args.out, size, len(fields), len(glyphs), met['bytes'], len(engine)))


if __name__ == '__main__':
    main()
