# 04 — Journal de développement

> Entrées chronologiques. Chaque session de travail ajoute une entrée : ce qui a été fait, découvert, décidé, et les prochaines étapes.
> Les hypothèses des premières sessions sont conservées pour l'historique ; les documents
> 05, 09 et 10 décrivent l'état technique actuel.

## Session 19 — 2026-09-23 : ISO de données pour le lecteur virtuel

- `TOOLS/cue_to_iso.py` extrait sans montage la piste MODE1 du CUE/BIN du CD 1
  Director’s Cut. Sortie privée `LOCAL/directors-cut-cd1-data.iso`, 358 246 400 o,
  SHA-256 `621b90e46bffb387a15be92cdf971fe2ee0f9085a57766718509a482604c6f42`.
  Le volume ISO9660 est `RISE2_DC_D1`. Cette ISO ne comporte pas les pistes CDDA.
- Réimport de l’ISO avec `--import-only` : 1 128 fichiers sur le volume, les 1 122
  fichiers du jeu sont identiques, SHA-256 par SHA-256, à l’import CUE/BIN.
  Les 30 robots et 105 ANI sont détectés ; la musique numérique est disponible.
- L’import d’un lecteur CD virtuel contenant seulement les données tolère une
  TOC audio indisponible et le signale. Tests synthétiques dédiés réussis.
  L’essai du lecteur virtuel monté par l’utilisateur reste à effectuer.

## Session 18 — 2026-09-23 : imports privés, Director’s Cut et dépôt local

- Dépôt Git initialisé sur `main`, sans remote. Liste de fichiers autorisés dans
  `.gitignore`, audit de l’index, README d’installation et CI sans données du jeu.
  L’index contient 68 fichiers texte de code/tests/documentation ; aucun original,
  asset converti, exécutable, projet Ghidra ou export de décompilation.
- Importateur CLI et fenêtre `IMPORTER.cmd` : dossier, ZIP, ISO9660, BIN/CUE,
  lecteur CD Windows et musique séparée (dossier/ZIP, pistes 02–10). Profils privés,
  empreintes SHA-256, pas d’écrasement d’un profil existant, résultat disponible
  après réussite des conversions. Deux éditions différentes ne sont pas fusionnées.
- Dossier DOS + neuf MP3 convertis ; ZIP et ISO fabriqués depuis cette source
  comparés sur les 996 fichiers (SHA-256 identiques). Sans pistes CD, le mode
  numérique est sélectionné ; MGA–MGF MRS/MRW sont bien la musique séquencée du jeu,
  confirmée par le manuel et `FUN_150b4`. Lecture musicale du port encore à intégrer.
- Director’s Cut CD 1 seul importé et converti : 1 122 fichiers de jeu, 30 robots,
  105 ANI conservés, 71 MRW / 1 179 WAV, neuf pistes CDDA. Même contenu jeu/musique
  qu’avec les deux disques. CD 2 = bonus, non requis. Les 102 ANI supplémentaires
  attendent la prochaine phase de décodage. Cette édition sera notre référence
  pour la suite, conformément au choix utilisateur.
- Atlas et sélection adaptés aux 30 portraits/robots ; RBT2 = SHEEPMAN,
  RBT3 = BUNNYRABBIT, palette propre suffisante, sans RB4/AG correspondants.
  Galerie à nombre de robots dynamique. Le raccourci historique n’a pas changé de profil.
- Loader LE corrigé : dernière entrée de table de fixups, source signée aux
  frontières, pages relatives aux objets, sélecteurs sans offset cible. Ancienne
  copie : 10 589 records / entrée 0x3EC2C ; Director’s Cut : 10 634 / 0x3F19C.
  Deux projets Ghidra isolés créés dans les profils (537 et 430 fonctions
  auto-détectées). Le projet historique et ses sorties restent intacts.
- Vérifications : **22 tests d’import/LE + 9 tests de codecs**, build Release et
  CTest `fighter_frames` réussis. Le test SDL caché `rotr2_asset_smoke` charge
  logos, titre, portraits et banques de tous les robots dans les profils 28 et 30.
  Export de l’index vers un checkout sans données, téléchargement des dépendances
  officielles avec SHA-256, compilation et mêmes tests réussis dans ce checkout.
  Fenêtre d’import initialisée sans erreur ; comportement visuel complet à tester.
- Limites avant publication : **CD physique non testé sur matériel** (aucun
  lecteur présent), choix de licence du code à finaliser. Aucun upload GitHub.
  Détails, commandes et rapports privés : document 11.

## Session 17 — 2026-09-23 : ANI présents, provenance de la copie et accueil du port

- `TOOLS/extract_ani.py` reproduit les lecteurs DOS VGA/haute résolution et
  exporte `LLOGO` (39 images), `END` (61) et `ENL` (61) vers `EXTRACTED/video/`.
  Les trois flux sont consommés exactement et les images contrôlées visuellement.
- `LLOGO` montre des mentions légales et logos, pas la cinématique complète.
  `SRC/ISO_version_install/ENERGY.NFO` précise que toutes les cinématiques ont
  été retirées de cette distribution. Les deux copies du jeu ne contiennent
  que les mêmes trois ANI. `RQLINK.ANI` manque ; son rôle exact dans le parcours
  ne se déduit pas de la seule erreur DOSBox.
- Le port charge ces logos, `MAINSCR.GGF` pour le titre, `VSFACE` pour les
  portraits et les banques RBT/MVS/CL2 selon les deux robots choisis. La cadence
  vidéo, la police et l'écran de choix restent provisoires ; comparaison au
  jeu DOS à faire. Une source non-rippée est nécessaire pour les films absents.

## Session 16 — 2026-09-23 : audit du port et remise à jour des formats

- Correction du lien image MVS → sprite : les 28 atlas `RBT?` ont une entrée vide en tête ;
  `image i` utilise `atlas.frames[i+1]`, tandis que les boîtes restent `CL2.records[i]`.
