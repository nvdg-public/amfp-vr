"""Converts a skinned glTF hand (Godot XR Tools hands, CC0) into an HPL2 .msh (MSH format v7).

Usage: python gltf2msh.py <in.gltf> <out.msh> <material.mat> [--flip-v]

Writes one sub mesh with a skeleton and no animations; the fingers are posed from code
(cLuxPlayerHands). The layout mirrors cMeshLoaderMSH::LoadMesh. Vertex data matches what the
game's own skinned meshes use (models/player/hands/hands.msh): Normal(3), Position(4), Color0(4),
Texture1Tangent(4), Texture0(3), all float. Tangents use the engine's cMath::CreateTriTangentVectors.
"""
import base64
import json
import math
import struct
import sys

MSH_MAGIC = 0x76034569
MSH_VERSION = 7
# eVertexBufferElement
EL_NORMAL, EL_POSITION, EL_COLOR0, EL_TANGENT, EL_TEX0 = 0, 1, 2, 4, 5
FMT_FLOAT = 0


# ---- small matrix helpers (row major 4x4, translation in column 3, like cMatrixf) ----
def mat_mul(a, b):
    return [[sum(a[r][k] * b[k][c] for k in range(4)) for c in range(4)] for r in range(4)]


def mat_from_trs(node):
    if 'matrix' in node:  # glTF is column major
        m = node['matrix']
        return [[m[c * 4 + r] for c in range(4)] for r in range(4)]
    tx, ty, tz = node.get('translation', [0, 0, 0])
    x, y, z, w = node.get('rotation', [0, 0, 0, 1])
    sx, sy, sz = node.get('scale', [1, 1, 1])
    r = [[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
         [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
         [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]]
    return [[r[0][0] * sx, r[0][1] * sy, r[0][2] * sz, tx],
            [r[1][0] * sx, r[1][1] * sy, r[1][2] * sz, ty],
            [r[2][0] * sx, r[2][1] * sy, r[2][2] * sz, tz],
            [0, 0, 0, 1]]


def mat_flat(m):
    return [m[r][c] for r in range(4) for c in range(4)]


def xform_point(m, p):
    return [m[r][0] * p[0] + m[r][1] * p[1] + m[r][2] * p[2] + m[r][3] for r in range(3)]


def xform_dir(m, d):
    return [m[r][0] * d[0] + m[r][1] * d[1] + m[r][2] * d[2] for r in range(3)]


def norm(v):
    l = math.sqrt(sum(x * x for x in v)) or 1.0
    return [x / l for x in v]


# ---- glTF reading ----
COMP = {5120: ('b', 1), 5121: ('B', 1), 5122: ('h', 2), 5123: ('H', 2), 5125: ('I', 4), 5126: ('f', 4)}
NCOMP = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4, 'MAT4': 16}


def load_gltf(path):
    g = json.load(open(path, encoding='utf-8'))
    bufs = []
    for b in g['buffers']:
        uri = b['uri']
        if uri.startswith('data:'):
            bufs.append(base64.b64decode(uri.split(',', 1)[1]))
        else:
            bufs.append(open(uri if ':' in uri else path.rsplit('/', 1)[0] + '/' + uri, 'rb').read())

    def accessor(i):
        a = g['accessors'][i]
        bv = g['bufferViews'][a['bufferView']]
        fmt, size = COMP[a['componentType']]
        n = NCOMP[a['type']]
        stride = bv.get('byteStride', size * n)
        base = bv.get('byteOffset', 0) + a.get('byteOffset', 0)
        data = bufs[bv['buffer']]
        out = []
        for k in range(a['count']):
            vals = struct.unpack_from('<%d%s' % (n, fmt), data, base + k * stride)
            if a.get('normalized') and fmt in 'BH':
                vals = tuple(v / (255.0 if fmt == 'B' else 65535.0) for v in vals)
            out.append(vals if n > 1 else vals[0])
        return out

    return g, accessor


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    flip_v = '--flip-v' in sys.argv
    src, dst, material = args
    g, acc = load_gltf(src)
    nodes = g['nodes']
    skin = g['skins'][0]
    joints = skin['joints']

    parent = {}
    for i, n in enumerate(nodes):
        for c in n.get('children', []):
            parent[c] = i

    def global_mat(i):
        m = mat_from_trs(nodes[i])
        while i in parent:
            i = parent[i]
            m = mat_mul(mat_from_trs(nodes[i]), m)
        return m

    # Skeleton: roots are joints whose parent is not a joint; their local transform includes
    # every non-joint ancestor (the Armature node). Bone index = depth-first creation order,
    # which is how cSkeleton numbers bones while the loader reads them.
    jset = set(joints)
    roots = [j for j in joints if parent.get(j) not in jset]
    order = []

    def visit(j):
        order.append(j)
        for c in nodes[j].get('children', []):
            if c in jset:
                visit(c)
    for r in roots:
        visit(r)
    bone_index = {j: order.index(j) for j in joints}

    mesh_node = next(i for i, n in enumerate(nodes) if 'mesh' in n and n.get('skin') == 0)
    prim = g['meshes'][nodes[mesh_node]['mesh']]['primitives']
    assert len(prim) == 1, 'expected one primitive'
    prim = prim[0]
    at = prim['attributes']
    pos = acc(at['POSITION'])
    nrm = acc(at['NORMAL'])
    uv = acc(at['TEXCOORD_0'])
    jnt = acc(at['JOINTS_0'])
    wgt = acc(at['WEIGHTS_0'])
    idx = acc(prim['indices'])
    # glTF front faces are counterclockwise; HPL2's are clockwise (as in the game's own meshes)
    idx = [idx[k + o] for k in range(0, len(idx), 3) for o in (0, 2, 1)]
    ibm_flat = acc(skin['inverseBindMatrices'])
    ibm = [[[m[c * 4 + r] for c in range(4)] for r in range(4)] for m in ibm_flat]
    skin_mats = [mat_mul(global_mat(j), ibm[k]) for k, j in enumerate(joints)]

    # Bind pose vertices in skeleton space: sum(w * jointGlobal * IBM * v)
    nv = len(pos)
    bpos, bnrm, pairs = [], [], []
    for v in range(nv):
        p = [0.0, 0.0, 0.0]; n = [0.0, 0.0, 0.0]
        ws = [(jnt[v][k], wgt[v][k]) for k in range(4) if wgt[v][k] > 0.0]
        total = sum(w for _, w in ws) or 1.0
        ws = [(j, w / total) for j, w in ws]
        for j, w in ws:
            tp = xform_point(skin_mats[j], pos[v]); tn = xform_dir(skin_mats[j], nrm[v])
            p = [p[i] + tp[i] * w for i in range(3)]; n = [n[i] + tn[i] * w for i in range(3)]
            pairs.append((v, bone_index[joints[j]], w))
        bpos.append(p); bnrm.append(norm(n))
    tex = [(u, (1.0 - t) if flip_v else t, 0.0) for u, t in uv]

    # Tangents: port of cMath::CreateTriTangentVectors
    t1 = [[0.0] * 3 for _ in range(nv)]; t2 = [[0.0] * 3 for _ in range(nv)]
    for k in range(0, len(idx), 3):
        a, b, c = idx[k], idx[k + 1], idx[k + 2]
        p12 = [bpos[b][i] - bpos[a][i] for i in range(3)]; p13 = [bpos[c][i] - bpos[a][i] for i in range(3)]
        t12 = [tex[b][i] - tex[a][i] for i in range(2)]; t13 = [tex[c][i] - tex[a][i] for i in range(2)]
        den = t12[0] * t13[1] - t12[1] * t13[0]
        r = 1.0 / den if den != 0 else 0.0
        sd = [(t13[1] * p12[i] - t12[1] * p13[i]) * r for i in range(3)]
        td = [(t12[0] * p13[i] - t13[0] * p12[i]) * r for i in range(3)]
        for x in (a, b, c):
            t1[x] = [t1[x][i] + sd[i] for i in range(3)]; t2[x] = [t2[x][i] + td[i] for i in range(3)]
    # smooth across seam copies (same position and normal)
    key = {}
    for v in range(nv):
        key.setdefault((tuple(round(x, 6) for x in bpos[v]), tuple(round(x, 5) for x in bnrm[v])), []).append(v)
    s1 = [list(x) for x in t1]; s2 = [list(x) for x in t2]
    for group in key.values():
        if len(group) > 1:
            g1 = [sum(t1[v][i] for v in group) for i in range(3)]; g2 = [sum(t2[v][i] for v in group) for i in range(3)]
            for v in group:
                s1[v] = g1; s2[v] = g2
    tang = []
    for v in range(nv):
        n = bnrm[v]; a = s1[v]
        d = sum(n[i] * a[i] for i in range(3))
        t = norm([a[i] - n[i] * d for i in range(3)])
        cr = [n[1] * a[2] - n[2] * a[1], n[2] * a[0] - n[0] * a[2], n[0] * a[1] - n[1] * a[0]]
        w = -1.0 if sum(cr[i] * s2[v][i] for i in range(3)) < 0 else 1.0
        tang.append(t + [w])

    # ---- write ----
    out = bytearray()
    i32 = lambda x: out.extend(struct.pack('<i', x))
    f32s = lambda xs: out.extend(struct.pack('<%df' % len(xs), *xs))
    s16 = lambda x: out.extend(struct.pack('<h', x))
    cstr = lambda s: out.extend(s.encode('latin-1') + b'\0')
    i32(MSH_MAGIC - (1 << 32) if MSH_MAGIC >= (1 << 31) else MSH_MAGIC); i32(MSH_VERSION)
    i32(1); i32(0); out.append(1)  # sub meshes, animations, has skeleton

    def write_bone(j, local):
        cstr(nodes[j]['name']); cstr(nodes[j]['name']); f32s(mat_flat(local))
        kids = [c for c in nodes[j].get('children', []) if c in jset]
        i32(len(kids))
        for c in kids:
            write_bone(c, mat_from_trs(nodes[c]))
    i32(len(roots))
    for r in roots:
        write_bone(r, global_mat(r))
    i32(0)  # nodes

    name = src.replace('\\', '/').rsplit('/', 1)[-1].rsplit('.', 1)[0]
    cstr(name); cstr(material)
    f32s(mat_flat([[1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 1, 0], [0, 0, 0, 1]])); f32s([1, 1, 1]); out.append(0)
    i32(0)  # colliders
    i32(len(pairs))
    for v, b, w in pairs:
        i32(v); i32(b); f32s([w])
    i32(nv); i32(5)
    for el, n, data in ((EL_NORMAL, 3, bnrm), (EL_POSITION, 4, [p + [1.0] for p in bpos]),
                        (EL_COLOR0, 4, [[1.0, 1.0, 1.0, 1.0]] * nv), (EL_TANGENT, 4, tang),
                        (EL_TEX0, 3, tex)):
        s16(el); s16(FMT_FLOAT); i32(0); i32(n)
        for row in data:
            f32s(list(row))
    i32(len(idx)); out.extend(struct.pack('<%di' % len(idx), *idx))
    i32(0)  # animations
    open(dst, 'wb').write(out)

    lo = [min(p[i] for p in bpos) for i in range(3)]; hi = [max(p[i] for p in bpos) for i in range(3)]
    print('%s: %d vertices, %d triangles, %d bones, %d weights, bounds %s..%s' % (
        dst, nv, len(idx) // 3, len(order), len(pairs),
        [round(x, 3) for x in lo], [round(x, 3) for x in hi]))
    for j in order:
        gm = global_mat(j)
        if nodes[j]['name'].startswith(('Wrist', 'Palm', 'Middle_Metacarpal', 'Middle_Tip')):
            print('  %-22s world pos %s' % (nodes[j]['name'], [round(gm[r][3], 3) for r in range(3)]))


if __name__ == '__main__':
    main()
