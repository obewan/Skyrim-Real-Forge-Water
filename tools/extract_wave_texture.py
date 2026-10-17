"""Extracts the game's vanilla wave texture into the mod's data folder.

The mod ships the vanilla textures\\water\\defaultwater.dds under its own path, so that water mods
(which replace the global one) do not replace the forge water's. It is a Bethesda asset, so it is
not stored in this repository: this script takes it from your own Skyrim installation.

    py tools\\extract_wave_texture.py "C:\\...\\Skyrim Special Edition\\Data"

Needs the lz4 package (py -m pip install lz4): Skyrim SE archives are LZ4-compressed.
"""

import os
import struct
import sys

import lz4.frame

ARCHIVE = "Skyrim - Textures7.bsa"
TEXTURE = "textures\\water\\defaultwater.dds"
OUTPUT = os.path.join(os.path.dirname(__file__), "..", "data", "Textures", "RealForgeWater", "Water", "DefaultWater.dds")


def read_bsa_file(bsa_path, wanted):
    """Returns the bytes of one file from a Skyrim SE archive (BSA version 105)."""
    data = open(bsa_path, "rb").read()
    _, version, offset, archive_flags, folder_count, _, _, _, _ = struct.unpack_from("<4sIIIIIIIH", data, 0)
    if version != 105:
        sys.exit(f"{bsa_path}: not a Skyrim SE archive (version {version})")

    pos = offset
    counts = []
    for _ in range(folder_count):
        _, count, _, _ = struct.unpack_from("<QIIQ", data, pos)
        pos += 24
        counts.append(count)

    records = []
    for count in counts:
        length = data[pos]
        folder = data[pos + 1:pos + length].rstrip(b"\0").decode("latin1")
        pos += 1 + length
        for _ in range(count):
            _, size, file_offset = struct.unpack_from("<QII", data, pos)
            pos += 16
            records.append((folder, size, file_offset))

    compressed_by_default = bool(archive_flags & 0x4)
    embedded_names = bool(archive_flags & 0x100)
    for folder, size, file_offset in records:
        end = data.index(b"\0", pos)
        name = data[pos:end].decode("latin1")
        pos = end + 1
        if f"{folder}\\{name}".lower() != wanted:
            continue

        compressed = compressed_by_default ^ bool(size & 0x40000000)
        size &= 0x3FFFFFFF
        if embedded_names:
            length = data[file_offset]
            file_offset += 1 + length
            size -= 1 + length
        blob = data[file_offset:file_offset + size]
        return lz4.frame.decompress(blob[4:]) if compressed else blob

    sys.exit(f"{wanted} not found in {bsa_path}")


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    texture = read_bsa_file(os.path.join(sys.argv[1], ARCHIVE), TEXTURE)
    os.makedirs(os.path.dirname(OUTPUT), exist_ok=True)
    with open(OUTPUT, "wb") as out:
        out.write(texture)
    print(f"wrote {os.path.normpath(OUTPUT)} ({len(texture)} bytes)")


if __name__ == "__main__":
    main()
