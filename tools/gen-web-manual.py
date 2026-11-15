import html,re
src=open('/home/claude/lumen/Resources/Manual.md').read().splitlines()
page=open('/mnt/project-files/lumen/manual/index.html').read()
body=[];inlist=False
def li(t):
    m=re.match(r'^(.{1,40}?): (.*)$',t)
    return f'<li><b>{html.escape(m.group(1))}:</b> {html.escape(m.group(2))}</li>' if m else f'<li>{html.escape(t)}</li>'
for l in src:
    l=l.rstrip()
    if not l: continue
    if l.startswith('- '):
        if not inlist: body.append('<ul>'); inlist=True
        body.append(li(l[2:])); continue
    if inlist: body.append('</ul>'); inlist=False
    if l.startswith('## '):
        t=l[3:]; body.append(f'<h2 id="{re.sub(r"[^a-z0-9]+","-",t.lower()).strip("-")}">{html.escape(t)}</h2>')
    elif l.startswith('# '): body.append(f'<h1>{html.escape(l[2:])}</h1>')
    else: body.append(f'<p>{html.escape(l)}</p>')
if inlist: body.append('</ul>')
a=page.index('<h1>'); b=page.index('</main>')
page=page[:a]+'\n'.join(body).replace('My plugins','<a href="../../account/">My plugins</a>')+'\n'+page[b:]
open('/mnt/project-files/lumen/manual/index.html','w').write(page)
