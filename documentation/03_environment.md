# 03 — Environment and toolchain

The paths below describe the original Windows analysis machine. Public commands should instead use each contributor's own installation paths.

| Tool | Original local setup | Role |
|---|---|---|
| Ghidra | `C:\Standalones\ghidra_12.1.4_PUBLIC` | Ghidra 12.1.4; no usable LE/LX loader in this workflow |
| Original Ghidra project | `K:\Projects\RiseOftheRobots2\Ghidra_Project` | Private `ROTR2_Reverse` project |
| JDK | Eclipse Adoptium JDK 25 | Ghidra runtime; set `JAVA_HOME` explicitly if another Java is on PATH |
| PyGhidra | 3.1.0 in the analysis environment | Python-controlled, headless Ghidra |
| Python | 3.14.5 locally | Data parsers and import tools; the public importer supports 3.12+ |
| DOSBox | 0.74-3 locally | Reference for game behavior |

## Headless Ghidra

Prefer the edition-specific private profile:

```powershell
python TOOLS/prepare_analysis.py --profile LOCAL/my-game
python TOOLS/prepare_analysis.py --profile LOCAL/my-game --ghidra "C:/Tools/ghidra" --java-home "C:/Tools/jdk"
```

The older project was also imported through Ghidra's `support/analyzeHeadless.bat` with a 300-second analysis timeout and scripts from `TOOLS/ghidra_scripts/`. Close Ghidra's GUI before using headless tools on the **same** project: it holds an exclusive project lock. Decompiler output, function lists and strings belong in private `ANALYSIS/`, never in Git.

## DOSBox reference run

The original machine used DOSBox 0.74-3 with `ANALYSIS/dosbox_rotr2.conf` to mount the source copy and run its launcher in `svga_s3` mode with 16 MB of memory. That configuration and the game data are private; the port does not require DOSBox to build.

## Python and image pipeline

The analysis scripts favor the standard library; image export uses Pillow. Python 3.14.5 and Pillow 12.3.0 were used for the recorded exports. Scripts locate the repository from their own path, so a public checkout need not use the original drive letter.

```powershell
python TOOLS/extract_ggf.py
python TOOLS/extract_anr.py
python TOOLS/build_image_gallery.py
python TOOLS/test_image_codecs.py
python TOOLS/verify_images.py
```

For a new import, use `TOOLS/import_game.py` rather than relying on historical default paths. Optional Unicorn 2.1.4 was placed in private `ANALYSIS/verification_deps/` for `TOOLS/verify_x86_images.py`. The generated `EXTRACTED/index.html` gallery opens locally without a network service.
