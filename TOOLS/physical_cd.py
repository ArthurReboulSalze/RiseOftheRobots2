"""Read-only Windows CDDA import; data files are copied from the mounted drive.

API: IOCTL_CDROM_READ_TOC and IOCTL_CDROM_RAW_READ (Microsoft ntddcdrm.h).
DiskOffset uses 2048-byte units even when the returned audio sectors are 2352 bytes.
"""
import os
from pathlib import Path
import struct
import wave


def parse_toc(raw):
    if len(raw) < 4:
        raise ValueError("Truncated CD TOC.")
    size = int.from_bytes(raw[:2], "big") + 2
    first, last = raw[2:4]
    count = last - first + 1
    if not 1 <= first <= last <= 99 or size < 4 + (count + 1) * 8 or len(raw) < size:
        raise ValueError("Inconsistent CD TOC or missing lead-out.")
    entries = []
    for i in range(count + 1):
        entry = raw[4 + i * 8:12 + i * 8]
        m, s, frame = entry[5:8]
        if s >= 60 or frame >= 75:
            raise ValueError("Invalid MSF address in the TOC.")
        entries.append({"number": entry[2], "data": bool(entry[1] & 4),
                        "start": (m * 60 + s) * 75 + frame - 150})
    if entries[-1]["number"] != 0xAA:
        raise ValueError("TOC is missing lead-out.")
    tracks = []
    for i, entry in enumerate(entries[:-1]):
        end = entries[i + 1]["start"]
        if entry["number"] != first + i or not 0 <= entry["start"] < end:
            raise ValueError("CD tracks are out of order.")
        tracks.append({**entry, "end": end})
    return tracks


def write_audio_tracks(tracks, read_sectors, destination, log=print):
    destination = Path(destination)
    destination.mkdir(parents=True, exist_ok=True)
    records = []
    for track in tracks:
        if track["data"]:
            continue
        number = track["number"]
        log(f"  Extracting physical CD track {number:02}")
        target = destination / f"{number:02}.wav"
        with wave.open(str(target), "wb") as out:
            out.setparams((2, 2, 44100, 0, "NONE", "not compressed"))
            sector = track["start"]
            while sector < track["end"]:
                count = min(16, track["end"] - sector)
                for attempt in range(3):
                    try:
                        block = read_sectors(sector, count)
                        if len(block) != count * 2352:
                            raise OSError("Incomplete CD audio read.")
                        break
                    except OSError:
                        if attempt == 2:
                            raise
                out.writeframesraw(block)
                sector += count
        records.append({"number": number, "sectors": track["end"] - track["start"], "file": target.name})
    return records


def rip_windows_cd(drive, destination, log=print):
    if os.name != "nt":
        raise ValueError("Direct CDDA extraction is available on Windows; elsewhere, provide a BIN/CUE or separate audio rip.")
    import ctypes
    from ctypes import wintypes
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p,
                                  wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
    kernel.CreateFileW.restype = wintypes.HANDLE
    kernel.DeviceIoControl.argtypes = [wintypes.HANDLE, wintypes.DWORD, ctypes.c_void_p, wintypes.DWORD,
                                      ctypes.c_void_p, wintypes.DWORD, ctypes.POINTER(wintypes.DWORD), ctypes.c_void_p]
    kernel.DeviceIoControl.restype = wintypes.BOOL
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    device = "\\\\.\\" + str(drive).rstrip("\\/")
    handle = kernel.CreateFileW(device, 0x80000000, 3, None, 3, 0, None)
    if handle == ctypes.c_void_p(-1).value:
        raise ctypes.WinError(ctypes.get_last_error())

    def ioctl(code, request, size):
        inp = ctypes.create_string_buffer(request) if request else None
        out = ctypes.create_string_buffer(size)
        transferred = wintypes.DWORD()
        if not kernel.DeviceIoControl(handle, code, inp, len(request), out, size, ctypes.byref(transferred), None):
            raise ctypes.WinError(ctypes.get_last_error())
        return out.raw[:transferred.value]

    try:
        tracks = parse_toc(ioctl(0x24000, b"", 804))
        return write_audio_tracks(tracks, lambda sector, count: ioctl(
            0x2403E, struct.pack("<qII", sector * 2048, count, 2), count * 2352), destination, log)
    finally:
        kernel.CloseHandle(handle)
