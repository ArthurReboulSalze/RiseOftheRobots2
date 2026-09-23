# 09 — Formats MRS/CHRSET restants et vidéos ANI extraites

> État au 2026-09-23. Les images (GGF/ANR/ANL) et les collisions (CL2) sont résolues (docs 07/08).
> Ce document suit les formats restants.
> Les adresses concernent l’ancienne copie DOS. La Director’s Cut possède un
> autre EXR ; garder les sources et analyses des deux éditions séparées.

## MVS — TABLES DE MOUVEMENTS (corrigé par Astra + vérifié sur les données — PAS de l'audio)

**Les `.MVS` décrivent les mouvements/animations de chaque robot (96 descripteurs par fichier),
pas des échantillons audio.** L'hypothèse audio initiale était fausse : les zones « audio »
proposées (0x330, 0xD74) sont à l'intérieur des descripteurs ; le bruit WAV venait de
l'interprétation de données de mouvements comme des échantillons.

### Structure fichier

```
[MV S 01][u32 0x1C48][u32 0x1BFE][répertoire : 96 u32 à partir de 0x0C (offsets de descripteurs)]
[descripteurs de 32 o dans [0x190, 0xD90)]
[données : séquences d'animation, déplacements, tables de transitions]
```

Correspondance fichier -> mémoire (chargeur `FUN_23e97`) :
`descripteur mémoire = B + offset_répertoire - 0x0C` ; `pointeur relogé = B + valeur_fichier - 0x0C`
(B = buffer chargé depuis l'octet 0x0C du fichier).

### Descripteur de 32 o (offsets relatifs, sémantique vérifiée dans le moteur)

| Offset | Signification |
|---|---|
| +00, +04, +08 | Trois pointeurs vers des séquences d'animation (sélection selon la vitesse) |
| +0C, +10, +14 | Trois pointeurs vers des déplacements horizontaux : int16 par pas (LE dans `RBT?`, BE dans `RBMG`/`RBMN`) ; 0 = aucun |
| +18 | Pointeur vers la table de transitions : couples (u16 masque_commandes, u16 mouvement_cible), fin = premier mot négatif |
| +1C | Octet de drapeaux (fins, boucles, transitions automatiques) |
| +1D | Mouvement cible pour certaines transitions automatiques |
| +1E | Index de reprise dans la séquence |
| +1F | Paramètre selon drapeaux : limite de répétition ou impulsion verticale signée |

Séquences d'animation : **2 octets par entrée** — `image = 2*octet0 + (octet1 & 1)` ;
les autres bits du second octet = champ de contrôle ; `octet0 == 0xFF` = fin.
Le marqueur de fin ne représente pas une image. Pour les 28 atlas `RBT?`, l'entrée 0 du
manifest est vide : **image MVS `i` → atlas `frames[i+1]`**, tandis que les boîtes CL2
utilisent **`records[i]`**. Les 28 atlas ont exactement une entrée de plus que leur CL2.
Dans les variantes `RBMG`/`RBMN`, les sept pointeurs sont big-endian, mais les quatre
champs `+1C..+1F` restent quatre octets distincts ; le JSON `flags` contient seulement `+1C`.

Vérification indépendante (RBT0, descripteur @0x190) : séquence @0xD90 = images
0,0,1,1,2,2,3,3,2,2,1,1 puis `FF` (idle respirant) ; transitions @0xDAA =
(masque 0x10->mvt 7, 0x14->7, 0x12->7, **0x04->2 = accroupi**) ; déplacements = 0 (debout).
Le moteur : `FUN_21baf` applique les déplacements (facteur ±2 selon l'orientation) ;
à 0x217A4 le descripteur+0x18 est chargé puis `FUN_234fa` choisit le mouvement suivant.

**Usage moteur** : `FUN_21baf` résout le descripteur du joueur dans la banque pointée par
`DAT_685cc[p]`, avec l'index d'animation de la structure joueur à `DAT_6620c+p*0x97`.
Les modulations IA (×125 %/80 %) écrivent dans les données de mouvement en mémoire.

## MRW — BANQUES DE SONS PCM (corrigé par Astra + couverture empirique 99,8 % — PAS des fonds vidéo)

**Les `.MRW` contiennent les échantillons audio du jeu** (les « pixels 0x80 » = silence PCM
non signé ; les entrées « identiques » = échantillons partagés). Les fonds animés du jeu =
banques ANR « décor » + scripts CTL (déjà décodés).

```
[u16 count][count x { u32 offset_absolu_dans_le_fichier ; u32 taille }][PCM 8 bits non signé, mono]
```

- **1 142 entrées sur les 69 banques, bornes toutes valides** (vérification Astra + couverture :
  l'interprétation (offset, taille) recouvre ~99,8 % de chaque fichier ; (taille, offset) laisse
  des trous de 40 Ko) ;
- config lecteur (`FUN_3a792` -> `FUN_417c9` SOS, appel @0x3A893) : **8 bits, mono, 11 025 Hz de
  base**, `fréquence = (facteur_pitch_16_16 × 11025) >> 16` ; données sans décompression ;
- exemple (Astra, vérifié) : R0.MRW entrée 0 = offset 0x9A, 2 769 octets.
- Conversion : `TOOLS/extract_mrw_audio.py` -> `EXTRACTED/audio/mrw/<banque>/<banque>_NN.wav`
  (+ manifest.json par banque, resume.json). Banques : R0-RZ (28 robots), BG0-BGZ
  (ambiances), MGA-MGF (musique numérique), EN/EX/FR (globales). Certains échantillons
  partagent les mêmes données. La Director’s Cut ajoute R2/R3 : **71 MRW, 1 179 entrées**.

## MRS — séquences qui pilotent les échantillons (68 fichiers, 48-21 156 o)

`FUN_3be83` : charge le `.MRW` en changeant temporairement l'extension en `.MRS` (charge les
séquences), puis rétablit `.MRW` ; `FUN_3a6b0` charge la banque dans `DAT_705bc[banque]`.
À parser : les séquences (déclenchement, facteur de pitch par échantillon).

## SOUND.DAT — configuration des cartes son (PAS une table d'événements)

Lu à 0x272D5-0x2732E (524 o -> `DAT_70428`). `FUN_2cbc1` récupère l'identifiant dans un
enregistrement de 12 o -> `DAT_703d8`, et `FUN_3a3e8` initialise SOS avec `HMIDRV.386`.
Les codes `0xE0xx` = identifiants de pilotes. (Correction de l'attribution précédente
« mapping d'événements -> sons ».)

## CHRSET1-3.DAT — polices (3456 o chacun)

3456 = 27×128 = 24×144 = ... à déterminer avec le moteur de texte (`FUN_1fa80`, table 0x6250F).

## ANI — trois séquences décodées, autres films conservés

Le lecteur DOS (`FUN_351e6` → `FUN_34bee` → `FUN_323d4`/`FUN_32474` en VGA,
`FUN_347bf` → `FUN_32171`/`FUN_322a2` en haute résolution) a permis de décoder
**les trois ANI de l’ancienne copie**. `TOOLS/extract_ani.py` exporte les PNG et manifests dans
`EXTRACTED/video/` : `LLOGO` = 39 images 320×200 (logos et mentions légales),
`END` = 61 images 640×400 et `ENL` = 61 images 320×200 (versions de la fin).
Total : **161 images**, avec consommation exacte des trois fichiers source.

Format : `u16 nombre_images`, première image en RLE par ligne (nombre de paquets
sur un octet, puis longueur signée : négative = octets littéraux ; positive =
répétition d'un indice), alignement sur 2 octets, palette VGA de 768 octets
(6 bits/canal). Chaque image suivante contient un nombre de lignes modifiées,
des sauts de lignes et des paquets de deltas ; un paquet saute des pixels puis
écrit des paires d'indices, littérales ou répétées. Les images se reconstruisent
en conservant la précédente. Les zones utiles occupent 199 lignes pour `LLOGO`,
200 lignes centrées pour `END` et 100 lignes centrées pour `ENL`.

**Les cinématiques longues manquent dans l’ancienne distribution.** `SRC/ISO_version_install/ENERGY.NFO`
dit explicitement : « THE CINEMATIC IS ALL RIPPED. DISABLE IT IN THE SETUP! ».
Les anciennes copies ISO extraite et DOS ne contiennent que ces trois ANI.
L'erreur `RQLINK.ANI` observée
sous DOSBox prouve qu'un ANI demandé par un chemin du jeu est absent ; elle ne
prouve pas que ce fichier était à lui seul le film d'introduction. Le lecteur
DOS règle séparément l'attente de chaque image : **39 images ne signifient pas
39/15 secondes**, et la durée exacte des logos reste à mesurer.

**Mise à jour Director’s Cut** : le CD 1 ajouté ensuite contient **105 ANI**, dont
`RQLINK.ANI`. L’import conserve les 105 fichiers et convertit actuellement les
trois noms connus ; les **102 autres** doivent encore être caractérisés et intégrés
au parcours vidéo. Le jeu entier n’a donc pas seulement 161 images animées.
Cette édition, avec ses 30 robots, est retenue pour la suite des extractions.
Le CD 2 contient des bonus FLC/GIF/WAV et n’est pas nécessaire au jeu.

## Audio — état confirmé (2026-09-23)

- **SFX : validé à l'oreille** par l'utilisateur — les 1 142 échantillons PCM extraits des 69
  banques MRW sont corrects (`EXTRACTED/audio/mrw/`). Reste à intégrer le facteur de pitch des
  séquences MRS (freq = (pitch_16_16 × 11025) >> 16) pour la restitution exacte.
- **Musique : CD-Audio rippé** par l'utilisateur dans `SRC/Audio_CD_RIP/` :
  9 MP3 renommés `Piste 02.mp3` ... `Piste 10.mp3` (numérotation = pistes du CD ; piste 01 =
  données ISO). Le jeu jouait la musique en CDDA (couche HMI SOS : wrappers MSCDEX INT 2Fh en
  `433e6/43410/43445/43564`, appelés par `40718/40b7b/40d58` ; la sélection de piste passe par
  le système SOS configuré par SOUND.DAT).
  Hypothèse d'usage (à confirmer en jouant) : piste 02 = titre/menu, pistes 03-10 = musique
  de combat (9 pistes = 9 étapes de tournoi ?). Pour le port : lecture MP3/OGG directe.

### Musique sans CD audio

Le manuel `README.FRA` cite la musique numérique et la musique CD ; `OPTIONS.TXT`
propose `DIGITAL MUSIC`, `SOUND EFFECTS`, `CD STREAMED MUSIC`. Dans `FUN_150b4`,
`DAT_6639c` sélectionne 0 = ambiance, 1 = numérique, 2 = CD. La chaîne à 0x50863
commence par `BGA.MRW` ; le mode numérique remplace B par M et utilise
`A + (arène % 6)` : **MGA à MGF**. Chaque MRS pilote les échantillons du MRW associé
via `FUN_3c17c` → `FUN_3be83`. Aucun fichier MID/HMI/HMP/XMI n’a été trouvé dans
les deux disques inspectés : il s’agit de musique séquencée à base d’échantillons.

L’importeur propose `auto`, `cd`, `digital`, `effects`, `off` et garde les banques.
**La lecture musicale du port reste à intégrer**, y compris le séquenceur MRS.
La disponibilité de cette alternative est confirmée ; sa restitution complète
et sa qualité d’écoute dans le port ne le sont pas encore.

## Prochaines étapes

- [x] MVS convertis en JSON : 30 banques, **2 880 mouvements** (96/banque ; pointeurs et
  déplacements big-endian dans `RBMG`/`RBMN`) — `EXTRACTED/data/mvs/`. Transitions (masque -> mouvement) intégrales : le graphe
  d'entrées de chaque robot est datable. 10 entrées physiques (OPTIONS.TXT) vs 6 bits de masque
  -> nommage des bits via RISE2.CFG + test.
- [ ] MRS : pitch/déclenchement exacts (chaîne FUN_3be83/3a6b0/3a792 décompilée ; pitch 0x8000).
- [ ] CHRSET (polices, via le moteur de texte FUN_1fa80, table 0x6250F).
- [x] LLOGO/END/ENL : codec et 161 images extraits dans les deux éditions.
- [ ] Étendre la conversion aux 102 autres ANI de la Director’s Cut et restituer
  leurs cadences/transitions dans le port, après finalisation de l’import.
