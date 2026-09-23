# 05 — Formats de données du jeu

Référence : `SRC/DOS_version_install_cracked/RISE2/`. Catalogue brut :
`ANALYSIS/catalog.md` / `catalog.csv`.

**Mise à jour du 22 septembre 2026 :** les anciennes identifications de la géométrie
GGF et du parseur ANR étaient erronées. Le format exact des images et les preuves
sont maintenant détaillés dans [07_decodage_images.md](07_decodage_images.md).

## Formats d'images validés

| Extension | Nombre | Structure et état |
|---|---|---|
| `.GGF` | 188 | Flag u8, palette de 768 octets si flag non nul, flux LZW, bourrage zéro. 185 images : 91 en 400×200, 93 en 800×400, A6K en 640×400. A6B/A6G/A6M sont des fragments invalides. |
| `.ANL` | 126 | u16 nombre de frames puis autant d'offsets u32 little-endian dans ANR. |
| `.ANR` | 126 | Segments horizontaux de pixels, coordonnées absolues ou relatives compactes ; u16 0xffff termine chaque frame. 20 969 frames validées. |
| `.PAL` | 32 | RGB VGA 6 bits. R?.PAL : 80 entrées, dont 70 chargées par joueur. VSFACE.PAL / ROBOTS.PAL : 28 blocs de 70 couleurs. CHIPS.PAL : 240 couleurs. EXTRA.PAL : en-tête à analyser. |

Les 28 slots fournis suivent `ABCDEFGHIJKLMNOPQRSTUVWXYZ01`. L'alphabet complet
embarqué dans l'EXR à 0x5087c est `ABCDEFGHIJKLMNOPQRSTUVWXYZ012345`.

Familles ANL/ANR :

- `RBT?` : combattants, canevas 640×400 ; `RB4?` : versions 320×200.
- `VSFACE`, `V4FACE` : portraits haute/basse résolution.
- `BGANIM?`, `BGANI4?` : éléments animés des décors, même codec de segments.
- `CHIPS/CHIPL`, `CURSOR/CURSOL`, `EXTRA/EXTR4`, `HANDICAP/HANDICAL`,
  `POPSCRNS/POPSCRNL`, `RBAN/RBAL` : autres banques ; segments validés,
  association des palettes contextuelles et rôle précis à poursuivre.

`TOOLS/extract_anr.py` exporte les 56 banques de combattants et les 2 banques de
portraits, soit 18 522 frames. Les planches couleur, indices de palette et positions
sont disponibles dans `EXTRACTED/sprites/`. La transparence dépend de l'absence de
segment, **pas d'un indice de couleur particulier**.

## AIP — structure correctement attribuée

`FUN_35247` lit `RBTn.AIP` (chaîne @0x62ad8), et non ANR. Les 28 AIP contiennent :

```text
u32 N1, u32 N2
N1 × record de 14 octets
N2 × record de 4 octets
```

Record de 14 octets : u32, trois u8, trois u16, un u8. Il est développé en 20
octets en mémoire. Exemple RBT0.AIP : N1=29, N2=42 ; 8+29×14+42×4=582 octets.
La structure est validée ; le lien entre ses champs, les états et l'IA est décrit
partiellement dans [08_moteur.md](08_moteur.md), avec des détails encore ouverts.

## Autres formats — état actuel

| Extension | Nombre | Observations confirmées / limites |
|---|---|---|
| `.STS` | 28 | Tables de 1 344 octets, chargées par robot ; valeurs 16 bits et sentinelles FFFF. |
| `.MVS` | 30 | 96 mouvements par banque : trois séquences, trois déplacements, transitions et quatre octets de contrôle par descripteur. `RBMG`/`RBMN` ont des pointeurs big-endian. Voir le détail dans [09_formats_restants.md](09_formats_restants.md). |
| `.A0C`–`.A5C` | 28×6 | Masques de 500 octets utilisés pour sélectionner les frames à charger dans `FUN_1a23f`. |
| `.MRW` | 69 | Banques de 1 142 échantillons PCM 8 bits non signés, mono : u16 nombre, puis paires u32 offset/taille. WAV exportés dans `EXTRACTED/audio/mrw/`. |
| `.MRS` | 68 | Séquences qui pilotent les MRW ; tailles de 48 à 21 156 octets. Pitch et déclenchement exacts restent à finaliser. |
| `.CL2` | 52 | Magic `CLL5` ou `CLL6`, boîtes de collision validées ; 28 banques robots, autres banques de décor. Voir [08_moteur.md](08_moteur.md). |
| `.CTL` | 23 | Scripts de défilement et d'animation des décors ; détails dans [08_moteur.md](08_moteur.md). |
| `.DAT` | 6 | CHRSET1–3 (3 456 octets), OPTIONS, SOUND (524 octets), OPTW95. `SOUND.DAT` configure les cartes son et leurs pilotes. |
| `.ANI` | 3 | LLOGO, END, ENL : codec RLE/deltas décodé, 161 images exportées. Les cinématiques longues sont absentes de cette distribution (`ENERGY.NFO`) ; `RQLINK.ANI` manque. Voir [09_formats_restants.md](09_formats_restants.md). |
| `.TXT` / `.FRA` / autres langues | — | Textes et messages lisibles, tags de mise en forme. |
| `.ISW`, `.BIN`, `.RAW` | 1 chacun | OPTIONS.ISW, BG.BIN, TESTD.RAW : à analyser. |
| `.RST` | 1 | STATE.RST : INI ASCII. |
| `.LST` | 1 | EXTRA.LST : liste ASCII. |
| `.GLL` | 1 | GRIP.GLL : structure à identifier. |
| `.386` | 3 | Drivers audio HMIDET/HMIDRV/HMIMDRV. |

## Points d'entrée binaires

| Fonction | Rôle |
|---|---|
| `3c731`, `3c9d0`, `3caac` | LZW GGF, palette, lecteur de bits |
| `1a0fc`, `1a23f` | Chargement ANL/ANR complet ou sélectif |
| `1c819` | Décodage des segments et rendu sprites |
| `20dda` | Décalage de palette du second combattant (+70 si indice <70) |
| `1a473` | Chargement des deux palettes de 70 couleurs |
| `35247` | Parseur AIP |
| `23e97` | Chargement MVS/STS |
| `1b304`, `1b39e`, `1b3e6` | Open, read, close DOS |
| `121c3` | Lecture complète d'un fichier |

## Prochaines étapes

- [x] Structures AIP/MRW/MVS/STS/CTL/CL2 extraites vers `EXTRACTED/data/*.json` (session 4).
- [ ] Finaliser les champs encore inconnus d'AIP/STS et les séquences MRS.
- [x] MVS → JSON ; MRW → WAV PCM ; CL2 → JSON. Voir les documents 08 et 09.
- [ ] Affiner les scripts CTL et l'association contextuelle des autres banques ANR.
- [ ] Exporter les autres banques ANR avec palettes contextuelles et les polices CHRSET ; chercher une source non-rippée pour les ANI manquants.

Les hypothèses précédentes restent consultables dans le journal historique, mais
ne doivent pas être utilisées à la place des structures validées ci-dessus.
