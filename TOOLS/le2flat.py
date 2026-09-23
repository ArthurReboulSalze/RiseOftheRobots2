"""Prepare a private flat LE image for Ghidra; see prepare_analysis.py for profiles."""
import json

from le_codec import flatten
from project_paths import SOURCE, ANALYSIS


def main():
    image, metadata = flatten((SOURCE / "RISE2.EXR").read_bytes())
    ANALYSIS.mkdir(parents=True, exist_ok=True)
    (ANALYSIS / "RISE2_flat.bin").write_bytes(image)
    (ANALYSIS / "RISE2_le_metadata.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    report = (f"Base={metadata['base']:#x}, entree={metadata['entry']:#x}, pages={metadata['pages']}\n"
              f"Fixups={metadata['fixup_records']}, adresses relogees={metadata['patched_addresses']}\n"
              f"Selecteurs annotes, attribues par DOS a l'execution : {len(metadata['selector_fixups'])}\n")
    (ANALYSIS / "RISE2_le_report.txt").write_text(report, encoding="utf-8")
    print(report)


if __name__ == "__main__":
    main()
