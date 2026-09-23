# 01 — Vue d'ensemble du projet

## Objectif

Portage de **RISE 2: Resurrection** (1996, Mirage Technologies / Acclaim Entertainment) — jeu de combat 2D DOS, vers des plateformes modernes (cible principale : Windows).

## Nature du jeu (pour orienter le portage)

- Jeu de combat **2D** en vue de face, deux combattants à l'écran.
- **Fonds animés : boucles vidéo pré-calculées** (stream vidéo en boucle) — pas de rendu temps réel du décor.
- **Combattants : images pré-rendues 3D** — planches fixes d'images animées (frames d'animations 3D calculées à l'avance, comme le jeu original).
- Conséquence : le portage n'a **pas besoin de moteur 3D** ; il consomme des planches d'images + des scripts d'animation, comme le jeu original.

## Inventaire des sources (`SRC/`)

| Élément | Rôle |
|---|---|
| `DOS_version_install_cracked/` | Ancienne copie DOS, identique à l’ancienne extraction ISO ; 28 robots, cinématiques longues retirées. Référence des adresses historiques. |
| `ISO_version_install/` | Extraction de l'ISO — mêmes fichiers. Ne sert qu'à corroborer. |
| `DOS_Installed_Files/ACCLAIM/RISE2/` | État après installation : `RISE2.CFG` (config binaire : touches/options) + `HISCORE.DAT` (high scores). |
| `Rise 2 Directors Cut/Disc 1/` | BIN/CUE du jeu : 1 122 fichiers dans RISE2, 30 robots, 105 ANI, neuf pistes audio. Base retenue pour les prochaines extractions. |
| `Rise 2 Directors Cut/Disc 2/` | Bonus FLC/GIF/WAV/documents ; non requis pour le port. |

### Structure du jeu (ancienne copie = `SRC/DOS_version_install_cracked/`)

- `RISE2.EXE` (19 724 octets) — **lanceur 16 bits Watcom** : cherche `dos4gw.exe`, **patche `RISE2.EXR`**, puis l'exécute sous DOS/4GW. Ce n'est PAS le jeu.
- `RISE2/RISE2.EXR` (421 979 octets) — **le vrai binaire du jeu** : exécutable **LE (Linear Executable) 32 bits DOS/4GW** (magic `LE` à l'offset `e_lfanew = 0xAD8`).
- `RISE2/DOS4GW.EXE` (265 Ko) — extendeur DOS/4GW Rational.
- `RISE2/RISE2.CFG` etc. — voir [05_formats_donnees.md](05_formats_donnees.md) pour l'inventaire des formats.

### Constats clés

1. Les deux anciennes distributions (dossier DOS / extraction ISO) sont identiques.
   La Director’s Cut ajoutée ensuite est différente : ne pas mélanger ses banques
   ou ses adresses d’exécutable avec celles de l’ancienne copie. Voir document 11.
2. Le lanceur **patche `RISE2.EXR`** avant exécution (chaînes : `Patching main executable from C:\ACCLAIM\RISE2\RISE2.EXR`, `%s\RISE2.EXR`, `.\rise2\rise2.exr`) — à comprendre en priorité : le binaire final exécuté est l'EXR **patché**, donc l'analyse de l'EXR devra tenir compte du patch.
3. **Ghidra 12.1.4 n'a pas de loader LE/LX natif** → le loader maison `TOOLS/le2flat.py` et l'import PyGhidra sont maintenant réalisés. L'image de ce jeu commence à **0x10000**. Les routines d'images ont été vérifiées par exécution x86 ; voir les documents 06 et 07.
4. Messages d'erreur lisibles dans `RISE2/ERRORS.TXT` (numérotés, ex. « Error reading GGF ») : points d'ancrage excellents pour cartographier le code du binaire.

## État de la chaîne d'outils

Voir [03_environnement.md](03_environnement.md).

## Résultat actuel : images décodées

185 GGF lisibles, 28 robots dans les deux résolutions et 56 portraits exportés
(18 522 frames sur 139 planches). Galerie : `EXTRACTED/index.html`.
Formats corrigés, validation et limites : [07_decodage_images.md](07_decodage_images.md).