- Le marqueur `end` des séquences MVS ne traverse plus la boucle de rendu ; les
  mouvements terminés restent sur la dernière image visible. Le log des transitions
  indique maintenant l'ancien mouvement et la cible.
- `extract_mvs.py` lit les quatre octets `+1C..+1F` séparément, y compris dans
  `RBMG`/`RBMN`, et borne chaque table de déplacements à sa séquence propre.
  Les 30 JSON ont été régénérés.
- Build Release et test `fighter_frames` passés ; vérification des 8 640
  séquences MVS et de l'alignement atlas/CL2 pour les 28 robots.
- Documents 02, 05, 08, 09, 10 et README alignés sur MVS/mouvements, MRW/audio,
  MRS/séquences et SOUND.DAT/cartes son. Le doublon des sessions 1 à 14 du
  journal a été retiré après comparaison exacte de sa partie commune.
- À comparer au jeu DOS : cadence et transitions automatiques exactes des MVS,
  échelle et placement visuel des CL2, déclenchement/pitch MRS.

---

## Session 14bis — 2026-09-22 : bascule en haute résolution (640x400)

- Décision utilisateur : le port part directement sur la résolution native haute (640x400,
  banques RBT), pas sur 320x200. L'ossature est passée : fenêtre 1280x800 (x2), banque RBT0,
  ancre au sol ~y=312 (le robot s'étend ~176-312 px d'après les boîtes CL2 x2). La basse
  résolution (RB4) reste disponible pour l'option « VGA mode » de l'original.


---

## Session 14 — 2026-09-22 : L'OSSATURE DU PORT TOURNE — premier robot animé

### Résultat

- **Chaîne d'outils** : MSVC 14.44 (VS2022 Community) + CMake VS + SDL2 2.30.11 + SDL2_image
  2.8.4 + nlohmann/json v3.11.3 (téléchargés dans PORT/thirdparty).
