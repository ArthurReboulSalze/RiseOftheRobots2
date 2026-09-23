# 06 — Analyse des binaires

## `RISE2.EXE` — lanceur (19 724 octets)

**Nature** : stub DOS 16 bits Watcom C/C++16 (runtime WATCOM visible en chaînes). Ce n'est pas le jeu.

**Flux reconstruit** (décompilé `ANALYSIS/launcher_decompiled.c`, `main` = `FUN_1000_0061`) :

1. Init CRT Watcom + scan d'environnement.
2. `getenv`-like (vraisemblablement `DOS4GPATH` / recherche de `dos4gw.exe`, chaînes présentes : `DOS4GPATH`, `dos4gw.exe`, `rise2\dos4gw.exe`).
3. Ouverture de `RISE2\RISE2.EXR` (chemin relatif `.\rise2\rise2.exr` en défaut, sinon recherche dans les chemins).
4. Affichage du message `Patching main executable from <chemin>\RISE2.EXR` (statut, pas une erreur).
5. `exec` de `dos4gw.exe` avec `RISE2.EXR` (chaînes d'erreur exec : « Stub exec failed », « Bad format on exec »…).

**Patch sur EXR** : l'offset fichier **0x52421** de `RISE2.EXR` contient la chaîne 16 octets `C:\ACCLAIM\RISE2` (chemin d'installation), entourée de `\r\n\0\0` + `FF×6 00×2` + compteurs — cible plausible du patch du lanceur. **Vérification dynamique** : après une session complète sous DOSBox, l'EXR sur disque est inchangé (MD5 identique avant/après) → patch appliqué en mémoire et/ou no-op quand le chemin par défaut convient. À confirmer en Phase 2 (non bloquant).

**Contexte** : `RISE2.EXR` résout ses données par : variable d'env `MIRAGEGAME` (présente dans ses chaînes), recherche relative `RISE2` / `\RISE2` depuis son répertoire, fallback chemin embarqué. Fichiers référencés dans l'EXR : `RISE2.CFG`, `CHRSET3.DAT`, `TESTD.RAW`, `RQLINK.ANI` (absent des données — voir journal).

## `RISE2.EXR` — binaire principal (421 979 octets)

- Format : **LE (Linear Executable)** DOS/4GW 32 bits.
  - `e_lfanew = 0xAD8`, magic `LE` à cet offset.
  - Ghidra 12.1.4 : import via le loader maison `TOOLS/le2flat.py`, réalisé et utilisé ci-dessous ; l'image réelle commence à **0x10000**, pas à la base 0x400000 initialement supposée.
- Chaînes notables (premier scan) : `File missing`, `MIRAGEGAME`, `RISE2`, `\RISE2`, `Please run the…`, `CHRSET3.DAT`, `wb`, `RISE2.CFG`, `TESTD.RAW`.

## `DOS4GW.EXE` (265 420 octets)

Extendeur Rational DOS/4GW, exécuté par le lanceur avec `RISE2.EXR`.

## Artefacts d'analyse

- `ANALYSIS/launcher_decompiled.c` — décompilé complet du lanceur (137 fonctions).
- `ANALYSIS/launcher_disasm.txt` — listing assembleur du lanceur.
- `ANALYSIS/launcher_functions.txt`, `ANALYSIS/launcher_strings.txt`.
- Scripts réutilisables : `TOOLS/export_launcher.py`, `TOOLS/export_listing.py` (pipeline PyGhidra).

## Import de RISE2.EXR dans Ghidra (fait, 2026-09-22)

- `TOOLS/le2flat.py` : premier import LE → `ANALYSIS/RISE2_flat.bin`
  (406 784 o, linéaire 0x10000–0x73500). Le contrôle de fixups initial était
  incomplet ; voir la correction ci-dessous avant de refaire une analyse.
- Import PyGhidra : `TOOLS/import_exr_ghidra.py` → programme `RISE2_EXR` dans `ANALYSIS/exr_proj/rotr2_exr` (image plate préfixée de 64 Ko de zéros : **offset fichier = adresse linéaire**).
- Résultat : **529 fonctions** auto-analysées ; entry CRT Watcom @0x3EC2C décompilée (`ANALYSIS/exr_entry.c`).
- Session réutilisable : `TOOLS/ghidra_session.py` (`open_rotr2()` → project, program). Décompilation ciblée : `TOOLS/exr_decompile_at.py <hex...>` → `ANALYSIS/funcs/fn_<addr>.c`.

