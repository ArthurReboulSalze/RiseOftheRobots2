# Décodeur LZW reproduisant EXACTEMENT FUN_0003C731/3C9D0/3CAAC (RISE2.EXR), vérifié sur l'assembleur.
# - bits LSB-first ; largeur 9 -> 12 (doublement de limite quand next_code >= limite)
# - 0x100 = CLEAR (réinit, puis lit le code suivant sur 9 bits = littéral 0-255)
# - 0x101 = EOI ; premier code libre 0x102 ; dictionnaire préfixe/suffixe (256 mots chacun)
# - prev_code initial = 0 ; après CHAQUE code (y compris le premier) : ajout
#   table_suffix[next]=root(chaîne courante), table_prefix[next]=prev_code ; next_code++
# - KwKwK : code >= next_code -> chaîne = decode(prev_code) + root(prev_code)


class BitReader:
    def __init__(self, data, bit_pos=0):
        if not 0 <= bit_pos <= len(data) * 8:
            raise ValueError("Position de lecture hors du flux")
        self.data = data
        self.n = len(data)
        self.pos = bit_pos

    def read(self, count):
        v = 0
        for i in range(count):
            byte = self.pos >> 3
            if byte >= self.n:
                raise EOFError
            bit = (self.data[byte] >> (self.pos & 7)) & 1
            v |= bit << i
            self.pos += 1
        return v


class LZW:
    def __init__(self, data, start_offset=0):
        self.bits = BitReader(data, start_offset * 8)

    def decode(self, max_out=1 << 26):
        if max_out < 0:
            raise ValueError("Limite de sortie négative")
        width = 9
        limit = 0x200
        next_code = 0x102
        suffix = list(range(256)) + [0] * (0x1000 - 256)
        prefix = [0] * 0x1000
        prev_code = 0
        prev_root = 0
        out = bytearray()
        while True:
            code = self.bits.read(width)
            if code == 0x101:
                break
            if code == 0x100:
                width = 9
                next_code = 0x102
                limit = 0x200
                code = self.bits.read(width)   # le code suivant est lu sur 9 bits aussi
                if code == 0x101:
                    break
                if code > 0xFF:
                    raise ValueError("CLEAR doit être suivi d'un littéral")
                out.append(code & 0xFF)
                prev_code = code
                prev_root = code & 0xFF
            else:
                cur = code
                string = bytearray()
                if cur > next_code:
                    raise ValueError(f"Code LZW invalide : {cur} > {next_code}")
                if cur == next_code:           # KwKwK
                    cur = prev_code
                    string.append(prev_root & 0xFF)
                while cur >= 0x102:
                    string.append(suffix[cur])
                    cur = prefix[cur]
                    if len(string) > 0x10000:
                        raise ValueError("chaîne cyclique")
                string.append(cur & 0xFF)
                string.reverse()
                out += string
                if next_code < limit:
                    prefix[next_code] = prev_code
                    suffix[next_code] = string[0]
                    prev_root = string[0]
                    next_code += 1
                    prev_code = code
                if next_code >= limit and width < 12:
                    limit <<= 1
                    width += 1
            if len(out) > max_out:
                raise ValueError(f"Sortie LZW supérieure à la limite ({max_out})")
        return bytes(out)


if __name__ == "__main__":
    import sys, os
    from project_paths import SOURCE
    for arg in sys.argv[1:]:
        name, _, off = arg.partition("@")
        p = str(SOURCE / name)
        d = open(p, 'rb').read()
        for s in ([int(off)] if off else (0, 1, 2)):
            try:
                dec = LZW(d, s).decode()
                print("%s@%d: %d -> %d octets ; début=%s" % (name, s, len(d), len(dec), dec[:32].hex(' ')))
            except Exception as e:
                print(name, s, "ERR", type(e).__name__, str(e)[:60])