- **Ossature compilée et exécutée** : `PORT/` (CMakeLists, src/main.cpp, src/assets.*, src/fighter.*)
  — fenêtre 640x400 (logique 320x200 x2), boucle 60 Hz, chargeur d'atlas (manifest + pages RGBA),
  chargeur MVS JSON, avancement de séquence (une entrée = une frame), application des déplacements
  (x2, signé par l'orientation), clavier provisoire (flèches + J/K) -> masques -> transitions MVS.
- **VALIDÉ À L'ÉCRAN par l'utilisateur** : le robot A (banque RB4A) s'anime en idle
  (images 0,0,1,1,2,2,3,3,2,2,1,1 -> frames 1-3 de l'atlas, boucle respirante). Capture :
  `documentation/captures/port_ossature_v1.png`.
- Architecture : `Assets` (atlas par banque + MVS par robot), `Fighter` (move_id = état,
  seq_pos, speed_level, step(inputs) = transitions + boucle de séquence).

### Pivots validés

- L'"image" des séquences MVS = l'index de frame dans le manifest de la banque (rect atlas +
  origin canvas) ✓ ; le rendu place la bbox de la frame avec un delta relatif à la frame 1
  (référence debout) — placement à affiner (delta exact via fn_1c819).

### Prochaines étapes du port

- 2e combattant (miroir) + câblage clavier complet ;
- hitboxes CL2 + application des dégâts (formule fn_38b72) + HUD (barres, timer) ;
- sons (WAV extraits) ; fonds (GGF AG + banques ANR décor) ;
- IA (35a7c/35eb8) puis menus/flow.


---

## Session 13 — 2026-09-22 : MVS 30/30 convertis (2 880 mouvements) — le graphe d'entrées est datable

### Résultat

- **TOUS les MVS convertis** (`TOOLS/extract_mvs.py` -> `EXTRACTED/data/mvs/*.json`) : 30 banques
  (28 robots + RBMG/RBMN), **2 880 mouvements (96 par banque)**. Découverte au passage :
  **RBMG/RBMN sont stockés en big-endian** (magic `01 "SVM"` inversé, répertoire/descripteurs/séquences
  en >I/>h) — support ajouté au convertisseur.
- **Terminateur des transitions vérifié : FFFF** (premier mot négatif) ; table vide = FF FF immédiat.
  Déplacements : int16 par pas, longueur = celle de la séquence (profils -4,-3,-2,-2,-1,-1... et
  ±3 constants = marche).
- Exemple (RBT0 idle) : images 0,0,1,1,2,2,3,3,2,2,1,1 ; transitions (16->7, 20->7, 18->7,
  **4->2 (accroupi)**, 2->3, 1->8, 32->9).
- **Masques d'entrées** : 6 bits (1, 2, 4, 8, 16, 32) + combinaisons (10, 12, 20, 18, 5, 36) ;
  2537/2880 mouvements sans transitions (états intermédiaires pilotés par le programme).
  OPTIONS.TXT confirme 10 entrées (4 directions + 3 punch + 3 kick) -> la nommation exacte des
  bits se fera en croisant RISE2.CFG (scancodes) et le test.
- MRS : chaîne de lecture lue (3be83 : 2 compteurs + 3 tables + pointeurs de séquences ; 3a792 :
  pitch 0x8000, volume/pan, SOS) — raffinement du pitch à finaliser (le SFX fonctionne déjà).

### Reste des données

- CHRSET (polices) + ANI (vidéos) ; pitch MRS ; nommage des bits d'entrées (croisement RISE2.CFG).


---

## Session 12bis — 2026-09-22 : pistes CD renommées

- Renommage des MP3 du rip : `Piste 02.mp3` ... `Piste 10.mp3` (= pistes du CD ; piste 01 =
  données). Couche CD identifiée dans l'EXR : wrappers MSCDEX INT 2Fh (`433e6/43410/43445/43564`)
  appelés par `40718/40b7b/40d58` (internes HMI SOS) ; la sélection de piste passe par le
  système SOS (config SOUND.DAT).
- Hypothèse d'usage : 02 = titre/menu ; 03-10 = combat (9 étapes ?) — à confirmer en jouant.


---

## Session 12 — 2026-09-22 : audio validé à l'oreille + musique CD-Audio rippée fournie

- L'utilisateur confirme : **les échantillons MRW (1 142 PCM) sont corrects**. Le format
  audio SFX est donc acté (reste le facteur de pitch des séquences MRS).
- Nouveau dossier `SRC/Audio_CD_RIP/` : **9 MP3 = pistes audio du CD (02-10, ~44 Mo)** — la
  musique du jeu était en CDDA (piste 01 = données). Pour le port : lecture MP3/OGG directe ;
  la correspondance piste -> scène à relever dans le code (MSCDEX) si besoin.
- Docs mises à jour : 09 (section audio FINAL), mémoire.


---

## Session 11 — 2026-09-22 : audio localisé — les MRW = banques PCM (Astra), 1 142 échantillons extraits

### Corrections Astra (vérifiées)

- **Les `.MRW` = banques d'échantillons audio** (PAS des fonds vidéo). Les `.MRS` = les séquences
  qui les pilotent. Les « fonds animés » = banques ANR décor + CTL.
- **SOUND.DAT = configuration des cartes son** (codes 0xE0xx = identifiants de pilotes), lue à
  0x272D5-0x2732E -> DAT_70428 ; FUN_2cbc1 (identifiant 12 o) -> FUN_3a3e8 (init SOS/HMIDRV.386).
- Chaîne audio : FUN_3be83 (.MRW -> .MRS temporaire pour les séquences, retour .MRW) ->
  FUN_3a6b0 (banque -> DAT_705bc[banque]) -> FUN_3a792 (sélection) -> FUN_417c9 (démarre l'échantillon
  SOS, appel @0x3A893).
- Format MRW : [u16 count][count x {u32 offset, u32 taille}][PCM 8 bits non signé mono].
  Base 11025 Hz, fréquence = (pitch_16_16 x 11025) >> 16. 1 142 entrées / 69 banques, bornes valides.
- **Vérifications GLM** : couverture empirique — (offset, taille) recouvre 99,8 % de chaque banque
  (l'autre interprétation laisse 40 Ko non référencés sur BG0) ; R0 entrée 0 = offset 0x9A / 2 769 o
  = signal réel (plage 0-255, onde). « Pixels 0x80 » = silence PCM non signé.
- Conversion : TOOLS/extract_mrw_audio.py -> EXTRACTED/audio/mrw/ (1 142 WAV + manifests).
- **Écoute de validation en cours avec l'utilisateur** (échantillons représentatifs proposés) avant
  d'acter définitivement ; l'extraction est réversible.


---

## Session 10 — 2026-09-22 : correction majeure — les MVS = tables de mouvements (Astra)

### Découverte

- **Les `.MVS` ne sont PAS de l'audio** : ce sont les tables de MOUVEMENTS par robot
  (96 descripteurs de 32 o). L'export WAV « bruit » venait de l'interprétation de données de
  mouvements comme des échantillons. Attribution corrigée dans doc 09.
- **Descripteur 32 o** : +0/+4/+8 = 3 séquences d'animation (selon vitesse) ; +C/+10/+14 = 3
  déplacements horizontaux (int16/pas) ; +18 = table de transitions (masque_commandes ->
  mouvement_cible, fin = premier mot négatif) ; +1C drapeaux, +1D mouvement auto, +1E index de
  reprise, +1F paramètre. Séquences : 2 o/entrée, image = 2*b0+(b1&1), 0xFF = fin.
- **Vérification indépendante sur les données (RBT0 @0x190)** : séquence @0xD90 = images
  0,0,1,1,2,2,3,3,2,2,1,1 + FF (idle respirant) ; transitions @0xDAA = 0x10->7, 0x14->7, 0x12->7,
  **0x04->2 (accroupi)** ; déplacements 0 (debout). Correspondance fichier->mémoire :
  descripteur mémoire = B + offset - 0x0C (chargeur FUN_23e97) — cohérent avec le relogage -0xC.
- **Impact** : c'est le pivot du moteur — chaque mouvement = animation (3 vitesses) +
  déplacement + transitions d'entrées. Les modulations IA (x125 %/x80 %) écrivent ces
  descripteurs en mémoire. Le graphe d'états complet devient datable directement des fichiers.
- **Nouvelle question ouverte : où sont les échantillons audio ?** SOUND.DAT mappe des
  événements vers des indices mais aucun fichier d'échantillons identifié (MRS ? banques ANR
  « effets » ? hors fichiers ?).

### À faire

- Convertir les 30 MVS en JSON (séquences/déplacements/transitions) — le convertisseur
  remplace extract_data.py::parse_mvs ;
- Réécrire le graphe d'états à partir des MVS (masques 0x10/0x14/0x12/0x04 = directions ?) ;
- Trancher l'emplacement audio.


---

## Session 9 — 2026-09-22 : test audio MVS négatif — question formulée pour Astra

### Résultat

- Export WAV empirique des zones de données de RBT0.MVS (11025 Hz, 8 bits) : **échec — bruit**
  (vérifié à l'écoute par l'utilisateur). Le MVS n'est donc PAS du PCM brut à ces offsets.
- Le codec est vraisemblablement le format son HMI **SOS** (les drivers HMIDET/HMIDRV/HMIMDRV.386
  du jeu = Sound Operating System) : entêtes des descripteurs de 32 o à décoder + débit +
  codage (PCM signé ? DPCM ? compression ?).
- **Question transmise à Astra** (voir bloc dans le bilan) pour identifier le codec MVS via le
  lecteur son dans l'EXR.

### État général

- Noyau de combat Phase 2 : entièrement cartographié (doc 08).
- Données restantes : MVS (codec), chunks MRW (deltas vidéo), CHRSET (polices), ANI (vidéos).


---

## Session 8bis — 2026-09-22 : périphériques du combat identifiés

- `FUN_253a8` = recul/mur (déplacement x, clamp aux bornes, transfert d'élan à l'adversaire au mur) ;
- `FUN_244eb` = projectiles (3 slots/joueur, scripts 6 o/frame : vx/vy/flags, bornes ±380 px,
  table de collision alternative +0x87a0) ;
- `FUN_22848` = effets/particules par joueur (5 slots de 12 o, deux tables de 8 pointeurs 0x62702/62722) ;
- `FUN_3a0d2` = défilement du décor (4 canaux de 14 o, dx/dy par frame, nourri par les CTL) ;
- complément de la chaîne de hit : fun_39fde/22fc9/3c595/3c28c = sons/réactions.

### État de la Phase 2 (carte du moteur)

Le noyau de combat est entièrement cartographié : frame update (2163a), orientation (25615),
dispatch d'états (234fa), physique (21baf), collisions CL2 (381a9/3885e), dégâts (38b72),
IA (35a7c/35eb8 + helpers), recul/projectiles/effets/décor (253a8/244eb/22848/3a0d2),
round (137ce), chargement match (12cc6 -> 14c04). Reste des données : MVS->WAV, chunks MRW,
CHRSET, ANI + détails de cadence à mesurer en implémentation.


---

## Session 8 — 2026-09-22 : graphe d'états + chaîne de dégâts

### Résultat

- **`FUN_25615` = orientation uniquement** : face selon les positions (bit 0 du flag), retournement
  seulement dans les états autorisés, paires 0x22<->0x23 / 0x4C<->0x4D / 0x1A<->0x21, transitions
  0x10->0x45 / défaut->0x44.
- **Table des états établie** (doc 08) : 0x00 idle, 0x02/03 accroupi, 0x0D/0E à terre, 0x10 relevé,
  0x12 saut, 0x14 hitstun, 0x19 dizzy, 0x1A/21 + 0x22/23 marches, 0x2A en l'air, 0x38/34 réactions,
  0x40-42 spéciaux, 0x44/45 retournement, 0x4A/4C/4D/4F multi-phases, 0x4B charge, 0x54 final,
  **0x58 = SUPER (jauge 24 pleine) + sans collision** ; `(état&0xF)<10` = interruptible.
- **`FUN_38b72` = application du hit** : jauge de super +2/coup (plafond 24) ; score +500/coup
  encaissé ; compteur de combos (<37 frames) ; **dégâts = (base_boîte(=octet4, signé) × multiplicateur
  × défense(victime 0..2 / 0x50 / 0x32)) >> 13**, ÷8 si attaquant étourdi, plancher 1 ;
  santé à struct+0x2C (0x6623A) ; KO -> FUN_3c28c, sinon FUN_3c595 ; hitstun DAT_6627a += 7059c ;
  recul DAT_6624e[attaquant] ; sons selon DAT_62b48[type].
- Correction de la carte : mot struct+0x02 = index d'animation (indexe les tables CL2 et 0x685cc) ;
  mot struct+0x00 = ID d'état ; santé = struct+0x2C.

### Reste des données

- Rôle exact de la boîte b2 (poussée ?) via fn_38b72/398a3 ; HUD/timer (253a8, 22848, 244eb, 3a0d2) ;
- MVS -> WAV ; chunks MRW ; CHRSET ; ANI.


---

## Session 7 — 2026-09-22 : données de collision CL2 complètes (format final)

### Résultat

- **Format CL2 verrouillé par l'assembleur de `fn_15d92`** : header 16 o (count u16 @12), records
  dès l'offset 16, avance = 3 + b0*5 + b1*6 + b2*5.
- **Sémantique des boîtes validée par le test `fn_3885e`** :
  - b0 (5 o) = boîtes d'ATTAQUE [x, y, w, h, dégât] — rares (frames de coup) ;
  - b1 (6 o) = boîtes de CORPS [x, y, w, h, p4, partie] — ~2/frame (tête + torse) ;
  - b2 (5 o) = boîte unique (poussée/empreinte, usage à confirmer) ;
  - échelle x×4, y×2 ; overlap X+Y ; hit le plus profond gagne ; état 0x58 sans collision ;
  - test mutuel par deltas opposés ; mode 0/1/2/3/4 (corps-corps, proj-joueur, proj-proj, décor).
- **`fn_381a9` = dispatcheur** : tables de pointeurs CL2 à +0 (p0) / +0x43d0 (p1) / +0x87a0
  (alternative, flag bit1) / +0x9850 (décors, 4 canaux CTL) ; indexées par l'INDEX D'ANIMATION
  (mot à +2 du struct joueur — correction : le dword 6620C = (anim<<16)|état).
- Conversion : `TOOLS/extract_cl2.py` -> `EXTRACTED/data/cl2/*.json` :
  **49/51 consommation exacte ; 28 CL2 robots (R0-RZ) = 9 204 frames, 1 119 boîtes d'attaque,
  11 170 boîtes de corps, 7 091 boîtes uniques** ; BGANIM*.CL2 = tables vides de décor ;
  BMANIMB = magic variante « 6LLC ».

### Reste des données (prochaines sessions)

- Graphe d'états complet (fn_25615/fn_234fa) + liaison états -> AIP -> animations ;
- application des dégâts (fn_38b72) et rôle exact de la boîte unique (b2) ;
- HUD/timer/projectiles (253a8, 22848, 244eb, 3a0d2) ; MVS -> WAV ; chunks MRW ; CHRSET ; ANI.


---

## Session 6 — 2026-09-22 : IA + physique + collisions décryptées

### IA (35a7c + 35eb8 + helpers)

- **Blocs IA** : `DAT_70518 + p*0x28` (file d'actions, index à +2), `DAT_704BA + p*0x28` (entrées
  courantes, sortie vers `DAT_66234` = input simulé), timer d'action `DAT_66243`.
- **Config joueur** : `FUN_22957` -> byte[0] type, byte[1] difficulté 0-3 (comportements > 1).
- **Décision 35eb8** : jauge `DAT_66268` (+4/max 140 pendant états 4/0x14, -1 sinon, seuil 0x50),
  distance |x0-x1|, **réaction à l'attaque adverse si < 161 px : roll RNG %100 vs seuil
  `DAT_62ae5 + personnalité*2` (par robot)** ; **modulation dynamique des records AIP** :
  durée (+8) = valeur(+6) ×125 %/111 % en approche, ×80 %/90 % en repli (selon tier) ;
  décision finale `FUN_36d20` sinon `FUN_36bb8` ; exécution `FUN_3785c` ; sélecteur `FUN_37db3`
  (roll %90 si difficulté > 1).
- États IA spéciaux : 0x0D/0x0E (étourdissement ?), 0x19 ; jauge 4/0x14 = charge.

### Physique (21baf)

- Compteur d'anim par état (`DAT_66285`) + table de durées `DAT_62742 + anim*2` ;
- freeze post-coup (`DAT_6627A`, -0x20) ; coût d'énergie par frame (`DAT_66299/6629b` -> clamp 0 +
  flag `DAT_66398`) ; avance = triplets de vecteurs dans la table de mouvement par animation
  (`0x685cc + anim*4`), vitesse via (66237&3)*3+3)/(0x66256>>16), index 0..2 ;
