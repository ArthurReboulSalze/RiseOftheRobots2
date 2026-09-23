"""Build an offline image browser. Open EXTRACTED/index.html in any browser."""

import argparse
import json
from pathlib import Path
from extract_ggf import ROOT


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--extracted', type=Path, default=ROOT / "EXTRACTED")
    extracted = parser.parse_args().extracted
    catalog = json.loads((extracted / "sprites/catalog.json").read_text(encoding="utf-8"))
    banks = []
    for entry in catalog:
        m = json.loads((extracted / "sprites" / entry["bank"] / "manifest.json").read_text(encoding="utf-8"))
        banks.append({"name":m['bank'], "size":m['source_size'], "pages":m['pages'],
                      "frames":[[f['page'],*f['rect'],*f['origin'],int(f['uses_opponent_palette'])] for f in m['frames']]})
    ggf = json.loads((extracted / 'ggf/manifest.json').read_text(encoding='utf-8'))
    data = {"banks":banks, "backgrounds":[f['file'][:-4] for f in ggf['files'] if f['status']=='ok'],
            "ggf_count":ggf['decoded'], "frame_count":sum(len(b['frames']) for b in banks)}
    template = (ROOT / "TOOLS/templates/image_gallery.html").read_text(encoding="utf-8")
    (extracted / 'index.html').write_text(template.replace('__DATA__', json.dumps(data,separators=(',',':'))),encoding='utf-8')
    print(extracted / 'index.html')


if __name__ == '__main__':
    main()