### Correction du loader et séparation des éditions (2026-09-23)

`TOOLS/le_codec.py` lit maintenant les **page_count + 1** offsets de fixups,
les positions source signées et les pages relatives à chaque objet. Les records
de sélecteur (type 2) n’ont pas d’offset cible ; ils sont annotés, leur valeur
étant attribuée par DOS. Types 7 = adresse 32 bits, 8 = déplacement relatif 32 bits.
Les records non pris en charge et les bornes incorrectes provoquent un échec.

| Source | Pages | Records lus | Adresses relogées uniques | Sélecteurs annotés | Entrée |
|---|---:|---:|---:|---:|---|
| Ancienne copie | 80 | 10 589 | 10 581 | 2 | 0x3EC2C |
| Director’s Cut CD 1 | 81 | 10 634 | 10 624 | 2 | 0x3F19C |

Les doublons de fixups aux frontières de pages sont contrôlés. Cinq tests
synthétiques couvrent ces cas et les troncatures. La nouvelle commande
`prepare_analysis.py --profile LOCAL/<profil>` écrit dans le profil privé ;
elle ne remplace pas l’ancien projet Ghidra. Les adresses des documents 06–09
restent celles de l’ancienne copie, jusqu’à une nouvelle cartographie Director’s Cut.
Référence du format : [en-têtes Open Watcom](https://github.com/open-watcom/open-watcom-v2/blob/master/bld/watcom/h/exeflat.h).

## Fonctions clés identifiées (adresses Ghidra, base 0x10000)

| Adresse | Rôle |
|---|---|
| `FUN_00014c04` | Séquence de chargement par robot (GGF→PAL→MVS/STS→AN→CL2) |
| `FUN_0003c731` | Décompresseur LZW + copie linéaire avec recadrage horizontal (GGF) |
| `FUN_0003c9d0` / `FUN_0003caac` | Init LZW (width 9→12, CLEAR 0x100, EOI 0x101) / lecteur de bits LSB-first |
| `FUN_00035247` | Parseur **AIP** (N1 records 14 o + N2 dwords), nom `RBTn.AIP` à 0x62ad8 |
| `FUN_0001a0fc` / `FUN_0001a23f` | Chargeurs ANL/ANR : u16 compteur + offsets u32, chargement complet/sélectif |
| `FUN_0001c819` | Blitter de sprites ; 0x1c9d6–0x1ca6d décode les 4 types de segments |
| `FUN_00020dda` | Parcourt les segments ANR et ajoute 70 aux indices <70 pour le deuxième joueur |
| `FUN_00023e97` | Parseur MVS/STS par robot (96 pointeurs auto-référentiels) |
| `FUN_0001524f` | Loader `?.AN` (renvoie valeur via `FUN_0001a23f`) |
| `FUN_0001a473` | Loader `?.PAL` (2 palettes par robot, via `FUN_0001b3f4`) |
| `FUN_0001b304` / `FUN_0001b39e` / `FUN_0001b3e6` | open / read-chunk / close DOS (INT 21h) |
| `FUN_000121c3` | Lecture complète fichier → mémoire (base `DAT_00065be0`) |
| `FUN_0001473e` | Vérification (touche/quit pendant chargement) |
| `FUN_0001b077` | Mise à jour écran de chargement |
| `DAT_00066354` | Index robot courant (base36) ; `DAT_0006639c` = mode (2 = ?) ; robots 0x13/0x16/0x18 traités par `FUN_0001aac2` |

Chaînes + refs : `ANALYSIS/exr_strings.txt` (106 chaînes définies par l'analyse — le listing brut contient plus, à définir).

Décodage et vérification des images : voir [07_decodage_images.md](07_decodage_images.md).
Les nouveaux exports `asm_fn_1c819.txt`, `asm_fn_1d387.txt`, `asm_fn_20dda.txt`,
`asm_fn_1a0fc.txt`, `asm_fn_1a23f.txt`, `asm_fn_1a473.txt`, `asm_fn_35247.txt`
conservent les instructions utilisées. `TOOLS/verify_x86_images.py` exécute ces
instructions dans Unicorn pour fournir une référence indépendante du décodeur Python.
