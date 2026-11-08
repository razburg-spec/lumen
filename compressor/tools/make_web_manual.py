"""Builds the website's user guide page from Resources/Manual.md, using Lumen's web manual as the page template.
Usage: python3 tools/make_web_manual.py <lumen-manual-template.html> <out.html>"""
import html, re, sys

tpl = open(sys.argv[1]).read()
md = open(__file__.rsplit('/tools/', 1)[0] + '/Resources/Manual.md').read().splitlines()
slug = lambda t: re.sub(r'[^a-z0-9]+', '-', t.lower()).strip('-')
body, nav, inlist = [], [], False

def close():
    global inlist
    if inlist:
        body.append('</ul>')
        inlist = False

for line in md:
    line = line.rstrip()
    if not line:
        continue
    if line.startswith('# '):
        close()
        body.append('<img class="shot" src="../../img/chroma.jpg" alt="Chroma plugin window">')
        body.append('<h1>%s</h1>' % html.escape(line[2:]))
    elif line.startswith('## '):
        close()
        t = line[3:]
        nav.append('<a href="#%s">%s</a>' % (slug(t), html.escape(t)))
        body.append('<h2 id="%s">%s</h2>' % (slug(t), html.escape(t)))
    elif line.startswith('- '):
        if not inlist:
            body.append('<ul>')
            inlist = True
        t = line[2:]
        i = t.find(': ')
        body.append('<li><b>%s:</b>%s</li>' % (html.escape(t[:i]), html.escape(t[i + 1:])) if 0 < i < 40 else '<li>%s</li>' % html.escape(t))
    else:
        close()
        body.append('<p>%s</p>' % html.escape(line))
close()

head, rest = tpl.split('<div class="wrap"><nav>', 1)
tail = rest.split('</main>', 1)[1]
head = (head.replace('<title>Lumen User Guide', '<title>Chroma User Guide')
            .replace('User guide for Lumen, the reverb, delay and shimmer plugin', 'User guide for Chroma, the multi-character compressor plugin')
            .replace('#lumen">Back to Lumen', '#chroma">Back to Chroma'))
open(sys.argv[2], 'w').write(head + '<div class="wrap"><nav>' + ''.join(nav) + '</nav><main>\n' + '\n'.join(body) + '\n</main>' + tail)