- verrou si record AIP octet+1 > 1 ; octet+5 -> 6623e ; octet+6 -> 6625a.

### Collisions CL2

- **CL2 = par ROBOT** (RB/RG/RS...= lettres, magic CLL5/CLL6 ; BGANIM*.CL2 = quasi vides) ;
- lecteur fn_15d92 : buffer partagé 0x65bd8 (+0x10f4 pour p2), count au 2e chunk (0x65bf8),
  records 3 o (a,b,c) + blocs 5/6 o, table de pointeurs construite à la volée ;
- données observées : tags ASCII ('F','f','b') + triplets de coordonnées = boîtes d'attaque par frame.

### Livrables

- Doc 08 augmentée (IA, physique, CL2).
- Décompilations : fn_35eb8, fn_3785c, fn_37db3, fn_36d20, fn_36bb8, fn_376c4, fn_22957, fn_3781a,
  fn_3717b, fn_251bc.


---

## Session 5 — 2026-09-22 : Phase 2 — carte du moteur établie

### Résultat

- Boucle de combat identifiée par remontée du graphe d'appels depuis le blitter :
  - `FUN_137ce` = gestionnaire de round (init, KO, bonus +1000 en victoire parfaite à 120 PV) ;
  - `FUN_2163a` = **mise à jour d'une frame de combat** (le cœur) ;
  - `FUN_234fa` = machine à états des combattants (entrées -> ID de coup, enchaînements, stun) ;
  - `FUN_21baf` = physique/collisions ; `FUN_25615` = étape de frame ;
  - `FUN_35a7c`/`FUN_35eb8` = IA (réactions + décision) ;
  - `FUN_1fa80` (4402 o, la plus grosse) = moteur de TEXTE, pas la logique de jeu.
