"""Bone names of an HPL2 .msh (MSH format v7) skeleton."""
import struct


def msh_bones(path):
    b = open(path, 'rb').read()
    pos = 8
    sub, anims = struct.unpack_from('<ii', b, pos); pos += 8
    has_skel = b[pos]; pos += 1
    names = []
    if not has_skel:
        return names

    def cstr():
        nonlocal pos
        e = b.index(b'\0', pos); s = b[pos:e].decode('latin-1'); pos = e + 1; return s

    def bone():
        nonlocal pos
        names.append(cstr()); cstr(); pos += 64
        n = struct.unpack_from('<i', b, pos)[0]; pos += 4
        for _ in range(n): bone()

    n = struct.unpack_from('<i', b, pos)[0]; pos += 4
    for _ in range(n): bone()
    return names
