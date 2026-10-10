"""List every .ent/.map reference to an .fbx whose file has no .anm/.msh cache next to it.

The engine resolves resource names by file name through resources.cfg, so references are matched by name.
"""
import os
import re
import sys

G = sys.argv[1] if len(sys.argv) > 1 else 'C:/Program Files/Epic Games/AmnesiaAMFP'
low = set()
names = {}
for root, _, files in os.walk(G):
    for f in files:
        p = os.path.join(root, f).replace(os.sep, '/')
        low.add(p.lower())
        names.setdefault(f.lower(), []).append(p)

bad = {}
for root, _, files in os.walk(G):
    for f in files:
        if not f.lower().endswith(('.ent', '.map')):
            continue
        txt = open(os.path.join(root, f), encoding='latin-1').read()
        for ref in set(re.findall(r'"([^"]+\.fbx)"', txt, re.I)):
            cands = names.get(os.path.basename(ref).lower(), [])
            if not cands:
                bad[(f, ref)] = 'NOT FOUND'
                continue
            # cFileSearcher::GetFilePath: pick the file whose directories best match the wanted path (from the end)
            want = [d.lower() for d in re.split(r'[\\/]', os.path.dirname(ref)) if d]

            def score(c):
                dirs = [d.lower() for d in os.path.dirname(c[len(G) + 1:]).split('/') if d]
                n = 0
                i, j = len(want) - 1, len(dirs) - 1
                while i >= 0 and j >= 0:
                    if want[i] == dirs[j]:
                        n += 1
                        i -= 1
                    j -= 1
                return n
            c = max(cands, key=score)
            base = os.path.splitext(c)[0].lower()
            if base + '.anm' not in low and base + '.msh' not in low:
                bad[(f, ref)] = c[len(G) + 1:]

for (f, ref), c in sorted(bad.items()):
    print('%-40s %-60s %s' % (f, ref, c))
print('%d references' % len(bad))
