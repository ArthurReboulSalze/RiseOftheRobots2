"""Copy the data track of a BIN/CUE disc to a mountable ISO9660 image."""
import argparse
import hashlib
import os
from pathlib import Path
import tempfile

from disc_image import SectorReader, read_cue


def convert(cue, output):
    cue, output = Path(cue).resolve(), Path(output).resolve()
    tracks = [track for track in read_cue(cue) if track.mode != "AUDIO"]
    if len(tracks) != 1:
        raise ValueError("Une unique piste de données est nécessaire pour créer cette ISO.")
    if output.exists():
        raise FileExistsError(f"Image déjà présente : {output}")
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    sha256 = hashlib.sha256()
    try:
        with tempfile.NamedTemporaryFile(prefix=".rise2-iso-", suffix=".part",
                                         dir=output.parent, delete=False) as dest:
            temporary = Path(dest.name)
            with SectorReader(tracks[0]) as source:
                while block := source.read(1024 * 1024):
                    dest.write(block)
                    sha256.update(block)
        with temporary.open("rb") as image:
            image.seek(16 * 2048)
            descriptor = image.read(7)
        if descriptor != b"\x01CD001\x01":
            raise ValueError("La piste de données ne contient pas de volume ISO9660 valide.")
        if output.exists():
            raise FileExistsError(f"Image créée entre-temps : {output}")
        os.rename(temporary, output)
        return {"path": str(output), "bytes": output.stat().st_size,
                "sha256": sha256.hexdigest(), "track": tracks[0].number}
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cue", required=True, type=Path, help="CUE du disque, avec son BIN adjacent")
    parser.add_argument("--output", required=True, type=Path, help="Destination ISO (non écrasée)")
    args = parser.parse_args()
    try:
        result = convert(args.cue, args.output)
    except (OSError, ValueError) as exc:
        parser.exit(1, f"Conversion interrompue : {exc}\n")
    print(f"ISO de données : {result['path']} ({result['bytes']} octets)")
    print(f"SHA-256 : {result['sha256']}")
    print("Les pistes audio du CD restent dans le BIN/CUE source.")


if __name__ == "__main__":
    main()