- **Structure joueur cartographiée** (stride 0x97 = 151 o, p0 @0x6620E, p1 @0x662A5) :
  état courant, x/y, frame, flags (bit1 = KO), entrées u16 + lissage 4 pas, verrou, compteur
  freeze (seuil 0x167F), stun (init 100, -1/13 frames), pointeur records AIP (bit 0x20 = non
  interruptible), au sol y, variables IA.
- **Données de combat** : tables de pointeurs par joueur (0x685cc/685d4, relogées -0xC par fn_23e97),
  table des coups AIP (0x68524 + move*0x14), scripts d'enchaînements encodés (0x685dc + p*0x10,
  échappements -1/-2), 3 slots projectiles/joueur (0x70314, 0x14 o), clavier 0x65FE0 (bits 0-5/6-11),
  bornes d'arène 0x65fd6/65fda, portée de coup de près = 0x3D (61 px) -> ID +0x40, santé pleine = 120.
- `FUN_12cc6` = chargement du match (appelle le loader robot `fn_14c04`) ; `FUN_15403` = écran de
  sélection ; `FUN_1fa80` = rendu texte (table @0x6250F).

### Livrables

- `documentation/08_moteur.md` — carte du moteur (boucle, structure joueur, données).
- `ANALYSIS/exr_functions.txt`, `ANALYSIS/exr_callgraph.txt` (529 fonctions).
- Nouvelles décompilations : `ANALYSIS/funcs/fn_2163a.c`, `fn_137ce.c`, `fn_234fa.c`, `fn_2298b.c`,
  `fn_21baf.c`, `fn_25615.c`, `fn_165fb.c`, `fn_20fb9.c`, `fn_35a7c.c`, `fn_1fa80.c`, `fn_12cc6.c`,
  `fn_15403.c`, `fn_3c17c.c`, `fn_3c1f0.c`, `fn_3c1b6.c`, `fn_3a6b0.c`.

### Reste de la Phase 2

- `FUN_35eb8` (3111 o) : décision IA ; `FUN_21baf` (2845 o) : physique détaillée + CL2 ;
- `FUN_25615` (3340 o) : étape de frame ; HUD/projectiles (253a8, 22848, 244eb, 3a0d2, 381a9) ;
- liaison états -> animations AIP + cadence (la structure 14 o du record AIP est déjà le pivot).


---

