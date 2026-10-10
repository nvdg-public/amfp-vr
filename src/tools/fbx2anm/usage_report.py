"""Which entities and maps use the FBX animations that have no Frictional cache (phase1b/missing_fbx.txt)."""
import collections
import os
import sys

G = sys.argv[1] if len(sys.argv) > 1 else 'C:/Program Files/Epic Games/AmnesiaAMFP'
missing = [l.strip().replace(os.sep, '/').lower() for l in open('phase1b/missing_fbx.txt') if l.strip()]

ents = {}
for root, _, files in os.walk(G):
    for f in files:
        if f.lower().endswith('.ent'):
            p = os.path.join(root, f)
            ents[os.path.relpath(p, G).replace(os.sep, '/').lower()] = open(p, encoding='latin-1').read().lower()

use = collections.defaultdict(set)
for e, txt in ents.items():
    for m in missing:
        tail = '/'.join(m.split('/')[-3:])  # e.g. dinkywretch/animations/bp_vomit.fbx
        if tail in txt:
            use[m].add(e)
unused = [m for m in missing if not use[m]]
print('missing FBX referenced by .ent files: %d, unreferenced: %d' % (len(missing) - len(unused), len(unused)))

maps = {}
for root, _, files in os.walk(os.path.join(G, 'maps')):
    for f in files:
        if f.lower().endswith('.map'):
            maps[f] = open(os.path.join(root, f), encoding='latin-1').read().lower()

entmaps = collections.defaultdict(set)
for e in set().union(*use.values()) if use else set():
    name = os.path.basename(e)
    for mp, txt in maps.items():
        if name in txt:
            entmaps[e].add(mp)

mapuse = collections.defaultdict(set)
for m, es in use.items():
    for e in es:
        for mp in entmaps[e]:
            mapuse[mp].add(m)

for mp in sorted(mapuse):
    entsHere = sorted({os.path.basename(e) for m in mapuse[mp] for e in use[m] if mp in entmaps[e]})
    print('%-26s %3d missing anims, entities: %s' % (mp, len(mapuse[mp]), ', '.join(entsHere)))
print('unreferenced:', ', '.join(unused))
