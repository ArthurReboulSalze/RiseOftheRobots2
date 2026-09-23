# 03 — Environnement & chaîne d'outils

## Chemins

| Outil | Chemin | Notes |
|---|---|---|
| Ghidra | `C:\Standalones\ghidra_12.1.4_PUBLIC` | v12.1.4, pas de loader LE/LX natif |
| Projet Ghidra | `K:\Projects\RiseOftheRobots2\Ghidra_Project` | projet `ROTR2_Reverse`, partagé GUI/headless |
| JDK (pour Ghidra) | `C:\Program Files\Eclipse Adoptium\jdk-25.0.4.101-hotspot` | le `java` par défaut du PATH est un JRE 1.8 → toujours exporter `JAVA_HOME` |
| PyGhidra | pip `pyghidra 3.1.0` | pour les scripts `.py` headless (Ghidra 12 n'embarque plus Jython) |
| Python | 3.14.5 (PATH) | parsers, outils |
| DOSBox | `C:\Program Files (x86)\DOSBox-0.74-3\DOSBox.exe` | 0.74-3, référence de comportement |

## Ghidra headless — commande de base

```bash
export JAVA_HOME="/c/Program Files/Eclipse Adoptium/jdk-25.0.4.101-hotspot"
"/c/Standalones/ghidra_12.1.4_PUBLIC/support/analyzeHeadless.bat" \
    "K:\Projects\RiseOftheRobots2\Ghidra_Project" ROTR2_Reverse \
    -import "K:\...\binaire" \
    -analysisTimeoutPerFile 300
```

Contraintes :
- **Fermer la GUI Ghidra** avant une passe headless sur le même projet (accès exclusif).
- Scripts Python : `TOOLS/ghidra_scripts/*.py` + `-scriptPath "K:\Projects\RiseOftheRobots2\TOOLS\ghidra_scripts" -postScript NomDuScript.py [args]`.
- Exports d'analyse → `ANALYSIS/` (décompilé, listes de fonctions, strings).

## DOSBox — commande de test

```bash
"/c/Program Files (x86)/DOSBox-0.74-3/DOSBox.exe" -conf ANALYSIS/dosbox_rotr2.conf
```

Config de référence : `ANALYSIS/dosbox_rotr2.conf` (montage du dossier source, lancement du lanceur, machine svga_s3, memsize 16).

## Python — conventions

- Python 3.14, stdlib d'abord (pas de dépendances lourdes) ; Pillow si besoin pour PNG.
- Parsers dans `TOOLS/`, sorties dans `EXTRACTED/`.

## Pipeline images validé

Python 3.14.5 et Pillow 12.3.0 utilisés pour les exports de cette session.
Les nouveaux outils résolvent la racine depuis leur propre emplacement.

```powershell
python TOOLS/extract_ggf.py
python TOOLS/extract_anr.py
python TOOLS/build_image_gallery.py
python TOOLS/test_image_codecs.py
python TOOLS/verify_images.py
```

Unicorn 2.1.4, facultatif, installé uniquement dans `ANALYSIS/verification_deps/`
pour `TOOLS/verify_x86_images.py`. La galerie `EXTRACTED/index.html` s'ouvre
directement dans un navigateur ; elle n'a pas de dépendance réseau.