## Session 4 — 2026-09-22 : formats de données restants (structures extraites)

### Fait

1. Reprise après la session 3 d'Astra (images validées). Lecture de `07_decodage_images.md` + état `EXTRACTED/` (237 Mo, galerie OK).
2. **Parsers écrits pour les structures restantes** (`TOOLS/extract_data.py` -> `EXTRACTED/data/*.json`) :
   - **AIP** (28 fichiers) : structure 100 % cohérente = [u32 N1][u32 N2][N1 x record 14 o][N2 dwords].
     Record = dword flags, 3 octets (b1,b2,b3 — vus 0,0,24 / 2,0,24 / 4,0,24), 3 x u16 (durée ~20-64,
     index d'animation croissant de 2, tier 1-4), 1 octet. RBT0 = 29 mouvements + 42 dwords d'entrées
     (motifs 0x80001, 0x530000, 0x3ff74... = séquences d'entrées/déclencheurs — sémantique à relier au moteur, Phase 2).
   - **MRW** (69) : [u16 count][count x (offset u32, taille u32)][données brutes de pixels].
     Chaine vérifiée : offset+taille des chunks se recouvrent jusqu'à la fin exacte du fichier
     (BGA : 130/963 -> 1093/1894 -> 2987/41344 = fin). La plupart des slots pointent le premier chunk.
     Données = pixels bruts lisses (début 0x80 = ciel), PAS du LZW. 40832 o (BG0/BG1) ne donne pas un
     W x H entier propre -> encodage par deltas/frames vidéo à confirmer via FUN_3c1b6/3c17c/3c1f0.
   - **MVS** (30) : magic `MVS` + 2 dwords (0x1c48, 0x1bfe) + répertoire d'offsets (0x190..0xD70,
     pas de 0x20 = 96 entrées) terminé par 0xFFFFFFFF ; entrées de 32 o contenant des pointeurs vers les
     données (0xd90, 0xdaa...). Le codage des échantillons (PCM ? DPCM ?) reste à établir via le lecteur
     son (drivers HMI/SOS : HMIDET/HMIDRV/HMIMDRV.386).
   - **SOUND.DAT** : mapping d'événements -> sons (triplets u16 avec codes 0xE0xx, priorités, -1) ; 262 mots.
   - **STS** (28) : tables brutes u16 (patterns 0xFF00/0xFF = états ?) — sémantique Phase 2.
   - **CTL** (23) : 4 dwords d'offsets (0x10/0x20/0x30/0x40) + sous-tables (0x7f/0xff = bornes d'animation) ;
     le lecteur `fn_15352` init 4 canaux (x/y offset + bornes) — scripts de défilement du décor.
   - **CL2** (52, magic `CLL5`) : lecteur `fn_15d92` = table de pointeurs construite sur des records de
     3 octets + blocs de 5/6 octets, offset +0x10f4 pour le joueur 2 -> **boîtes de collision par joueur**.
3. **RISE2.CFI/HISCORE** : `RISE2.CFG` = 46 x u16 : scancodes de touches (72=haut, 80=bas, 75=gauche,
   77=droite, 30/31/32, 44/45/46 = boutons) + options ; `HISCORE.DAT` = 12 records de 8 o
   (nom 3 lettres + score dword, défauts 500000 -> 100000).

### Reste (Phase 1/2)

- Codage des chunks MRW (deltas vidéo) et lectures CTL complètes — via FUN_3c1b6/3c17c/3c1f0 et fn_150b4.
- Échantillons MVS (codec SOS/HMI) -> WAV.
- Sémantique des champs AIP/STS/SOUND.DAT : à relier au code du moteur (Phase 2, ancres prêtes).
- ANI (vidéos), CHRSET (polices), autres banques ANR avec palettes contextuelles.


---

## Session 3 — 2026-09-22 : blocage levé, planches des 28 robots extraites

### Résultat

- GGF : **185 images lisibles** ; largeurs corrigées (400/800 au lieu de 320/640),
  plus A6K en 640×400. Les 3 petits A6B/A6G/A6M sont des fragments de palette,
  pas un sous-format d'image.
- ANL/ANR : **126 banques, 20 969 frames** parcourues avec des fins exactes.
- Export couleur : **9 232 frames RBT + 9 234 frames RB4 + 56 portraits**, soit
  **18 522 frames** réparties sur **139 planches**. Indices, alpha et positions conservés.
- Galerie `EXTRACTED/index.html` et aperçu `EXTRACTED/sprites/robots.png`.

### Corrections de rétro-ingénierie

- `FUN_35247` lit **RBT?.AIP**, pas ANR : le nom est présent à 0x62ad8.
- ANL contient un u16 puis des offsets u32 ; ANR contient des segments horizontaux
  compacts et un terminateur 0xffff par frame. Le blitter est `FUN_1c819`.
- Les GGF sont des pixels linéaires ; les sauts du blitter sont du recadrage,
  pas des plans VGA. Le bourrage final d'un octet zéro est normal.
- Palettes de combattants : 70 couleurs par joueur ; couleurs partagées pour
  les effets et quelques pixels dépendant de la palette adverse.
- A6K atteint des codes LZW 12 bits. Le chargeur original corrompt ses tableaux
  rapprochés ; son résultat devient identique à Python après relogement des tableaux
  dans l'émulateur. Ce diagnostic est distingué des tests sans modification.

### Vérification et reprise

9 tests de régression passent. Tous les atlas et PNG ont été relus. Les instructions
x86 originales donnent les mêmes segments sur **456 frames dans les 126 banques**,
et les mêmes pixels sur A4A/AG0/VS. Détail, limites et commandes reproductibles dans
**[07_decodage_images.md](07_decodage_images.md)**, qui remplace les hypothèses des
sessions précédentes. La suite porte sur les scripts de mouvements et le moteur,
plus sur la géométrie des planches.

