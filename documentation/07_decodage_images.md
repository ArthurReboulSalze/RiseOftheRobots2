# 07 — Décodage des images : résultats validés (22 septembre 2026)

Le blocage des images est levé. La galerie `EXTRACTED/index.html` permet de choisir
un robot, parcourir ses frames, afficher une boucle et ouvrir les planches PNG.
Elle fonctionne hors ligne, par double-clic dans un navigateur.

## Livrables

| Sortie | Résultat |
|---|---|
| `EXTRACTED/ggf/` | 185 PNG corrects sur 188 GGF ; 3 sources invalides consignées dans `manifest.json` |
| `EXTRACTED/sprites/RBT*/` | 28 banques haute résolution, **9 232 frames** |
| `EXTRACTED/sprites/RB4*/` | 28 banques basse résolution, **9 234 frames** |
| `EXTRACTED/sprites/VSFACE/`, `V4FACE/` | 28 portraits par résolution, **56 frames** |
| `EXTRACTED/sprites/robots.png` | Aperçu des 28 robots |
| `EXTRACTED/sprites/*/atlas_*.png` | **139 planches RGBA**, avec transparence |
| `EXTRACTED/sprites/*/indices_*.png` | Les mêmes planches en indices de palette, sans perte |
| `EXTRACTED/sprites/*/manifest.json` | Rectangles d'atlas, positions d'origine, offsets source et empreintes |
| `EXTRACTED/validation.json` | Contrôle des **126 banques ANR**, **20 969 frames**, dont 1 199 vides |
| `EXTRACTED/validation_x86.json` | Comparaison indépendante avec les instructions x86 originales |

Les 18 522 frames exportées comprennent les frames vides réellement présentes dans
les index. Les autres banques ANR (interface, effets et animations de décors) sont
décodables et leur structure a été validée ; leur export couleur contextuel reste à
intégrer. Les différences de deux frames entre RBT/RB4 proviennent de **RBTN/RB4N**
et **RBTU/RB4U**, pas d'images perdues par l'extracteur.

## Pourquoi les GGF semblaient brouillés

La décompression LZW existante était correcte pour les fichiers ordinaires, mais
le script imposait les mauvaises dimensions : 320 × 250 et 640 × 500.

| Pixels décompressés | Dimensions réelles | Nombre |
|---|---|---|
| 80 000 | **400 × 200** | 91 |
| 320 000 | **800 × 400** | 93 |
| 256 000 | **640 × 400** (`A6K.GGF`) | 1 |

Les pixels sont **linéaires, un octet par pixel**, sans réorganisation en plans VGA.
Les compteurs de `FUN_3c731` correspondent au recadrage horizontal : à haute
résolution, sauter les 80 premiers pixels, copier 640 pixels, sauter 160 pixels
(fin de ligne et début de la suivante), puis recommencer. Cela révèle une largeur
source de 800 pixels. La famille basse résolution a une largeur de 400 pixels.
L'extracteur conserve le panorama complet, utile au défilement du décor.

Attention : la branche basse résolution du blitter contient des constantes
différentes selon le chemin CLEAR ou dictionnaire. Ne pas en faire un modèle
générique de recadrage avant de porter les modes vidéo ; cela ne modifie pas
l'ordre des pixels dans les fichiers.

Structure :

```text
u8 flag
si flag != 0 : 768 octets, palette RGB VGA 6 bits
flux LZW LSB-first, CLEAR=0x100, EOI=0x101
un octet zéro de bourrage après l'octet contenant EOI (corpus fourni)
```

Après CLEAR, le littéral est lu sur **9 bits**, pas 8. L'ajout au dictionnaire après
le premier code ordinaire et son gel quand il est plein suivent le code original.
`TOOLS/lzw.py` rejette désormais les codes futurs invalides et applique réellement
la limite de taille de sortie.

### Les trois petits GGF ne constituent pas un format d'image à deviner

