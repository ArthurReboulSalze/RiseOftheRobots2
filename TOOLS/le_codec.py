"""Bounded LE loader for internal fixups in the two DOS Rise 2 editions.

Format reference: Open Watcom bld/watcom/h/exeflat.h. No original code embedded.
Selector fixups are recorded, not guessed: DOS allocates selectors at load time.
"""
import hashlib
import struct


def flatten(data):
    def take(offset, size):
        if offset < 0 or size < 0 or offset + size > len(data):
            raise ValueError(f"Executable LE tronqué à {offset:#x}")
        return data[offset:offset + size]
    if take(0, 2) != b"MZ":
        raise ValueError("Executable DOS MZ attendu")
    header = struct.unpack("<I", take(60, 4))[0]
    if take(header, 4) != b"LE\0\0":
        raise ValueError("Executable LE little-endian attendu")
    def u32(relative):
        return struct.unpack("<I", take(header + relative, 4))[0]
    page_count, page_size = u32(0x14), u32(0x28)
    last_size, data_offset = u32(0x2c), u32(0x80)
    object_count, object_table = u32(0x44), u32(0x40)
    if not 1 <= page_count <= 100000 or not 1 <= page_size <= 65536 or not 1 <= object_count <= 1024:
        raise ValueError("Géométrie LE non prise en charge")
    if u32(0x74):
        raise ValueError("Imports LE externes non pris en charge")
    objects = []
    for i in range(object_count):
        size, base, flags, first, count, _ = struct.unpack("<6I", take(header + object_table + i * 24, 24))
        if count and not 1 <= first <= first + count - 1 <= page_count:
            raise ValueError("Pages d'objet LE hors bornes")
        objects.append(dict(size=size, base=base, flags=flags, first_page=first, page_count=count))
    base = min(o["base"] for o in objects)
    end = max(o["base"] + o["size"] for o in objects)
    if not 0 < end - base <= 64 * 1024**2:
        raise ValueError("Image LE trop grande")
    ordered = sorted(objects, key=lambda o: o["base"])
    if any(a["base"] + a["size"] > b["base"] for a, b in zip(ordered, ordered[1:])):
        raise ValueError("Objets LE qui se chevauchent")
    page_map = take(header + u32(0x48), page_count * 4)
    pages = [(int.from_bytes(page_map[i:i+3], "big"), page_map[i+3]) for i in range(0, len(page_map), 4)]
    final_physical_page = max(n for n, _ in pages)
    image = bytearray(end - base)
    owners = {}
    for object_index, obj in enumerate(objects):
        for local_page in range(obj["page_count"]):
            global_page = obj["first_page"] - 1 + local_page
            if global_page in owners:
                raise ValueError("Page LE attribuée à plusieurs objets")
            owners[global_page] = (object_index, local_page)
            number, flags = pages[global_page]
            if flags == 3:
                continue
            if flags or number < 1:
                raise ValueError(f"Type de page LE non pris en charge : {flags}")
            size = (last_size or page_size) if number == final_physical_page else page_size
            chunk = take(data_offset + (number - 1) * page_size, size)
            count = min(size, obj["size"] - local_page * page_size)
            if count <= 0:
                raise ValueError("Page hors de la taille de son objet")
            target = obj["base"] - base + local_page * page_size
            image[target:target + count] = chunk[:count]
    fix_offsets = struct.unpack(f"<{page_count + 1}I", take(header + u32(0x68), (page_count + 1) * 4))
    if any(a > b for a, b in zip(fix_offsets, fix_offsets[1:])):
        raise ValueError("Table de fixups LE non croissante")
    fix_base = header + u32(0x6c)
    take(fix_base, fix_offsets[-1])
    selectors, writes = [], {}
    records = 0
    for page in range(page_count):
        pos, stop = fix_base + fix_offsets[page], fix_base + fix_offsets[page + 1]
        def field(size):
            nonlocal pos
            if pos + size > stop:
                raise ValueError("Record de fixup LE tronqué")
            result = take(pos, size)
            pos += size
            return result
        while pos < stop:
            source_type, flags = field(2)
            if source_type not in (2, 7, 8) or flags & ~0x50:
                raise ValueError(f"Fixup LE non pris en charge : type={source_type:#x}, flags={flags:#x}")
            source_offset = struct.unpack("<h", field(2))[0]
            object_id = int.from_bytes(field(2 if flags & 0x40 else 1), "little")
            target_offset = 0 if source_type == 2 else int.from_bytes(field(4 if flags & 0x10 else 2), "little")
            if page not in owners or not 1 <= object_id <= len(objects):
                raise ValueError("Objet de fixup LE invalide")
            owner, local_page = owners[page]
            source_object = objects[owner]
            source = source_object["base"] + local_page * page_size + source_offset
            width = 2 if source_type == 2 else 4
            if not source_object["base"] <= source <= source_object["base"] + source_object["size"] - width:
                raise ValueError("Adresse source de fixup hors objet")
            records += 1
            if source_type == 2:
                selectors.append({"address": source, "target_object": object_id})
                continue
            target = objects[object_id - 1]["base"] + target_offset
            value = (target - source - 4 if source_type == 8 else target) & 0xffffffff
            if source in writes and writes[source] != value:
                raise ValueError("Fixups contradictoires sur une adresse")
            writes[source] = value
    for address, value in writes.items():
        struct.pack_into("<I", image, address - base, value)
    entry_object = u32(0x18)
    if not 1 <= entry_object <= len(objects) or u32(0x1c) >= objects[entry_object - 1]["size"]:
        raise ValueError("Point d'entrée LE hors objet")
    metadata = {"source": "RISE2.EXR", "source_sha256": hashlib.sha256(data).hexdigest(),
                "base": base, "entry": objects[entry_object - 1]["base"] + u32(0x1c),
                "objects": objects, "pages": page_count, "fixup_records": records,
                "patched_addresses": len(writes), "selector_fixups": selectors,
                "skipped_fixups": 0, "invalid_segments": 0}
    return bytes(image), metadata
