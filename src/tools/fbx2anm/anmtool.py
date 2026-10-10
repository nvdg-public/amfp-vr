"""Read and compare HPL2 .anm (MSH format v7 animation) files.

Usage:
  python anmtool.py dump  <file.anm> [maxframes]
  python anmtool.py diff  <reference.anm> <ours.anm>
  python anmtool.py batch <pairs.txt> <out.json>    # lines: reference<TAB>ours
"""
import json
import math
import struct
import sys

MAGIC = 0x76034569
VERSION = 7


def read_anm(path):
    b = open(path, 'rb').read()
    pos = 0

    def i32():
        nonlocal pos
        v = struct.unpack_from('<i', b, pos)[0]
        pos += 4
        return v

    def f32(n=1):
        nonlocal pos
        v = struct.unpack_from('<%df' % n, b, pos)
        pos += 4 * n
        return v if n > 1 else v[0]

    def s16():
        nonlocal pos
        v = struct.unpack_from('<h', b, pos)[0]
        pos += 2
        return v

    def cstr():
        nonlocal pos
        end = b.index(b'\0', pos)
        v = b[pos:end].decode('latin-1')
        pos = end + 1
        return v

    magic, version = i32() & 0xffffffff, i32()
    if magic != MAGIC or version != VERSION:
        raise ValueError('%s: bad header %x v%d' % (path, magic, version))
    anim = {'name': cstr(), 'length': f32(), 'tracks': []}
    for _ in range(i32()):
        tr = {'name': cstr(), 'flags': s16(), 'keys': []}
        for _ in range(i32()):
            t = f32()
            trans = f32(3)
            quat = f32(4)  # x, y, z, w
            tr['keys'].append((t, trans, quat))
        anim['tracks'].append(tr)
    if pos != len(b):
        raise ValueError('%s: %d trailing bytes' % (path, len(b) - pos))
    return anim


def quat_angle(q1, q2):
    d = abs(sum(a * b for a, b in zip(q1, q2)))
    return math.degrees(2 * math.acos(min(1.0, d)))


def diff(ref, ours):
    r, o = read_anm(ref), read_anm(ours)
    rep = {'length_ref': r['length'], 'length_ours': o['length'], 'name_ref': r['name'], 'name_ours': o['name']}
    rt = {t['name']: t for t in r['tracks']}
    ot = {t['name']: t for t in o['tracks']}
    rep['tracks_ref'] = len(rt)
    rep['tracks_ours'] = len(ot)
    rep['missing_tracks'] = sorted(set(rt) - set(ot))
    rep['extra_tracks'] = sorted(set(ot) - set(rt))
    rep['track_order_same'] = [t['name'] for t in r['tracks']] == [t['name'] for t in o['tracks']]
    max_dt = max_dpos = max_drot = 0.0
    key_count_mismatch = []
    flag_mismatch = []
    for name in set(rt) & set(ot):
        a, c = rt[name], ot[name]
        if a['flags'] != c['flags']:
            flag_mismatch.append(name)
        if len(a['keys']) != len(c['keys']):
            key_count_mismatch.append((name, len(a['keys']), len(c['keys'])))
        for ka, kc in zip(a['keys'], c['keys']):
            max_dt = max(max_dt, abs(ka[0] - kc[0]))
            max_dpos = max(max_dpos, max(abs(x - y) for x, y in zip(ka[1], kc[1])))
            max_drot = max(max_drot, quat_angle(ka[2], kc[2]))
    rep.update(key_count_mismatch=key_count_mismatch, flag_mismatch=flag_mismatch,
               max_dtime=max_dt, max_dpos=max_dpos, max_drot_deg=max_drot)
    return rep


def main():
    cmd = sys.argv[1]
    if cmd == 'dump':
        a = read_anm(sys.argv[2])
        n = int(sys.argv[3]) if len(sys.argv) > 3 else 2
        print('name=%r length=%.4f tracks=%d' % (a['name'], a['length'], len(a['tracks'])))
        for t in a['tracks']:
            print('  %-32s flags=%d keys=%d' % (t['name'], t['flags'], len(t['keys'])))
            for k in t['keys'][:n]:
                print('     t=%.4f pos=(%.4f %.4f %.4f) q=(%.4f %.4f %.4f %.4f)' % ((k[0],) + k[1] + k[2]))
    elif cmd == 'diff':
        print(json.dumps(diff(sys.argv[2], sys.argv[3]), indent=1))
    elif cmd == 'batch':
        out = {}
        for line in open(sys.argv[2], encoding='utf-8'):
            if line.strip():
                ref, ours = line.rstrip('\n').split('\t')
                try:
                    out[ref] = diff(ref, ours)
                except Exception as e:  # keep going, report per file
                    out[ref] = {'error': str(e)}
        json.dump(out, open(sys.argv[3], 'w'), indent=1)


if __name__ == '__main__':
    main()