---

## Session 2bis — 2026-09-22 : LZW verrouillé, GGF décompressés — BLOCAGE : géométrie des images

> Entrée historique : ce blocage et plusieurs identifications ci-dessous sont
> corrigés par la session 3 et le document 07.

### Fait

1. **LZW du jeu entièrement verrouillé par lecture d'assembleur** (`ANALYSIS/funcs/asm_fn_3c731.txt`, `asm_fn_3c9d0.txt`, `asm_fn_3caac.txt`) :
   - bits LSB-first, largeur 9 bits, doublement de limite à 0x200/0x400/0x800, max 12 bits ;
   - 0x100 = CLEAR (réinit), **0x101 = EOI**, premier libre 0x102 ;
   - dictionnaire préfixe (code) / suffixe (octet), ajout après CHAQUE code (y compris le premier, avec prefix = état initial 0) ;
   - le code après CLEAR est lu sur 9 bits aussi ; KwKwK = code >= next_code ;
   - implémentation Python conforme : `TOOLS/lzw.py`.
2. **Découverte de la structure GGF via l'assembleur de l'init** : après le flag (1 octet), le jeu lit **768 octets de palette** (256 x RGB 6 bits) dans un buffer séparé (`1b39e(0x65c7c, 0x300)`), PUIS le flux LZW. Donc :
   **GGF = [u8 flag=1][palette 768 o][flux LZW]** — le flux décompressé = pixels indexés (1 o/pixel).
3. **185 GGF sur 188 décompressent proprement** (`TOOLS/extract_ggf.py`) : deux familles — petits (80 000 o décompressés : A40/A4A/B4Z…) et grands (320 000 o : AG0/VS/RVCONT…). 3 fichiers trop petits pour la palette (A6B 495 o, A6G 495 o, A6M 988 o) = sous-format distinct, à traiter à part.
4. Palettes extraites -> `EXTRACTED/_palettes/*.pal` (RGB 8 bits expansées depuis le 6 bits VGA).
5. Rendus PNG de candidats de géométrie -> `EXTRACTED/ggf/_candidats/` (à valider visuellement).

### Blocage précis : la géométrie (largeur x hauteur) des pixels décompressés