- `A6B.GGF` et `A6G.GGF` : 495 octets, identiques aux **495 premiers octets de
  `AGK.GGF`** (et d'autres images de cette arène). Ils s'arrêtent dans la palette,
  avant les 768 octets nécessaires. Aucun flux de pixels n'est présent.
- `A6M.GGF` : deux copies identiques de **494 octets**, chacune égale au début de
  ces mêmes fichiers. Ce sont des fragments de palette répétés, sans flux valide.

Ils sont conservés tels quels dans `SRC` et signalés comme sources invalides.
Inventer des pixels ou une géométrie pour ces fragments serait incorrect.

### Cas isolé A6K

`A6K.GGF` donne une image cohérente de 640 × 400 pixels. Son LZW utilise jusqu'à
12 bits, alors que les fichiers ordinaires testés se réinitialisent beaucoup plus
tôt. Dans l'émulation du chargeur original, les tableaux du dictionnaire se
recouvrent : l'initialisation place les préfixes seulement **0x200 octets** après les
suffixes (`0x3c781`–`0x3c795`). Le chargement d'A6K finit par corrompre l'état du lecteur.

En relogeant uniquement ces deux tableaux dans des zones séparées, **le même code
x86 décompresse A6K exactement comme Python, pixel pour pixel**. Le rapport distingue
ce diagnostic de la vérification sans modification des GGF ordinaires. La sélection
de noms dans `FUN_17f91` utilise A4/AG, pas A6 ; l'usage historique d'A6K reste inconnu.

## ANL/ANR : le véritable format des planches de sprites

La documentation précédente attribuait à ANR le parseur de `RBT?.AIP`. C'était une
erreur : la chaîne à **0x62ad8** est `RBTn.AIP`. La structure à deux compteurs et
records de 14 octets appartient à AIP, pas aux images.

Les chargeurs corrects sont **FUN_1a0fc** et **FUN_1a23f** :

```text
.ANL : u16 nombre_de_frames ; nombre_de_frames × u32 offset_dans_ANR
.ANR : concaténation de frames ; chaque frame finit par le mot 0xffff
```

L'offset de fin d'une frame est l'offset suivant dans ANL, ou la taille d'ANR pour
la dernière. Une frame réduite à `ff ff` est vide et valide. Aucun en-tête global
N1/N2 et aucune décompression LZW ne sont nécessaires pour ANR.

### Segments horizontaux

Le blitter **FUN_1c819**, en particulier **0x1c9d6–0x1ca6d**, décode des segments
opaques. Chaque segment commence par un mot `t` little-endian, puis ses coordonnées
éventuelles et ses indices de couleur. `previous_end` est la fin exclusive du segment
précédent ; `previous_y` est sa ligne.

| Condition | Position | Nombre de pixels `n` |
|---|---|---|
| `t == 0xffff` | Fin de frame | — |
| `(t & 0xc000) == 0` | Lire `a,b` ; `x=a+((t&3)<<8)` ; `y=b+((t&4)<<6)` | `t >> 3` |
| `(t & 0x8000) == 0` (bit 14 présent) | Lire `a` ; `x=a+((t&3)<<8)` ; `y=previous_y+1+((t&0x3fff)>>12)` | `(t>>2)&0x3ff` |
| Bit 15 présent, `(t&0x7000)==0` | Lire `a` ; `x=a+((t&3)<<8)` ; même `y` | `(t>>2)&0x3ff` |
| Bit 15 présent, `(t&0x7000)!=0` | `x=previous_end+((t&0x7fff)>>12)` ; même `y` | `t&0x3ff` |

Lire ensuite `n` octets de pixels ; conserver `previous_end=x+n` et `previous_y=y`.
Les zones sans segment sont transparentes. **L'indice zéro n'est pas une couleur
transparente** : un pixel zéro à l'intérieur d'un segment reste opaque. L'alpha
exporté provient des segments, indépendamment des valeurs des pixels.

Les coordonnées sont essentielles : une frame recadrée à son rectangle minimal
doit être replacée à `origin` dans le canevas de 640 × 400 (RBT) ou 320 × 200 (RB4).
Le recadrage ne doit pas recentrer chaque pose, sinon les animations sauteraient.

### Palettes et fidélité

`FUN_1a473` charge **70 couleurs** de `R?.PAL` pour chaque combattant (210 octets),
aux indices 0–69 puis 70–139. Le fichier PAL contient 80 entrées, mais ce chargeur
n'en copie que 70. `FUN_20dda` décale les indices inférieurs à 70 de +70 pour le
second joueur. Les indices supérieurs sont conservés : plusieurs frames utilisent
les couleurs communes des effets (203–239, 253) et quelques-unes utilisent la
palette de l'adversaire (notamment RBTP et RBTN).

Pour l'aperçu autonome :

- 0–69 : palette du robot ;
- 70–139 : même palette du robot, comme adversaire de référence ;
- 140–255 : palette du GGF AG correspondant, comprenant décor et effets ;
- portraits : blocs de 70 couleurs dans `VSFACE.PAL`, ordre `ABCDEFGHIJKLMNOPQRSTUVWXYZ01`.

Les manifestes identifient les frames utilisant la palette adverse. Les modifications
de teinte faites en jeu par `FUN_1a67e` ne sont pas appliquées à ces exports de base.
Les **indices originaux** sont livrés dans `indices_*.png` pour permettre au port
d'appliquer la palette du combat. Leur transparence se lit dans le canal alpha de
l'atlas RGBA correspondant. Aucun indice n'est sacrifié à une couleur de transparence.

## Vérifications réalisées

1. Les **20 969 frames des 126 banques** se terminent exactement à l'offset suivant,
   sans octet perdu, frame tronquée ou terminateur surnuméraire.
2. Relecture des **185 PNG GGF** : tailles, indices et palettes identiques au décodage.
3. Relecture des **18 522 rectangles d'atlas** : empreintes RGBA, indices et masques
   identiques aux images avant assemblage.
4. Émulation Unicorn du code x86 original : **456 frames échantillonnées dans les
   126 banques**, positions et pixels strictement identiques. Aucun décodeur Python
   n'est utilisé dans la partie de référence.
5. Décompresseur x86 complet, avec uniquement les E/S DOS simulées : **A4A, AG0, VS**
   correspondent pixel pour pixel. A6K correspond après isolation des buffers du
   dictionnaire, comme expliqué ci-dessus.
6. **9 tests de régression** : quatre types de coordonnées, bits hauts, transparence
   indépendante de la palette, frames vides, troncatures, CLEAR/KwKwK et limites LZW.

## Reproduire les exports

Depuis la racine du projet, avec Python et Pillow :

```powershell
python TOOLS/extract_ggf.py
python TOOLS/extract_anr.py
python TOOLS/build_image_gallery.py
python TOOLS/test_image_codecs.py
python TOOLS/verify_images.py
```

Une banque, avec PNG individuels en plus des planches :

```powershell
python TOOLS/extract_anr.py --banks RBTA --individual --output EXTRACTED/single_bank
```

Contrôle x86 facultatif (Unicorn 2.1.4 a été installé localement sous ANALYSIS) :

```powershell
python -m pip install --target ANALYSIS/verification_deps unicorn==2.1.4
python TOOLS/verify_x86_images.py
```

La galerie peut aussi être servie localement :

```powershell
python -m http.server 8762 --bind 127.0.0.1 --directory EXTRACTED
```

## Suite du port

Le décodage des planches n'est plus un prérequis bloquant. Les prochaines tâches
sont l'association des frames aux mouvements (AIP/MVS/STS/CL2), la temporalité et
les collisions, puis les fonds animés et l'interface. Les boucles proposées par la
galerie sont une navigation visuelle dans les frames ; elles ne prétendent pas
reproduire les séquences de combat ni leur cadence originale.