- Les flux décompressés font **80 000 o** (petits) ou **320 000 o** (grands) de données d'images indexées cohérentes.
- Le blit du jeu (`fn_3c731`) a une mise en page **dépendante du mode vidéo** :
  - un mode écrit **tout** le flux (jusqu'à 320 000 o consécutifs) ;
  - un autre mode **entrelace** : sauter 80 o, écrire 640, sauter 160, écrire 640… (période source 800 o) ;
  - un troisième (VGA bit 3) : sauter 40, écrire 320, puis cycles (160, 640).
- Rendu linéaire 320x250 = **images brouillées** (confirmé visuellement par l'utilisateur sur A4A.png).
- L'autocorrélation (continuité horizontale) est **inconclusive** : candidats petits = 800x100, 400x200, 640x125 ; grands = 800x400, 640x500, 640x400.
- **Résolution prévue** : capturer l'écran réel sous DOSBox (Ctrl+F5 -> PNG dans `ANALYSIS/dosbox_captures/`, dossier configuré dans la conf) puis **appareiller les pixels capturés aux flux décompressés** -> déduction de la géométrie exacte. À ce stade :
  - le chemin « DEMARRAGE » du menu n'atteint pas les écrans de chargement robot : il échoue sur `RQLINK.ANI` manquant **avant** tout affichage de GGF ;
  - l'écran de menu qui s'affiche utilise a priori d'autres assets (MRW ?) ;
  - la capture Ctrl+F5 n'a pas produit de fichier lors de l'essai (livraison de touche/focus DOSBox à déboguer).

### Prochaines étapes (reprise)

1. **Vérification visuelle des candidats** (`EXTRACTED/ggf/_candidats/`) : demander à l'utilisateur quelle mise en page est correcte (800x100 / 400x200 / 640x125 pour les petits ; 800x400 / 640x500 / 640x400 pour les grands) — 10 secondes de son temps remplacent toute l'analyse.
2. Faire fonctionner la capture DOSBox (Ctrl+F5 avec focus) OU utiliser une capture fournie par l'utilisateur comme étalon.
3. Corriger `TOOLS/extract_ggf.py` avec la géométrie validée -> ré-extraire les 185 PNG.
4. Cracker les 3 petits GGF (sous-format), puis ANR/ANL (records de `fn_35247`), MRW (chunks), CL2 (`CLL5`), CTL, AIP/STS, MVS (sons), ANI.
5. Parser `RISE2.CFG` (touches/options, 88 o) et `HISCORE.DAT`.


---

## Session 2 — 2026-09-22 (suite) : Phase 1 — catalogue des formats + RE des loaders

### Fait

1. **Catalogue complet** des 1 047 fichiers (`TOOLS/catalog.py` → `ANALYSIS/catalog.md`/`.csv`) : structure de nommage (28 slots robots base36), tailles/entropies par type.
2. **Crack du loader LE** : format entièrement décodé (header Watcom `e32_exe`, table d'objets 24 o, page map compacte 4 o/entrée, fixups par page via `fpagetab` cumulatif, records 9/10 o [stype|flags|soff16|tobj8|toff32], stype 7 = linéaire additif base+toff — vérifié sur 10 410 records) → `TOOLS/le2flat.py` produit l'image plate 406 Ko.
3. **RISE2.EXR importé dans Ghidra** (`TOOLS/import_exr_ghidra.py`, projet `ANALYSIS/exr_proj`) : 529 fonctions, entry CRT @0x3EC2C décompilée.
4. **Loaders de formats localisés et décompilés** : séquence robot `FUN_14c04`, LZW+blit `FUN_3c731/3c9d0/3caac`, parseur ANR/ANL `FUN_35247`, parseur MVS/STS `FUN_23e97`, loaders AN/PAL `FUN_1524f/1a0fc/1a473`, primitives E/S `1b304/1b39e/1b3e6/121c3` → `ANALYSIS/funcs/`.
5. **Décrypté la sémantique LZW exacte du jeu** (9→12 bits croissants, CLEAR=0x100, EOI=0x101, premier libre 0x102, LSB-first, dico préfixe/suffixe) → implémentée dans `TOOLS/lzw.py` (à valider sur la vraie zone compressée des GGF).
6. Constats formats : ANR non compressé (0xFFFF + records), MVS = magic + table (offset,taille), CL2 = magic `CLL5`, PAL = palettes 6 bits brutes, MRW = table de chunks (taille, offset), GGF = palette brute variable + données (LZW à valider).

### Pièges rencontrés

- fpagetab = offsets cumulatifs (début de segment), pas des compteurs — pages de l'objet pile (59–80) sans fixups.
- stype 7 = linéaire (addend = offset objet) et PAS self-relatif ; tobj numéroté **à partir de 1**.
- Le motif `3f 00 00` des GGF n'est pas du LZW 9 bits (alignement impossible) → palette brute.
- Ghidra : `-postScript x.py` sans mode PyGhidra ne marche pas ; utiliser l'API PyGhidra `open_program()` + `GhidraProject.openProgram("/", nom)`.

### Prochaines étapes

- Valider structure GGF (palette variable + zone LZW) et décompresser les 188 GGF.
- Extraire records ANR/ANL (non compressés), chunks MRW, CL2/CTL/AIP/STS, échantillons MVS.
- Extraction visuelle PNG (premier sprite robot attendu).

---

## Session 1 — 2026-09-22 : Phase 0 (mise en place + premiers constats)

### Fait

1. **Exploration des sources** : inventaire complet de `SRC/` (voir [01_overview.md](01_overview.md)). Les deux distributions (DOS crackée / ISO) sont identiques octet pour octet → référence canonique unique.
2. **Arborescence + documentation** créées.
3. **Chaîne d'outils validée** :
   - Ghidra headless fonctionne (import + analyse auto de `RISE2.EXE` dans le projet `ROTR2_Reverse`).
   - Pipeline **Python → Ghidra via PyGhidra** validé (`TOOLS/export_launcher.py`, `TOOLS/export_listing.py`) : décompilé + listing assembleur exportés vers `ANALYSIS/`.
   - DOSBox 0.74-3 opérationnel avec config projet (`ANALYSIS/dosbox_rotr2.conf`).
4. **Analyse du lanceur** `RISE2.EXE` (19 Ko, Watcom 16 bits) — voir [06_analyse_binaire.md](06_analyse_binaire.md).
5. **Test de démarrage sous DOSBox : RÉUSSI.** Le jeu boote, affiche le menu principal (captures dans `documentation/captures/`), lit `RISE2.CFG` (interface en français), rend en VESA.

5. **Validation complète par l'utilisateur** : lancé manuellement, le jeu atteint **le combat en cours** (LOADER vs DEADLIFT) — fond industriel animé pré-calculé, combattants pré-rendus, HUD (barres de vie, timer), menu pause français (CONTINUER MATCH / F9 CALIBRER JOYSTICKS / F10 QUITTER MATCH). Capture : `documentation/captures/dosbox_combat_loader_vs_deadlift.png`. Mon échec « DEMARRAGE » (Entrée envoyée aveuglément) a probablement sauté/déclenché la séquence d'intro ; le jeu se lance correctement quand on le pilote normalement.

### Découvertes

- Le vrai binaire est `RISE2/RISE2.EXR` (LE DOS/4GW 32 bits), pas `RISE2.EXE`.
- **`RQLINK.ANI` est absent des données fournies** — en tappant Entrée aveuglément sur le menu, le jeu quitte avec `Unable to open file: RQLINK.ANI`. Seuls `END.ANI`, `ENL.ANI`, `LLOGO.ANI` existent. Le démarrage normal par l'utilisateur fonctionne ; à clarifier en Phase 1 (l'intro est-elle skippable ? le fichier est-il sur l'ISO complète ?).
- Noms de robots visibles en jeu : **LOADER**, **DEADLIFT** (→ mapping futur vers les `RBT*.ANR`).
- L'EXR contient à l'offset fichier 0x52421 une chaîne `C:\ACCLAIM\RISE2` (16 octets) = chemin d'installation fallback, cible probable du « patching » du lanceur. Le jeu résout aussi ses données par recherche relative (`\RISE2`).
- Expérience dynamique : après une session complète (menu + tentative de partie), `RISE2.EXR` est **inchangé sur disque** (MD5 identique) → le « patching » du lanceur est in-memory et/ou idempotent ; mécanique exacte à confirmer en Phase 2 (non bloquant).
- Types de données repérés dans l'EXR : `MIRAGEGAME` (variable d'env), `RISE2.CFG`, `CHRSET3.DAT`, `TESTD.RAW`.

### Décisions

- Référence canonique de données : `SRC/DOS_version_install_cracked/` (jamais modifiée ; les tests se font sur la copie `ANALYSIS/dosbox_rotr2/`).
- Analyse Ghidra par scripts Python (PyGhidra), projets dédiés `ANALYSIS/pyghidra_proj` pour les expérimentations, projet GUI `ROTR2_Reverse` pour le travail partagé.

### Prochaines étapes

- Phase 1 : catalogueur de formats (GGF/ANR/ANL/MRW/CL2/PAL/CTL/DAT/STS/CFG) + extraction.
- Résoudre la question `RQLINK.ANI` (demander à l'utilisateur si l'ISO originale existe sous forme d'image ; sinon RE du format ANI et placeholder).
- Phase 2 : parser LE + import de `RISE2.EXR` dans Ghidra.
